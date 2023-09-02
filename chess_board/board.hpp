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
    uint8_t castling_rights;
    Square enpassant;
    uint8_t fifty_move_clock;
    chess_move move;
    zobrist hash_key;
    Piece captured_piece;
};

class board {
    Color  side; // side to move
    Square enpassant;
    uint8_t castling_rights;

    std::array<Piece, 64> pieces;
    std::array<std::array<uint64_t, 6>, 2> bitboards;
    std::array<uint64_t, 2>  side_occupancy; // occupancy bitboards
    std::uint64_t occupancy;

    std::uint8_t  fifty_move_clock;
    zobrist hash_key;

    std::vector<board_info> history;
public:
    board(const std::string & fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
            : bitboards{}, side_occupancy{}, occupancy{}, enpassant(Square::Null_Square) {
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
        for(auto & color_bitboard : bitboards) {
            for (auto & bitboard: color_bitboard) {
                bitboard = 0ull;
            }
        }
        for (auto & piece : pieces) {
            piece = Piece::Null_Piece;
        }
        side_occupancy[Color::White] = side_occupancy[Color::Black] = occupancy = 0ull;
        history.clear();
        hash_key = zobrist();

        // FEN fields
        std::string board_str, side_str, castling_str, enpassant_str; //, fifty_move_clock, full_move_number

        // Split the FEN string into fields
        std::stringstream ss(fen);
        ss >> board_str >> side_str >> castling_str >> enpassant_str >> fifty_move_clock ;//>> full_move_counter;

        // Parse the board state
        std::int8_t rank = 7, file = 0;
        std::string rank_str;
        std::stringstream rank_ss(board_str);
        while (std::getline(rank_ss, rank_str, '/')) {
            for (char fen_char : rank_str) {
                if (std::isdigit(fen_char)) {
                    file += fen_char - '0';
                } else {
                    auto [color, piece] = get_color_and_piece(fen_char);
                    auto square = static_cast<Square>(rank * 8 + file);
                    set_bit(bitboards[color][piece], square);
                    hash_key.update_psqt_hash(color, piece, square);
                    pieces[square] = piece;
                    file++;
                }
            }
            rank--;
            file = 0;
        }

        // Parse the side to move
        if (side_str == "w") {
            side = Color::White;
        } else {
            side = Color::Black;
            hash_key.update_side_hash();
        }

        // Parse the castling rights
        castling_rights = 0;
        for(char fen_right : castling_str) {
            castling_rights |= char_to_castling_right(fen_right);
        }
        hash_key.update_castling_hash(castling_rights);

        // Parse the en passant square
        enpassant = square_from_string(enpassant_str);
        hash_key.update_enpassant_hash(enpassant);

        for (auto piece : {Pawn, Knight, Bishop, Rook, Queen}) {
            side_occupancy[White] |= bitboards[White][piece];
            side_occupancy[Black] |= bitboards[Black][piece];
        }
        occupancy = side_occupancy[White] | side_occupancy[Black];

        //chess_move nullmove;
        history.push_back({castling_rights, enpassant, fifty_move_clock, chess_move(), hash_key, Piece::Null_Piece});
    }

    [[nodiscard]] bool in_check() const {
        if(side == White) {
            return attackers<Black>(get_king_square());
        } else {
            return attackers<White>(get_king_square());
        }
    }

    template<Color their_color>
    [[nodiscard]] std::uint64_t attackers(Square square) const {
        return    (attacks<Ray::ROOK>(square, occupancy) & (bitboards[their_color][Rook] | bitboards[their_color][Queen]))
                | (attacks<Ray::BISHOP>(square, occupancy) & (bitboards[their_color][Bishop] | bitboards[their_color][Queen]))
                | (KING_ATTACKS[square]   & bitboards[their_color][King])
                | (KNIGHT_ATTACKS[square] & bitboards[their_color][Knight])
                | (PAWN_ATTACKS_TABLE[side][square] & bitboards[their_color][Pawn]);
    }

    // TODO: REFACTOR
    template <Color their_color>
    std::uint64_t get_attacked_squares() {
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
    std::uint64_t get_safe_squares(Square square) {
        pop_bit(occupancy, square);
        std::uint64_t attackers_squares = ~get_attacked_squares<their_color>();
        set_bit(occupancy, square);
        return attackers_squares;
    }

    std::uint64_t get_checkmask(std::uint64_t checkers, Square square) {
        if(checkers == 0) {
            return full_board;
        }
        int sq = pop_lsb(checkers);
        if(checkers) {
            return 0ull;
        }
        return pinmask[square][sq];
    }

    [[nodiscard]] Piece get_piece(int square) const {
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
    std::uint64_t get_side_occupancy() const {
        return side_occupancy[color];
    }

    std::uint64_t get_side_occupancy(Color color) const {
        return side_occupancy[color];
    }

    template <Color enemy_color>
    std::uint64_t get_enemy_bitboard() const {
        return side_occupancy[enemy_color];
    }

    template <Color enemy_color>
    std::uint64_t get_checkers() const {
        return attackers<enemy_color>(get_king_square());
    }

    std::uint64_t get_pieces (int color, int piece) const {
        return bitboards[color][piece];
    }

    template<Color our_color, Color their_color>
    [[nodiscard]] std::tuple<std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t> get_pinners(int king_square) const {
        std::uint64_t seen_squares = attacks<Ray::QUEEN>(king_square, occupancy);
        std::uint64_t possibly_pinned_pieces = seen_squares & side_occupancy[our_color];

        // If there are no potentially pinned pieces, return an empty tuple
        if (possibly_pinned_pieces == 0ull) {
            return {};
        }

        // Calculate the occupied squares excluding the potentially pinned pieces
        std::uint64_t occupied = occupancy ^ possibly_pinned_pieces;

        // Calculate the squares where pinning might occur for each ray direction
        std::uint64_t seen_enemy_hv_pieces = ~seen_squares & (bitboards[their_color][Rook]   | bitboards[their_color][Queen]);
        std::uint64_t seen_enemy_ad_pieces = ~seen_squares & (bitboards[their_color][Bishop] | bitboards[their_color][Queen]);
        std::uint64_t horizontal_pinners   = attacks<Ray::HORIZONTAL>(king_square, occupied)   & seen_enemy_hv_pieces;
        std::uint64_t vertical_pinners     = attacks<Ray::VERTICAL>(king_square, occupied)     & seen_enemy_hv_pieces;
        std::uint64_t antidiagonal_pinners = attacks<Ray::ANTIDIAGONAL>(king_square, occupied) & seen_enemy_ad_pieces;
        std::uint64_t diagonal_pinners     = attacks<Ray::DIAGONAL>(king_square, occupied)     & seen_enemy_ad_pieces;

        auto get_pinmask = [king_square, this] (Ray ray_type, std::uint64_t pinners) {
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

    template<Color our_color, Color their_color>
    [[nodiscard]] std::tuple<std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t> get_discovering_pieces() const {
        int enemy_king = lsb(bitboards[King][their_color]);
        std::uint64_t seen_squares = attacks<Ray::QUEEN>(enemy_king, occupancy);
        std::uint64_t possibly_discovering_pieces = seen_squares & side_occupancy[our_color];

        if (possibly_discovering_pieces == 0ull) {
            return {};
        }

        std::uint64_t occupied = occupancy ^ possibly_discovering_pieces;

        std::uint64_t check_hv_pieces = ~seen_squares & (bitboards[our_color][Rook] | bitboards[our_color][Queen]);
        std::uint64_t check_ad_pieces = ~seen_squares & (bitboards[our_color][Bishop] | bitboards[our_color][Queen]);
        std::uint64_t horizontal_pinners   = attacks<Ray::HORIZONTAL>(enemy_king, occupied)   & check_hv_pieces;
        std::uint64_t vertical_pinners     = attacks<Ray::VERTICAL>(enemy_king, occupied)     & check_hv_pieces;
        std::uint64_t antidiagonal_pinners = attacks<Ray::ANTIDIAGONAL>(enemy_king, occupied) & check_ad_pieces;
        std::uint64_t diagonal_pinners     = attacks<Ray::DIAGONAL>(enemy_king, occupied)     & check_ad_pieces;

        auto get_discover_mask = [enemy_king, this](std::uint64_t discovers, Ray ray_type) {
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

    int get_castle_rights() const {
        return castling_rights;
    }

    int enpassant_square() const {
        return enpassant;
    }

    // TODO: do removing using 3ull << some_square
    template<Color their_color>
    bool check_legality_of_enpassant (int square_from, int enpassant_pawn) const {
        // CHECK if king will get horizontal check after removing both pawns after enpassant
        Square king_square = get_king_square();
        std::uint64_t occupancy_after_enpassant = occupancy & ~(1ull << (square_from) | 1ULL << (enpassant_pawn));
        return !(attacks<Ray::HORIZONTAL>(king_square, occupancy_after_enpassant & (bitboards[their_color][Rook] | bitboards[their_color][Queen])));
    }

    bool pawn_endgame() const {
        return side_occupancy[side] == (bitboards[side][Pawn] | bitboards[side][King]);
    }

    bool is_draw() const {
        if (fifty_move_clock == 100) {
            return true;
        }

        const std::size_t end = std::max(0, static_cast<int>(history.size()) - 1 - fifty_move_clock);
        int repetitions = 0;

        for (int i = history.size() - 3; i >= end; i -= 2) {
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

    template<Color our_color>
    void set_piece(Square square, Piece piece) {
        pieces[square] = piece;

        const std::uint64_t bitboard = (1ull << square);

        bitboards[our_color][piece] |= bitboard;
        side_occupancy[our_color] |= bitboard;
        occupancy |= bitboard;

        hash_key.update_psqt_hash(our_color, piece, square);
    }

    template<Color our_color>
    void unset_piece(Square square, Piece piece) {
        pieces[square] = Piece::Null_Piece;

        const std::uint64_t bitboard = ~(1ull << square);

        bitboards[our_color][piece] &= bitboard;
        side_occupancy[our_color] &= bitboard;
        occupancy &= bitboard;

        hash_key.update_psqt_hash(our_color, piece, square);
    }

    template<Color our_color, Color their_color, bool make_move>
    void replace_piece(Square square, Piece piece, Piece captured_piece) {
        pieces[square] = piece;

        const std::uint64_t bitboard = (1ull << square);
        const std::uint64_t rem_bitboard = ~bitboard;

        bitboards[their_color][captured_piece] &= rem_bitboard;
        bitboards[our_color][piece] |= bitboard;
        side_occupancy[their_color] &= rem_bitboard;
        side_occupancy[our_color]   |= bitboard;

        hash_key.update_psqt_hash(our_color, piece, square);
        hash_key.update_psqt_hash(our_color, captured_piece, square);
    }

    template<Color our_color>
    void make_move(chess_move move) {
        fifty_move_clock++;

        hash_key.update_enpassant_hash(enpassant);
        hash_key.update_side_hash();

        enpassant = Square::Null_Square;

        constexpr Color their_color = (our_color == White) ? Black : White;
        constexpr Direction down  =   (our_color == White) ? SOUTH : NORTH;
        constexpr Square our_rook_A_square = (our_color == White) ? A1 : A8;
        constexpr Square our_rook_H_square = (our_color == White) ? H1 : H8;
        constexpr Square opponent_rook_A_square = (our_color == Black) ? A1 : A8;
        constexpr Square opponent_rook_H_square = (our_color == Black) ? H1 : H8;
        constexpr Square our_C_square = (our_color == White) ? C1 : C8;
        constexpr Square our_D_square = (our_color == White) ? D1 : D8;
        constexpr Square our_F_square = (our_color == White) ? F1 : F8;
        constexpr Square our_G_square = (our_color == White) ? G1 : G8;

        const Square square_from = move.get_from();
        const Square square_to   = move.get_to();
        const MoveType movetype  = move.get_move_type();
        const Piece piece = pieces[square_from];
        const Piece captured_piece = pieces[square_to];

        unset_piece<our_color>(square_from, piece);
        switch (movetype) {
            case QUIET:
                set_piece<our_color>(square_to, piece);
                break;
            case DOUBLE_PAWN_PUSH:
                set_piece<our_color>(square_to, Pawn);
                enpassant = (Square) (square_to + down);
                hash_key.update_enpassant_hash(enpassant);
                break;
            case KING_CASTLE:
                unset_piece<our_color>(our_rook_H_square,Rook);
                set_piece<our_color>(our_F_square, Rook);
                set_piece<our_color>(our_G_square, King);
                break;
            case QUEEN_CASTLE:
                unset_piece<our_color>(our_rook_A_square,Rook);
                set_piece<our_color>(our_D_square, Rook);
                set_piece<our_color>(our_C_square, King);
                break;
            case CAPTURE:
                fifty_move_clock = 0;
                replace_piece<our_color, their_color, true>(square_to, piece);
                update_castling_rights(square_to);
                break;
            case EN_PASSANT:
                set_piece<our_color>(square_to, piece);
                unset_piece<their_color>((Square) (square_to + down), piece);
                break;
            case KNIGHT_PROMOTION:
                set_piece<our_color>(square_to, Knight);
                break;
            case BISHOP_PROMOTION:
                set_piece<our_color>(square_to, Bishop);
                break;
            case ROOK_PROMOTION:
                set_piece<our_color>(square_to, Rook);
                break;
            case QUEEN_PROMOTION:
                set_piece<our_color>(square_to, Queen);
                break;
            case KNIGHT_PROMOTION_CAPTURE:
                replace_piece<our_color, their_color, true>(square_to, Knight);
                update_castling_rights(square_to);
                break;
            case BISHOP_PROMOTION_CAPTURE:
                replace_piece<our_color, their_color, true>(square_to, Bishop);
                update_castling_rights(square_to);
                break;
            case ROOK_PROMOTION_CAPTURE:
                replace_piece<our_color, their_color, true>(square_to, Rook);
                update_castling_rights(square_to);
                break;
            case QUEEN_PROMOTION_CAPTURE:
                replace_piece<our_color, their_color, true>(square_to, Queen);
                update_castling_rights(square_to);
                break;
        }

        if (piece == Pawn) {
            fifty_move_clock = 0;
        }

        update_castling_rights(square_from);

        side = their_color;
        history.emplace_back(castling_rights, enpassant, fifty_move_clock, move, hash_key, captured_piece);
    }

    template <Color our_color>
    void undo_move() {
        board_info b_info = history.back();
        const chess_move played_move = b_info.move;
        const Piece captured_piece = b_info.captured_piece;

        side = our_color;

        constexpr Color their_color = (our_color) == White ? Black : White;
        constexpr Direction down = (our_color == White) ? SOUTH : NORTH;

        constexpr Square our_rook_A_square = (our_color == White) ? A1 : A8;
        constexpr Square our_rook_H_square = (our_color == White) ? H1 : H8;
        constexpr Square our_C_square = (our_color == White) ? C1 : C8;
        constexpr Square our_D_square = (our_color == White) ? D1 : D8;
        constexpr Square our_F_square = (our_color == White) ? F1 : F8;
        constexpr Square our_G_square = (our_color == White) ? G1 : G8;

        const Square square_from = played_move.get_from();
        const Square square_to = played_move.get_to();
        const MoveType movetype = played_move.get_move_type();
        const Piece piece = pieces[square_to];

        set_piece<our_color>(square_from, piece);
        switch (movetype) {
            case QUIET:
            case DOUBLE_PAWN_PUSH:
                unset_piece<our_color>(square_to, piece);
                break;
            case KING_CASTLE:
                set_piece<our_color>(our_rook_H_square,Rook);
                unset_piece<our_color>(our_F_square, Rook);
                unset_piece<our_color>(our_G_square, King);
                break;
            case QUEEN_CASTLE:
                set_piece<our_color>(our_rook_A_square,Rook);
                unset_piece<our_color>(our_D_square, Rook);
                unset_piece<our_color>(our_C_square, King);
                break;
            case CAPTURE:
                replace_piece<their_color, our_color, false>(square_to, captured_piece, piece);
                break;
            case EN_PASSANT:
                unset_piece<our_color>(square_to, Pawn);
                set_piece<their_color>(static_cast<Square>(square_to + down), Pawn);
                break;
            case KNIGHT_PROMOTION:
                unset_piece<our_color>(square_to, Knight);
                break;
            case BISHOP_PROMOTION:
                unset_piece<our_color>(square_to, Bishop);
                break;
            case ROOK_PROMOTION:
                unset_piece<our_color>(square_to, Rook);
                break;
            case QUEEN_PROMOTION:
                unset_piece<our_color>(square_to, Queen);
                break;
            case KNIGHT_PROMOTION_CAPTURE:
            case BISHOP_PROMOTION_CAPTURE:
            case ROOK_PROMOTION_CAPTURE:
            case QUEEN_PROMOTION_CAPTURE:
                replace_piece<their_color, our_color, false>(square_to, captured_piece, piece);
                break;
        }

        history.pop_back();
        b_info = history.back();
        enpassant = b_info.enpassant;
        fifty_move_clock  = b_info.fifty_move_clock;
        castling_rights   = b_info.castling_rights;
        hash_key = b_info.hash_key;
    }

    [[nodiscard]] std::uint64_t get_hash_key() const {
        return hash_key.get_key();
    }
};

#endif //MOTOR_BOARD_HPP
