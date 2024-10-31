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

#include "../evaluation/nnue.hpp"

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
    std::uint8_t castling_rights = {};
    Square enpassant = Square::Null_Square;
    std::uint8_t fifty_move_clock = {};
    Piece captured_piece = Piece::Null_Piece;
    chess_move move = {};
    zobrist hash_key = {};
    zobrist pawn_key = {};
    zobrist minor_key = {};
    zobrist center_key = {};
    std::array<zobrist, 2> nonpawn_key = {};
    std::uint64_t threats = {};
    std::uint64_t checkers = {};
    std::uint64_t checkmask = {};
    std::uint64_t pin_diagonal = {};
    std::uint64_t pin_orthogonal = {};
};

class board {
    std::array<Piece, 64> pieces;
    std::array<std::array<std::uint64_t, 6>, 2> bitboards;
    std::array<std::uint64_t, 2> side_occupancy; // occupancy bitboards
    std::uint64_t occupancy;
    board_info * state;
    std::array<board_info, 384> history;
    Color  side; // side to move
public:
    board (const std::string & fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
            : bitboards{}, side_occupancy{}, occupancy{}, history {}, side {Color::White}  {
        state = &history[0];
        std::ranges::fill(pieces, Piece::Null_Piece);
        fen_to_board(fen);
    }

    void fen_to_board(const std::string& fen) {
        bitboards = {};
        side_occupancy = {};
        occupancy = {};
        std::ranges::fill(pieces, Piece::Null_Piece);
        history = {};
        state = &history[0];
        side = Color::White;
        state->hash_key = zobrist();
        state->pawn_key = zobrist();
        state->center_key = zobrist();
        state->minor_key = zobrist();
        state->nonpawn_key[White] = state->nonpawn_key[Black] = zobrist();

        std::string board_str, side_str, castling_str, enpassant_str; //, fifty_move_clock, full_move_number

        std::stringstream ss(fen);
        ss >> board_str >> side_str >> castling_str >> enpassant_str >> state->fifty_move_clock ;//>> full_move_counter;

        int square = Square::A8;
        for (const char fen_char : board_str) {
            if (std::isdigit(fen_char)) {
                square += fen_char  - '0';
            } else if (fen_char == '/') {
                square -= 16;
            } else {
                auto [color, piece] = get_color_and_piece(fen_char);
                const auto piece_square = static_cast<Square>(square);
                bitboards[color][piece] |= bb(piece_square);
                update_hash(color, piece, piece_square);
                pieces[square] = piece;
                square += 1;
            }
        }

        for (const auto piece : {Pawn, Knight, Bishop, Rook, Queen, King}) {
            side_occupancy[White] |= bitboards[White][piece];
            side_occupancy[Black] |= bitboards[Black][piece];
        }
        occupancy = side_occupancy[White] | side_occupancy[Black];

        if (side_str == "w") {
            side = Color::White;
        } else {
            side = Color::Black;
            state->hash_key.update_side_hash();
        }

        state->castling_rights = 0;
        for (const char fen_right : castling_str) {
            state->castling_rights |= char_to_castling_right(fen_right);
        }
        state->hash_key.update_castling_hash(state->castling_rights);

        state->enpassant = square_from_string(enpassant_str);
        state->hash_key.update_enpassant_hash(state->enpassant);

        update_bitboards();
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
        state->threats = threatened;
    }

    template <Color our_color>
    void calculate_checkers() {
        // state->checkers = (state->threats & bitboards[our_color][King]) != 0ull ? attackers<our_color>(lsb(bitboards[our_color][King])) : 0ull;
        state->checkers = attackers<our_color>(lsb(bitboards[our_color][King]));
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

        state->pin_diagonal = diagonal_pins;
        state->pin_orthogonal = orthogonal_pins;
    }

    template <Color our_color>
    void update_bitboards() {
        calculate_threats<our_color>();
        calculate_checkers<our_color>();
        state->checkmask = 0ull;
        std::uint64_t checks = checkers();
        if (checks) {
            state->checkmask |= pinmask[get_king_square<our_color>()][lsb(checks)];
        }
    }

    void update_bitboards() {
        side == White ? update_bitboards<White>() : update_bitboards<Black>();
    }

    [[nodiscard]] bool in_check() const {
        return state->checkers;
    }

    template <Color color>
    [[nodiscard]] std::uint64_t attackers(const Square square) const {
        constexpr Color their_color = color == White ? Black : White;
        return    (attacks<Ray::ROOK>(square, occupancy) & (bitboards[their_color][Rook] | bitboards[their_color][Queen]))
                  | (attacks<Ray::BISHOP>(square, occupancy) & (bitboards[their_color][Bishop] | bitboards[their_color][Queen]))
                  | (KNIGHT_ATTACKS[square] & bitboards[their_color][Knight])
                  | (PAWN_ATTACKS_TABLE[side][square] & bitboards[their_color][Pawn]);
    }

    template <Color color>
    [[nodiscard]] std::uint64_t attackers(const Square square, std::uint64_t occ) const {
        constexpr Color their_color = color == White ? Black : White;
        return    (attacks<Ray::ROOK>(square, occ) & (bitboards[their_color][Rook] | bitboards[their_color][Queen]))
                  | (attacks<Ray::BISHOP>(square, occ) & (bitboards[their_color][Bishop] | bitboards[their_color][Queen]))
                  | (KING_ATTACKS[square]   & bitboards[their_color][King])
                  | (KNIGHT_ATTACKS[square] & bitboards[their_color][Knight])
                  | (PAWN_ATTACKS_TABLE[color][square] & bitboards[their_color][Pawn]);
    }

    [[nodiscard]] Piece get_piece(const int square) const {
        return pieces[square];
    }

    [[nodiscard]] Color get_side() const {
        return side;
    }

    template <Color color>
    [[nodiscard]] Square get_king_square() const {
        return lsb(bitboards[color][King]);
    }

    [[nodiscard]] Square get_king_square() const {
        return lsb(bitboards[side][King]);
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

    [[nodiscard]] Square enpassant_square() const {
        return state->enpassant;
    }

    [[nodiscard]] bool pawn_endgame() const {
        return side_occupancy[side] == (bitboards[side][Pawn] | bitboards[side][King]);
    }

    [[nodiscard]] bool is_draw() const {
        if (state->fifty_move_clock < 4) {
            return false;
        }

        if (state->fifty_move_clock >= 100) {
            return true;
        }

        const int current_index = state - history.data();
        const int end = std::max(0, current_index - static_cast<int>(state->fifty_move_clock));

        for (int i = current_index - 4; i >= end; i -= 2) {
            if (history[i].hash_key == state->hash_key) {
                return true;
            }
        }

        return false;
    }

    void update_castling_rights(Square square) {
        state->hash_key.update_castling_hash(state->castling_rights);
        state->castling_rights &= castling_mask[square];
        state->hash_key.update_castling_hash(state->castling_rights);
    }

    template<Color color>
    void make_null_move()
    {
        constexpr Color their_color = (color == Black) ? White : Black;

        side = their_color;
        board_info * old_info = state++;
        state->hash_key = old_info->hash_key;
        state->hash_key.update_side_hash();
        state->hash_key.update_enpassant_hash(old_info->enpassant);
        state->pawn_key = old_info->pawn_key;
        state->minor_key = old_info->minor_key;
        state->center_key = old_info->center_key;
        state->nonpawn_key = old_info->nonpawn_key;
        state->enpassant = Square::Null_Square;
        state->fifty_move_clock++;
        state->castling_rights = old_info->castling_rights;
        state->fifty_move_clock = old_info->fifty_move_clock + 1;
        state->captured_piece = Null_Piece;
        state->move = {};
        state->checkers = 0ull;
        calculate_threats<their_color>();
    }

    template<Color color>
    void undo_null_move()
    {
        side = color;
        state--;
    }

    [[nodiscard]] std::uint64_t get_hash_key() const {
        return state->hash_key.get_key();
    }

    [[nodiscard]] std::uint64_t get_pawn_key() const {
        return state->pawn_key.get_key();
    }

    [[nodiscard]] std::uint64_t get_minor_key() const {
        return state->minor_key.get_key();
    }

    [[nodiscard]] std::uint64_t get_center_key() const {
        return state->center_key.get_key();
    }

    [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> get_nonpawn_key() const {
        return { state->nonpawn_key[White].get_key(), state->nonpawn_key[Black].get_key()};
    }

    [[nodiscard]] chess_move get_last_played_move() const {
        return history.back().move;
    }

    void reset_fifty_move_clock() {
        this->state->fifty_move_clock = 0;
    }

    void set_enpassant(Square square) {
        this->state->enpassant = square;
        state->hash_key.update_enpassant_hash(state->enpassant);
    }

    template <Color color>
    [[nodiscard]] std::uint64_t get_orthogonal_pieces() const { return bitboards[color][Rook] | bitboards[color][Queen]; }
    template <Color color>
    [[nodiscard]] std::uint64_t get_diagonal_pieces() const { return bitboards[color][Bishop] | bitboards[color][Queen]; }

    [[nodiscard]] std::uint64_t checkmask() const { return state->checkmask; }
    [[nodiscard]] std::uint64_t pin_diagonal() const { return state->pin_diagonal; }
    [[nodiscard]] std::uint64_t pin_orthogonal() const { return state->pin_orthogonal; }
    [[nodiscard]] std::uint64_t checkers() const { return state->checkers; }
    [[nodiscard]] std::uint64_t checked_squares() const { return state->threats; }

    [[nodiscard]] bool can_castle(CastlingRight cr) const { return state->castling_rights & cr; }

    template<Color Me> void set_piece(Piece p, Square sq){
        std::uint64_t b = bb(sq);
        pieces[sq] = p;
        occupancy |= b;
        side_occupancy[Me] |= b;
        bitboards[Me][p] |= b;
    }

    template<Color Me> void unset_piece(Piece piece, Square sq) {
        std::uint64_t b = bb(sq);
        pieces[sq] = Null_Piece;
        occupancy &= ~b;
        side_occupancy[Me] &= ~b;
        bitboards[Me][piece] &= ~b;
    }

    template<Color Me> void move_piece(Piece piece, Square from, Square to) {
        std::uint64_t fromTo = bb(from) | bb(to);
        pieces[to] = piece;
        pieces[from] = Null_Piece;
        occupancy ^= fromTo;
        side_occupancy[Me] ^= fromTo;
        bitboards[Me][piece] ^= fromTo;
    }

    Piece get_captured_piece() {
        return state->captured_piece;
    }

    template <Color color>
    void undo_state() {
        state--;
        side = color;
    }

    template <Color color>
    void make_state(Piece captured_piece, chess_move played_move) {
        board_info * old_state = state++;
        state->hash_key = old_state->hash_key;
        state->hash_key.update_enpassant_hash(old_state->enpassant);
        state->hash_key.update_side_hash();
        state->pawn_key = old_state->pawn_key;
        state->center_key = old_state->center_key;
        state->minor_key = old_state->minor_key;
        state->nonpawn_key = old_state->nonpawn_key;
        state->enpassant = Null_Square;
        state->castling_rights = old_state->castling_rights;
        state->fifty_move_clock = old_state->fifty_move_clock + 1;
        state->captured_piece = captured_piece;
        state->move = played_move;
        side = color;
    }

    void update_hash(Color color, Piece piece, Square square) {
        state->hash_key.update_psqt_hash(color, piece, square);

        if (bb(square) & 0xffff000000ull) {
            state->center_key.update_psqt_hash(color, piece, square);
        }

        if (piece == Pawn) {
            state->pawn_key.update_psqt_hash(color, piece, square);
        } else {
            state->nonpawn_key[color].update_psqt_hash(color, piece, square);
            if (piece == Knight || piece == Bishop || piece == King) {
                state->minor_key.update_psqt_hash(color, piece, square);
            }
        }
    }

    bool is_quiet(const chess_move & move) {
        if (move.get_move_type() == PROMOTION || move.get_move_type() == EN_PASSANT || pieces[move.get_to()] != Null_Piece)
            return false;
        return true;
    }

    bool is_capture(const chess_move & move) {
        return pieces[move.get_to()] != Null_Piece;
    }

    void shift_history() {
        const int index = state - history.data();
        for (int i = 100; i <= index; ++i) {
            history[i - 100] = history[i];
        }

        board_info default_value = {}; // Default constructed board_info
        std::fill(history.begin() + (index - 100 + 1), history.begin() + (index + 1), default_value);

        state -= 100;
    }

    std::uint64_t get_threats() const {
        return state->threats;
    }

    int move_count() {
        const int current_index = state - history.data();
        return (current_index + 1) / 2;
    }

    std::uint64_t get_material_key() const {
        auto murmur_hash_3 = [](std::uint64_t key) -> std::uint64_t {
            key ^= key >> 33;
            key *= 0xff51afd7ed558ccd;
            key ^= key >> 33;
            key *= 0xc4ceb9fe1a85ec53;
            key ^= key >> 33;
            return key;
        };

        std::uint64_t material_key = 0;

        for (Color c : {Color::White, Color::Black}) {
            for (Piece p : {Pawn, Knight, Bishop, Rook, Queen}) {
                int shift = p * 6 + c * 30;
                std::uint64_t count = std::popcount(this->bitboards[c][p]);
                material_key |= (count << shift);
            }
        }

        return murmur_hash_3(material_key);
    }
};

#endif //MOTOR_BOARD_HPP