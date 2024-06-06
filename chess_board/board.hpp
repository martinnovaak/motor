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
    }

    [[nodiscard]] bool in_check() const {
        if (side == Black) {
            return attackers<Black>(get_king_square());
        } else {
            return attackers<White>(get_king_square());
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

    template <Color their_color>
    std::uint64_t get_attacked_squares() const {
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

    template<Color our_color, Color their_color>
    [[nodiscard]] std::tuple<std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t> get_pinners(Square king_square) const {
        std::uint64_t seen_squares = attacks<Ray::QUEEN>(king_square, occupancy);
        std::uint64_t possibly_pinned_pieces = seen_squares & side_occupancy[our_color];

        // If there are no potentially pinned pieces, return an empty tuple
        if (possibly_pinned_pieces == 0ull) {
            return {};
        }

        // Calculate the occupied squares excluding the potentially pinned pieces
        std::uint64_t occupied = occupancy & ~possibly_pinned_pieces;

        // Calculate the squares where pinning might occur for each ray direction
        std::uint64_t seen_enemy_hv_pieces = ~seen_squares & (bitboards[their_color][Rook]   | bitboards[their_color][Queen]);
        std::uint64_t seen_enemy_ad_pieces = ~seen_squares & (bitboards[their_color][Bishop] | bitboards[their_color][Queen]);
        std::uint64_t horizontal_pinners   = attacks<Ray::HORIZONTAL>(king_square, occupied)   & seen_enemy_hv_pieces;
        std::uint64_t vertical_pinners     = attacks<Ray::VERTICAL>(king_square, occupied)     & seen_enemy_hv_pieces;
        std::uint64_t antidiagonal_pinners = attacks<Ray::ANTIDIAGONAL>(king_square, occupied) & seen_enemy_ad_pieces;
        std::uint64_t diagonal_pinners     = attacks<Ray::DIAGONAL>(king_square, occupied)     & seen_enemy_ad_pieces;

        auto get_pinmask = [king_square] (Ray ray_type, std::uint64_t pinners) {
            std::uint64_t pinmask_accumulator = 0ull;
            while (pinners) {
                pinmask_accumulator |= pinmask[king_square][pop_lsb(pinners)];
            }
            return pinmask_accumulator;
        };

        std::uint64_t horizontal_pinmask   = get_pinmask(Ray::HORIZONTAL, horizontal_pinners);
        std::uint64_t vertical_pinmask     = get_pinmask(Ray::VERTICAL, vertical_pinners);
        std::uint64_t antidiagonal_pinmask = get_pinmask(Ray::ANTIDIAGONAL, antidiagonal_pinners);
        std::uint64_t diagonal_pinmask     = get_pinmask(Ray::DIAGONAL, diagonal_pinners);

        return { horizontal_pinmask, vertical_pinmask, antidiagonal_pinmask, diagonal_pinmask };
    }

    template<Color our_color>
    [[nodiscard]] std::tuple<std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t> get_discovering_pieces(Square enemy_king, std::uint64_t seen_squares) const {
        //std::uint64_t seen_squares = attacks<Ray::QUEEN>(enemy_king, occupancy);
        const std::uint64_t possibly_discovering_pieces = seen_squares & side_occupancy[our_color];

        if (possibly_discovering_pieces == 0ull) {
            return {};
        }

        std::uint64_t occupied = occupancy & ~possibly_discovering_pieces;

        std::uint64_t check_hv_pieces = ~seen_squares & (bitboards[our_color][Rook] | bitboards[our_color][Queen]);
        std::uint64_t check_ad_pieces = ~seen_squares & (bitboards[our_color][Bishop] | bitboards[our_color][Queen]);
        std::uint64_t horizontal_pinners   = attacks<Ray::HORIZONTAL>(enemy_king, occupied)   & check_hv_pieces;
        std::uint64_t vertical_pinners     = attacks<Ray::VERTICAL>(enemy_king, occupied)     & check_hv_pieces;
        std::uint64_t antidiagonal_pinners = attacks<Ray::ANTIDIAGONAL>(enemy_king, occupied) & check_ad_pieces;
        std::uint64_t diagonal_pinners     = attacks<Ray::DIAGONAL>(enemy_king, occupied)     & check_ad_pieces;

        auto get_discover_mask = [enemy_king](std::uint64_t discovers, Ray ray_type) {
            std::uint64_t discover_mask_accumulator = 0;
            while (discovers) {
                discover_mask_accumulator |= pinmask[pop_lsb(discovers)][enemy_king];
            }
            return discover_mask_accumulator;
        };

        std::uint64_t horizontal_discover_mask   = get_discover_mask(horizontal_pinners, Ray::HORIZONTAL);
        std::uint64_t vertical_discover_mask     = get_discover_mask(vertical_pinners, Ray::VERTICAL);
        std::uint64_t antidiagonal_discover_mask = get_discover_mask(antidiagonal_pinners, Ray::ANTIDIAGONAL);
        std::uint64_t diagonal_discover_mask     = get_discover_mask(diagonal_pinners, Ray::DIAGONAL);

        return { horizontal_discover_mask, vertical_discover_mask, antidiagonal_discover_mask, diagonal_discover_mask };
    }

    [[nodiscard]] int get_castle_rights() const {
        return castling_rights;
    }

    [[nodiscard]] Square enpassant_square() const {
        return enpassant;
    }

    template<Color their_color>
    [[nodiscard]] bool check_legality_of_enpassant (Square square_from, Square enpassant_pawn) const {
        // CHECK if king will get horizontal check after removing both pawns after enpassant
        Square king_square = get_king_square();
        std::uint64_t occupancy_after_enpassant = ~((1ull << (square_from)) | (1ull << (enpassant_pawn))) & occupancy;
        return !(attacks<Ray::HORIZONTAL>(king_square, occupancy_after_enpassant) & (bitboards[their_color][Rook] | bitboards[their_color][Queen]));
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

    [[nodiscard]] chess_move get_last_played_move() const {
        return history.back().move;
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
};

#endif //MOTOR_BOARD_HPP
