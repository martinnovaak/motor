#ifndef MOTOR_BOARD_HPP
#define MOTOR_BOARD_HPP

#include <sstream>
#include <tuple>
#include <vector>
#include <array>

#include "bits.hpp"
#include "types.hpp"
#include "attacks.hpp"
#include "chess_move.hpp"
#include "zobrist.hpp"
#include "fen_utilities.hpp"
#include "pinmask.hpp"

constexpr int castling_mask[64] = {
        13, 15, 15, 15, 12, 15, 15, 14,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        7, 15, 15, 15,  3, 15, 15, 11,
};

struct board_info {
    std::uint8_t castling_rights;
    Square enpassant;
    std::uint8_t fifty_move_clock;
    Piece captured_piece;
    chess_move move;
    zobrist hash_key;
    std::uint64_t orthogonal_pins = 0ull;
    std::uint64_t diagonal_pins = 0ull;
    std::uint64_t checkers = 0ull;
    std::uint64_t checkmask = 0ull;
    std::uint64_t threats = 0ull;
};

class board {
    Color  side; // side to move
    Square enpassant;
    std::uint8_t castling_rights;
    std::uint8_t  fifty_move_clock;

    std::array<Piece, 64> pieces;
    std::array<std::array<uint64_t, 6>, 2> bitboards;
    std::array<std::uint64_t, 2>  side_occupancy; // occupancy bitboards
    std::uint64_t occupancy;

    zobrist hash_key;

    std::vector<board_info> history;
    board_info * info;
public:
    board (const std::string & fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
            : enpassant(Square::Null_Square), bitboards{}, side_occupancy{}, occupancy{} {
        castling_rights = fifty_move_clock;
        hash_key = zobrist();
        for (auto& piece : pieces) {
            piece = Piece::Null_Piece;
        }
        side = Color::White;
        fen_to_board(fen);
    }

    void fen_to_board(const std::string& fen) {
        // Reset bitboards
        bitboards = {};

        for (auto & piece : pieces) {
            piece = Piece::Null_Piece;
        }

        side_occupancy[Color::White] = side_occupancy[Color::Black] = occupancy = 0ull;
        history.clear();
        history.reserve(2000);
        info = history.data();

        hash_key = zobrist();

        std::string board_str, side_str, castling_str, enpassant_str; //, fifty_move_clock, full_move_number

        std::stringstream ss(fen);
        ss >> board_str >> side_str >> castling_str >> enpassant_str >> fifty_move_clock ;//>> full_move_counter;

        int square = Square::A8;
        for (const char fen_char : board_str) {
            if (std::isdigit(fen_char)) {
                square += fen_char  - '0';
            } else if (fen_char == '/') {
                square -= 16;
            } else {
                auto [color, piece] = get_color_and_piece(fen_char);
                const auto piece_square = static_cast<Square>(square);
                set_bit(bitboards[color][piece], square);
                hash_key.update_psqt_hash(color, piece, piece_square);
                pieces[square] = piece;
                square += 1;
            }
        }

        for (const auto piece : {Pawn, Knight, Bishop, Rook, Queen, King}) {
            side_occupancy[White] |= bitboards[White][piece];
            side_occupancy[Black] |= bitboards[Black][piece];
        }
        occupancy = side_occupancy[White] | side_occupancy[Black];

        std::uint64_t checks;
        if (side_str == "w") {
            side = Color::White;
        } else {
            side = Color::Black;
            hash_key.update_side_hash();
        }

        castling_rights = 0;
        for (const char fen_right : castling_str) {
            castling_rights |= char_to_castling_right(fen_right);
        }
        hash_key.update_castling_hash(castling_rights);

        enpassant = square_from_string(enpassant_str);
        hash_key.update_enpassant_hash(enpassant);

        chess_move move;
        board_info binfo {castling_rights, enpassant, fifty_move_clock, Piece::Null_Piece, move, hash_key};
        history.push_back(binfo);

        side == White ? update_bitboards<White>() : update_bitboards<Black>();
    }

    template <Color our_color>
    void calculate_threats() {
        constexpr Color their_color = our_color == White ? Black : White;

        std::uint64_t threatened = pawn_attacks<their_color>(bitboards[their_color][Pawn]);

        std::uint64_t knights = bitboards[their_color][Knight];
        while (knights) {
            threatened |= KNIGHT_ATTACKS[pop_lsb(knights)];
        }
        std::uint64_t occupied = occupancy ^ bitboards[our_color][King]; // x-ray through king

        std::uint64_t bishops = bitboards[their_color][Bishop];
        while (bishops) {
            threatened |= attacks<Ray::BISHOP>(pop_lsb(bishops), occupied); // x-ray through king
        }

        std::uint64_t rooks = bitboards[their_color][Rook];
        while (rooks) {
            threatened |= attacks<Ray::ROOK>(pop_lsb(rooks), occupied);
        }

        std::uint64_t queens = bitboards[their_color][Queen];
        while (queens) {
            threatened |= attacks<Ray::QUEEN>(pop_lsb(queens), occupied);
        }

        threatened |= KING_ATTACKS[lsb(bitboards[their_color][King])];
        info->threats = threatened;
    }

    template <Color our_color>
    void calculate_checkers() {
        // state->checkers = (state->threats[King] & bitboards[our_color][King]) != 0ull ? attackers<our_color>(lsb(bitboards[our_color][King])) : 0ull;
        info->checkers = attackers<our_color>(lsb(bitboards[our_color][King]));
    }

    template <Color our_color>
    void calculate_pins() {
        constexpr Color their_color = our_color == White ? Black : White;
        const Square king_square = lsb(bitboards[our_color][King]);

        std::uint64_t diagonal_pins = 0ull, orthogonal_pins = 0ull;

        std::uint64_t pinners = (attacks<Ray::BISHOP>(king_square, side_occupancy[their_color]) & (bitboards[their_color][Bishop] | bitboards[their_color][Queen]));
        while (pinners) {
            const Square s = pop_lsb(pinners);
            const std::uint64_t b = pinmask[king_square][s];

            if(popcount(b & side_occupancy[our_color]) == 1) {
                diagonal_pins |= b;
            }
        }

        pinners = (attacks<Ray::ROOK>(king_square   , side_occupancy[their_color]) & (bitboards[their_color][Rook] | bitboards[their_color][Queen]));
        while (pinners) {
            const Square s = pop_lsb(pinners);
            const std::uint64_t b = pinmask[king_square][s];

            if(popcount(b & side_occupancy[our_color]) == 1) {
                orthogonal_pins |= b;
            }
        }

        info->orthogonal_pins = orthogonal_pins;
        info->diagonal_pins = diagonal_pins;
    }

    template <Color our_color>
    void update_bitboards() {
        calculate_threats<our_color>();
        calculate_checkers<our_color>();
        info->checkmask = 0ull;
        if (info->checkers) {
            info->checkmask |= pinmask[get_king_square<our_color>()][lsb(info->checkers)];
        }
    }

    [[nodiscard]] bool in_check() const {
        return info->checkers;
    }

    template <Color color>
    [[nodiscard]] std::uint64_t attackers(const Square square) const {
        constexpr Color their_color = color == White ? Black : White;
        return    (attacks<Ray::ROOK>(square, occupancy) & (bitboards[their_color][Rook] | bitboards[their_color][Queen]))
                  | (attacks<Ray::BISHOP>(square, occupancy) & (bitboards[their_color][Bishop] | bitboards[their_color][Queen]))
                  | (KING_ATTACKS[square]   & bitboards[their_color][King])
                  | (KNIGHT_ATTACKS[square] & bitboards[their_color][Knight])
                  | (PAWN_ATTACKS_TABLE[side][square] & bitboards[their_color][Pawn]);
    }

    template <Color color>
    [[nodiscard]] std::uint64_t attackers(const Square square, std::uint64_t occ) const {
        constexpr Color their_color = color == White ? Black : White;
        return    (attacks<Ray::ROOK>(square, occ) & (bitboards[their_color][Rook] | bitboards[their_color][Queen]))
                  | (attacks<Ray::BISHOP>(square, occ) & (bitboards[their_color][Bishop] | bitboards[their_color][Queen]))
                  | (KING_ATTACKS[square] & bitboards[their_color][King])
                  | (KNIGHT_ATTACKS[square] & bitboards[their_color][Knight])
                  | (PAWN_ATTACKS_TABLE[color][square] & bitboards[their_color][Pawn]);
    }

    [[nodiscard]] Piece get_piece(const int square) const {
        return pieces[square];
    }

    [[nodiscard]] Color get_side() const {
        return side;
    }

    [[nodiscard]] Square get_king_square() const {
        return lsb(bitboards[side][King]);
    }

    template <Color color>
    [[nodiscard]] Square get_king_square() const {
        return lsb(bitboards[color][King]);
    }

    [[nodiscard]] std::uint64_t get_occupancy() const {
        return occupancy;
    }

    template <Color color>
    [[nodiscard]] std::uint64_t get_side_occupancy() const {
        return side_occupancy[color];
    }

    [[nodiscard]] std::uint64_t get_pieces (int color, int piece) const {
        return bitboards[color][piece];
    }

    template <Color color>
    [[nodiscard]] std::uint64_t get_orthogonal_pieces() const { return bitboards[color][Rook] | bitboards[color][Queen]; }
    template <Color color>
    [[nodiscard]] std::uint64_t get_diagonal_pieces() const { return bitboards[color][Bishop] | bitboards[color][Queen]; }

    [[nodiscard]] bool can_castle(CastlingRight cr) const { return castling_rights & cr; }

    [[nodiscard]] Square enpassant_square() const {
        return enpassant;
    }

    [[nodiscard]] bool pawn_endgame() const {
        return side_occupancy[side] == (bitboards[side][Pawn] | bitboards[side][King]);
    }

    [[nodiscard]] bool is_draw() const {
        if (fifty_move_clock < 4) {
            return false;
        }

        if (fifty_move_clock >= 100) {
            return true;
        }

        const int end = std::max(0, static_cast<int>(history.size()) - 1 - fifty_move_clock);

        for (int i = static_cast<int>(history.size()) - 3; i >= end; i -= 2) {
            if (history[i].hash_key == hash_key) {
                return true;
            }
        }

        return false;
    }

    void update_castling_rights(Square square) {
        hash_key.update_castling_hash(castling_rights);
        castling_rights &= castling_mask[square];
        hash_key.update_castling_hash(castling_rights);
    }

    template<Color our_color, bool make>
    void set_piece(const Square square, const Piece piece) {
        pieces[square] = piece;

        const std::uint64_t bitboard = (1ull << square);

        bitboards[our_color][piece] |= bitboard;
        side_occupancy[our_color] |= bitboard;
        occupancy |= bitboard;

        if constexpr (make) {
            hash_key.update_psqt_hash(our_color, piece, square);
        }
    }

    template<Color our_color, bool make>
    void unset_piece(const Square square, const Piece piece) {
        pieces[square] = Piece::Null_Piece;

        const std::uint64_t bitboard = ~(1ull << square);

        bitboards[our_color][piece] &= bitboard;
        side_occupancy[our_color] &= bitboard;
        occupancy &= bitboard;

        if constexpr (make) {
            hash_key.update_psqt_hash(our_color, piece, square);
        }
    }

    template<Color our_color, Color their_color, bool make>
    void replace_piece(const Square square, const Piece piece, const Piece captured_piece) {
        pieces[square] = piece;

        const std::uint64_t bitboard = (1ull << square);
        const std::uint64_t rem_bitboard = ~bitboard;

        bitboards[their_color][captured_piece] &= rem_bitboard;
        bitboards[our_color][piece] |= bitboard;
        side_occupancy[their_color] &= rem_bitboard;
        side_occupancy[our_color]   |= bitboard;

        if constexpr (make) {
            hash_key.update_psqt_hash(our_color, piece, square);
            hash_key.update_psqt_hash(their_color, captured_piece, square);
        }
    }

    template<Color color>
    void make_null_move()
    {
        constexpr Color their_color = (color == Black) ? White : Black;

        side = their_color;
        hash_key.update_side_hash();
        hash_key.update_enpassant_hash(enpassant);
        enpassant = Square::Null_Square;
        fifty_move_clock++;

        chess_move move;
        history.emplace_back(castling_rights, enpassant, fifty_move_clock, Piece::Null_Piece, move, hash_key);
        info = &history.back();
        calculate_threats<their_color>();
    }

    template<Color color>
    void undo_null_move()
    {
        history.pop_back();
        info--;

        side = color;
        hash_key = info->hash_key;
        enpassant = info->enpassant;
        castling_rights = info->castling_rights;
        fifty_move_clock = info->fifty_move_clock;
    }

    [[nodiscard]] std::uint64_t get_hash_key() const {
        return hash_key.get_key();
    }

    [[nodiscard]] chess_move get_last_played_move() const {
        return info->move;
    }

    void increment_fifty_move_clock() {
        this->fifty_move_clock++;
    }

    void reset_fifty_move_clock() {
        this->fifty_move_clock = 0;
    }

    void set_side(Color color) {
        this->side = color;
    }

    void set_enpassant(Square square) {
        this->enpassant = square;
        hash_key.update_enpassant_hash(enpassant);
    }

    void emplace_history(Piece captured_piece, chess_move move) {
        history.emplace_back(castling_rights, enpassant, fifty_move_clock, captured_piece, move, hash_key);
        info = &history.back();
    }

    void update_board_hash() {
        hash_key.update_enpassant_hash(enpassant);
        hash_key.update_side_hash();
    }

    board_info get_history() {
        return history.back();
    }

    void undo_history() {
        history.pop_back();
        info--;
        enpassant = info->enpassant;
        fifty_move_clock = info->fifty_move_clock;
        castling_rights = info->castling_rights;
        hash_key = info->hash_key;
    }

    generator_data get_generator_data() {
        generator_data generatorData = {};
        generatorData.orthogonal_pins = info->orthogonal_pins;
        generatorData.diagonal_pins = info->diagonal_pins;
        generatorData.checkmask = info->checkmask;
        generatorData.threats = info->threats;
        generatorData.checkers = info->checkers;
        return generatorData;
    }
};

#endif //MOTOR_BOARD_HPP