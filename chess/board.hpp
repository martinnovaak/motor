#ifndef MOTOR_BOARD_HPP
#define MOTOR_BOARD_HPP

#include <sstream>
#include <tuple>
#include <vector>
#include <array>

#include "bits.hpp"
#include "types.hpp"
#include "attacks.hpp"
#include "chessmove.hpp"
#include "zobrist.hpp"
#include "fen_utils.hpp"
#include "pinmask.hpp"

constexpr std::array<std::uint8_t, 64> castling_mask = {
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
    chessmove played_move;
    zobrist hash_key;
};

class board {
    Color  side; // side to move
    Square enpassant;
    std::uint8_t castling_rights;
    std::uint8_t  fifty_move_clock;

    std::array<Piece, 64> pieces; // mailbox
    std::array<std::array<std::uint64_t, 6>, 2> bitboards;
    std::array<std::uint64_t, 2>  side_occupancy; // occupancy bitboards
    std::uint64_t occupancy;

    zobrist hash_key;

    std::vector<board_info> history;
public:
    board (const std::string & fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
            : enpassant(Square::Null_Square), bitboards{}, side_occupancy{}, occupancy{} {
        castling_rights = fifty_move_clock = 0;
        pieces = {};
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
                bitboards[color][piece] |= (1ull << square);
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
            checks = attackers<White>(lsb(bitboards[side][King]));
        } else {
            side = Color::Black;
            hash_key.update_side_hash();
            checks = attackers<Black>(lsb(bitboards[side][King]));
        }

        castling_rights = 0;
        for (const char fen_right : castling_str) {
            castling_rights |= char_to_castling_right(fen_right);
        }
        hash_key.update_castling_hash(castling_rights);

        enpassant = square_from_string(enpassant_str);
        hash_key.update_enpassant_hash(enpassant);

        chessmove move;
        board_info binfo {castling_rights, enpassant, fifty_move_clock, move, hash_key};
        history.push_back(binfo);
    }

    [[nodiscard]] bool in_check() const {
        if(side == White) {
            return attackers<White>(get_king_square());
        } else {
            return attackers<Black>(get_king_square());
        }
    }

    template <Color enemy_color>
    std::uint64_t get_checkmask(Square square) {
        constexpr Color our_color = enemy_color == White ? Black : White;
        std::uint64_t checkers = attackers<our_color>(lsb(bitboards[our_color][King]));

        if (checkers == 0) {
            return full_board;
        }
        int sq = pop_lsb(checkers);
        if(checkers) {
            return 0ull;
        }
        return pinmask[square][sq];
    }

    template <Color color>
    [[nodiscard]] std::uint64_t attackers(const Square square) const {
        constexpr Color their_color = color == White ? Black : White;
        std::uint64_t att =    (attacks<Ray::ROOK>(square, occupancy) & (bitboards[their_color][Rook] | bitboards[their_color][Queen]))
                  | (attacks<Ray::BISHOP>(square, occupancy) & (bitboards[their_color][Bishop] | bitboards[their_color][Queen]))
                  | (KING_ATTACKS[square]   & bitboards[their_color][King])
                  | (KNIGHT_ATTACKS[square] & bitboards[their_color][Knight])
                  | (PAWN_ATTACKS_TABLE[color][square] & bitboards[their_color][Pawn]);
        return att;
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

    template<Color their_color>
    [[nodiscard]] std::uint64_t discovery_attackers(const Square square) const {
        return    (attacks<Ray::ROOK>(square, occupancy)   & (bitboards[their_color][Rook]   | bitboards[their_color][Queen]))
                  | (attacks<Ray::BISHOP>(square, occupancy) & (bitboards[their_color][Bishop] | bitboards[their_color][Queen]));
    }

    template <Color their_color>
    [[nodiscard]] std::uint64_t get_attacked_squares() const {
        std::uint64_t attacked_squares = 0ull;

        std::uint64_t pawns = bitboards[their_color][Pawn];
        while(pawns) {
            attacked_squares |= PAWN_ATTACKS_TABLE[their_color][pop_lsb(pawns)];
        }

        std::uint64_t knights = bitboards[their_color][Knight];
        while(knights) {
            attacked_squares |= KNIGHT_ATTACKS[pop_lsb(knights)];
        }

        std::uint64_t ad_pieces = bitboards[their_color][Bishop] | bitboards[their_color][Queen];
        while(ad_pieces) {
            attacked_squares |= attacks<Ray::BISHOP>(pop_lsb(ad_pieces), occupancy);
        }

        std::uint64_t hv_pieces = bitboards[their_color][Rook] | bitboards[their_color][Queen];
        while(hv_pieces) {
            attacked_squares |= attacks<Ray::ROOK>(pop_lsb(hv_pieces), occupancy);
        }

        attacked_squares |= KING_ATTACKS[lsb(bitboards[their_color][King])];
        return attacked_squares;
    }

    template <Color their_color>
    std::uint64_t get_safe_squares(const Square square) {
        pop_bit(occupancy, square);
        const std::uint64_t attackers_squares = ~get_attacked_squares<their_color>();
        set_bit(occupancy, square);
        return attackers_squares;
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

    [[nodiscard]] int get_castle_rights() const {
        return castling_rights;
    }

    [[nodiscard]] Square enpassant_square() const {
        return enpassant;
    }

    [[nodiscard]] bool pawn_endgame() const {
        return side_occupancy[side] == (bitboards[side][Pawn] | bitboards[side][King]);
    }

    [[nodiscard]] bool is_draw() const {
        if (fifty_move_clock >= 100) {
            return true;
        }

        const int end = std::max(0, static_cast<int>(history.size()) - 1 - fifty_move_clock);
        int repetitions = 0;

        for (int i = static_cast<int>(history.size()) - 3; i >= end; i -= 2) {
            if (history[i].hash_key == hash_key) {
                repetitions++;
            }
        }

        return repetitions >= 2;
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

        chessmove move;
        history.emplace_back(castling_rights, enpassant, fifty_move_clock, move, hash_key);
    }

    template<Color color>
    void undo_null_move()
    {
        history.pop_back();
        const auto & hist = history.back();

        side = color;
        hash_key = hist.hash_key;
        enpassant = hist.enpassant;
        castling_rights = hist.castling_rights;
        fifty_move_clock = hist.fifty_move_clock;
    }

    [[nodiscard]] std::uint64_t get_hash_key() const {
        return hash_key.get_key();
    }

    [[nodiscard]] chessmove get_last_played_move() const {
        return history.back().played_move;
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

    void emplace_history(Piece captured_piece, chessmove move) {
        history.emplace_back(castling_rights, enpassant, fifty_move_clock, move, hash_key);
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
        const board_info & b_info = history.back();
        enpassant = b_info.enpassant;
        fifty_move_clock = b_info.fifty_move_clock;
        castling_rights = b_info.castling_rights;
        hash_key = b_info.hash_key;
    }

    void display() const {
        for (int rank = 7; rank >= 0; --rank) {
            for (int file = 0; file < 8; ++file) {
                const int idx = rank * 8 + file;
                const auto piece_color = static_cast<Color>((side_occupancy[1] >> idx) & 1);
                std::cout << piece_to_char(pieces[idx], piece_color) << ' ';
            }
            std::cout << std::endl;
        }
        std::cout << std::endl << std::endl;;
    }
};

#endif //MOTOR_BOARD_HPP
