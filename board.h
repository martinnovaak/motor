#ifndef MOTOR_BOARD_H
#define MOTOR_BOARD_H

#include "bitboard.h"
#include "attacks.h"
#include "pinmask.h"
#include "move_t.h"
#include <sstream>
#include <tuple>
#include <vector>

struct board_info {
    int castling_rights;
    Square enpassant;
    int fifty_move_clock;
    int full_move_counter;
    move_t m_move;
    uint64_t hv_occ_white;
    uint64_t hv_occ_black;
    uint64_t ad_occ_white;
    uint64_t ad_occ_black;
};

class board {
    Color side; // side to move_t
    Square enpassant;
    int castling_rights;

    PieceType pieces[N_SQUARES];
    uint64_t  bitboards[N_COLORS][N_PIECE_TYPES];
    uint64_t  side_occupancy[N_COLORS]; // occupancy bitboards
    uint64_t  occupancy;
    uint64_t  hv_occupancy[N_COLORS];
    uint64_t  ad_occupancy[N_COLORS];

    int fifty_move_clock;
    int full_move_counter;

    std::vector<board_info> history;
public:
    board(const std::string & fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
    : bitboards{}, side_occupancy{}, occupancy{}, enpassant(N_SQUARES) {
        castling_rights = fifty_move_clock = full_move_counter = 0;
        side = WHITE;
        fen_to_board(fen);
        history.emplace_back(castling_rights, enpassant, fifty_move_clock, full_move_counter, move_t(), hv_occupancy[WHITE], hv_occupancy[BLACK], ad_occupancy[WHITE], ad_occupancy[BLACK]);
    }

    void print_square(int square) const
    {
        bool empty = true;

        for(int color = WHITE; color < N_COLORS && empty; color++){
            for(int piece = PAWN; piece < N_PIECE_TYPES && empty; piece++) {
                if(get_bit(bitboards[color][piece], square)) {
                    std::cout << piece_to_char[Color(color)][PieceType(piece)] << " ";
                    return;
                }
            }
        }
        std::cout << ". ";
    }

    void print_board() const
    {
        for (int rank = 7; rank >= 0; rank--)
        {
            for(int file = 0; file < 8 ; file++) {
                int square = rank * 8 + file;

                // print number of file on the left side of board
                if(file == 0) {
                    std::cout << rank + 1 << " ";
                }
                print_square(square);
            }
            std::cout << "\n";
        }
        std::cout << "  ";

        // print file letters under board
        for(int file = 0; file < 8 ; file++) {
            std::cout << char('a' + file) << " ";
        }

        std::cout << ((side == WHITE) ? "\n\nWHITE" : "\n\nBLACK") << " to move_t ";

        if(enpassant != N_SQUARES) {
            std::cout << "\n\nEnpassant square: " << squareToString[enpassant];
        }

        std::cout << "\n\nCastling: " << std::bitset<4>(castling_rights) <<"\n";
        std::cout << "\n\n";
    }

    void fen_to_board(const std::string& fen) {
        // Reset bitboards
        for(auto &color_bitboard : bitboards) {
            for (auto &bitboard: color_bitboard) {
                bitboard = 0ull;
            }
        }
        side_occupancy[WHITE] = side_occupancy[BLACK] = occupancy = 0ull;

        // FEN fields
        std::string board_str, side_str, castling_str, enpassant_str; //, fifty_move_clock, full_move_number

        // Split the FEN string into fields
        std::stringstream ss(fen);
        ss >> board_str >> side_str >> castling_str >> enpassant_str >> fifty_move_clock >> full_move_counter;

        // Parse the board state
        int rank = 7, file = 0;
        std::string rank_str;
        std::stringstream rank_ss(board_str);
        while (std::getline(rank_ss, rank_str, '/')) {
            for (char c : rank_str) {
                if (std::isdigit(c)) {
                    file += c - '0';
                } else {
                    auto [color, piece] = charToPiece[c];
                    int square = rank * 8 + file;
                    set_bit(bitboards[color][piece], square);
                    pieces[square] = piece;
                    file++;
                }
            }
            rank--;
            file = 0;
        }

        // Parse the side to move
        side = (side_str == "w") ? WHITE : BLACK;

        // Parse the castling rights
        castling_rights = 0;
        for(char right : castling_str) {
            castling_rights |= char_to_castling_right.at(right);
        }

        // Parse the en passant square
        enpassant = stringToSquare.at(enpassant_str);

        for(int piece = PAWN; piece < N_PIECE_TYPES; piece ++) {
            side_occupancy[WHITE] |= bitboards[WHITE][piece];
            side_occupancy[BLACK] |= bitboards[BLACK][piece];
        }

        occupancy = (side_occupancy[WHITE] | side_occupancy[BLACK]);
        hv_occupancy[WHITE] = bitboards[WHITE][ROOK] | bitboards[WHITE][QUEEN];
        hv_occupancy[BLACK] = bitboards[BLACK][ROOK] | bitboards[BLACK][QUEEN];
        ad_occupancy[WHITE] = bitboards[WHITE][BISHOP] | bitboards[WHITE][QUEEN];
        ad_occupancy[BLACK] = bitboards[BLACK][BISHOP] | bitboards[BLACK][QUEEN];
    }

    template<Color enemy_color>
    uint64_t attackers(int square) const {
        return (attacks<ROOK>(square, occupancy) & (hv_occupancy[enemy_color]))
               | (attacks<BISHOP>(square, occupancy) & (ad_occupancy[enemy_color]))
               | (KING_ATTACKS[square] & bitboards[enemy_color][KING])
               | (KNIGHT_ATTACKS[square] & bitboards[enemy_color][KNIGHT])
               | (PAWN_ATTACKS_TABLE[side][square] & bitboards[enemy_color][PAWN]
               & side_occupancy[enemy_color]);
    }

    template <Color their_color>
    uint64_t get_attacked_squares() {
        uint64_t attacked_squares = 0ull;

        uint64_t pawns = bitboards[their_color][PAWN];
        while(pawns) {
            attacked_squares |= PAWN_ATTACKS_TABLE[their_color][pop_lsb(pawns)];
        }

        uint64_t knights = bitboards[their_color][KNIGHT];
        while(knights) {
            attacked_squares |= KNIGHT_ATTACKS[pop_lsb(knights)];
        }

        uint64_t ad_pieces = ad_occupancy[their_color];
        while(ad_pieces) {
            attacked_squares |= KGSSB::bishop(pop_lsb(ad_pieces), occupancy);
        }

        uint64_t hv_pieces = hv_occupancy[their_color];
        while(hv_pieces) {
            attacked_squares |= KGSSB::rook(pop_lsb(hv_pieces), occupancy);
        }

        attacked_squares |= KING_ATTACKS[lsb(bitboards[their_color][KING])];
        return attacked_squares;
    }

    template <Color their_color>
    uint64_t get_safe_squares(int square) {
        pop_bit(occupancy, square);
        uint64_t attackers_squares = ~get_attacked_squares<their_color>();
        set_bit(occupancy, square);
        return attackers_squares;
    }

    uint64_t get_checkmask(uint64_t checkers, int square) {
        unsigned int number_of_checks = popcount(checkers);
        if(number_of_checks == 0) {
            return full_board;
        } else if(number_of_checks == 1) {
            return pinmask[(square << 6) | lsb(checkers)];
        } else {
            return 0ull;
        }
    }

    PieceType get_piece(int square) {
        return pieces[square];
    }

    Color get_side() { return side; }

    uint64_t get_pawn_bitboard() const {
        return bitboards[side][PAWN];
    }

    uint64_t get_knight_bitboard() {
        return bitboards[side][KNIGHT];
    }

    template <Color our_color>
    std::tuple<uint64_t, uint64_t, uint64_t> get_slider_bitboards() {
        return {bitboards[our_color][ROOK], bitboards[our_color][BISHOP], bitboards[our_color][QUEEN]};
    }

    int get_king_square() const {
        return lsb(bitboards[side][KING]);
    }

    uint64_t get_occupancy() const {
        return occupancy;
    }

    template <Color enemy_color>
    uint64_t get_enemy_bitboard() const {
        return side_occupancy[enemy_color];
    }

    template <Color enemy_color>
    uint64_t get_checkers() const {
        return attackers<enemy_color>(get_king_square());
    }

    template<Color our_color, Color their_color>
    std::tuple<uint64_t, uint64_t,uint64_t, uint64_t> get_pinners(int king_square) const {
        uint64_t seen_squares = KGSSB::queen(king_square, occupancy);
        uint64_t our_side_occupancy = side_occupancy[our_color];
        uint64_t possibly_pinned_pieces = seen_squares & our_side_occupancy;
        uint64_t occupied = occupancy ^ possibly_pinned_pieces;

        uint64_t seen_enemy_pieces  = ~(seen_squares & side_occupancy[their_color]);
        uint64_t seen_enemy_hv_pieces = seen_enemy_pieces & hv_occupancy[their_color];
        uint64_t seen_enemy_ad_pieces = seen_enemy_pieces & ad_occupancy[their_color];
        uint64_t horizontal_pinners = KGSSB::rook_horizontal(king_square, occupied) & seen_enemy_hv_pieces;
        uint64_t vertical_pinners = KGSSB::rook_vertical(king_square, occupied) & seen_enemy_hv_pieces;
        uint64_t antidiagonal_pinners = KGSSB::bishop_antidiagonal(king_square, occupied) & seen_enemy_ad_pieces;
        uint64_t diagonal_pinners = KGSSB::bishop_diagonal(king_square, occupied) & seen_enemy_ad_pieces;

        uint64_t horizontal_pinmask{}, vertical_pinmask{}, antidiagonal_pinmask{}, diagonal_pinmask{};

        int king_square_shifted = king_square << 6;

        while (horizontal_pinners) {
            horizontal_pinmask |= pinmask[king_square_shifted | pop_lsb(horizontal_pinners)];
        }

        while (vertical_pinners) {
            vertical_pinmask |= pinmask[king_square_shifted | pop_lsb(vertical_pinners)];
        }

        while (antidiagonal_pinners) {
            antidiagonal_pinmask |= pinmask[king_square_shifted | pop_lsb(antidiagonal_pinners)];
        }

        while (diagonal_pinners) {
            diagonal_pinmask |= pinmask[king_square_shifted | pop_lsb(diagonal_pinners)];
        }   

        return {horizontal_pinmask, vertical_pinmask, antidiagonal_pinmask, diagonal_pinmask};
    }

    int get_castle_rights() const {
        return castling_rights;
    }

    int enpassant_square() const {
        return enpassant;
    }

    template<Color their_color>
    bool check_legality_of_enpassant (int square_from, int enpassant_pawn) const {
        // CHECK if king will get horizontal check after removing both pawns after enpassant
        int king_square = get_king_square();
        return !(KGSSB::rook_horizontal(king_square, pop_bits(occupancy, square_from, enpassant_pawn)) & (hv_occupancy[their_color]));
    }

    template<Color our_color>
    void set_piece(Square square, PieceType piece) {
        pieces[square] = piece;

        const uint64_t bitboard = (1ull << square);

        bitboards[our_color][piece] |= bitboard;
        side_occupancy[our_color] |= bitboard;
        occupancy |= bitboard;
    }

    template<Color our_color>
    void unset_piece(Square square, PieceType piece) {
        pieces[square] = N_PIECE_TYPES;

        const uint64_t bitboard = ~(1ull << square);

        bitboards[our_color][piece] &= bitboard;
        side_occupancy[our_color] &= bitboard;
        occupancy &= bitboard;
    }

    template<Color our_color, Color their_color, bool make_move>
    void replace_piece(Square square, PieceType piece) {
        PieceType captured_piece = pieces[square];

        pieces[square] = piece;

        const uint64_t bitboard = (1ull << square);
        const uint64_t rem_bitboard = ~bitboard;

        bitboards[their_color][captured_piece] &= rem_bitboard;
        bitboards[our_color][piece] |= bitboard;
        side_occupancy[their_color] &= rem_bitboard;
        side_occupancy[our_color]   |= bitboard;

        if constexpr (make_move) {
            switch (captured_piece) {
                case BISHOP:
                    ad_occupancy[their_color] &= rem_bitboard;
                    break;
                case ROOK:
                    hv_occupancy[their_color] &= rem_bitboard;
                    break;
                case QUEEN:
                    hv_occupancy[their_color] &= rem_bitboard;
                    ad_occupancy[their_color] &= rem_bitboard;
                    break;
            }
        }
    }

    template<Color our_color>
    void make_move(move_t m) {
        fifty_move_clock++;
        if constexpr (our_color == BLACK) {
            full_move_counter++;
        }
        enpassant = N_SQUARES;

        constexpr Color their_color = (our_color == WHITE) ? BLACK : WHITE;
        constexpr Direction down  =   (our_color == WHITE) ? SOUTH : NORTH;
        constexpr int forbid_player_castling = (our_color == WHITE) ? 0b1100 : 0b0011;
        constexpr int forbid_player_kingside_castling    = (our_color == WHITE) ? 0b1110 : 0b1011;
        constexpr int forbid_player_queenside_castling   = (our_color == WHITE) ? 0b1101 : 0b0111;
        constexpr int forbid_opponent_kingside_castling  = (our_color == BLACK) ? 0b1110 : 0b1011;
        constexpr int forbid_opponent_queenside_castling = (our_color == BLACK) ? 0b1101 : 0b0111;
        constexpr Square our_rook_A_square = (our_color == WHITE) ? A1 : A8;
        constexpr Square our_rook_H_square = (our_color == WHITE) ? H1 : H8;
        constexpr Square opponent_rook_A_square = (our_color == BLACK) ? A1 : A8;
        constexpr Square opponent_rook_H_square = (our_color == BLACK) ? H1 : H8;
        constexpr Square our_C_square = (our_color == WHITE) ? C1 : C8;
        constexpr Square our_D_square = (our_color == WHITE) ? D1 : D8;
        constexpr Square our_F_square = (our_color == WHITE) ? F1 : F8;
        constexpr Square our_G_square = (our_color == WHITE) ? G1 : G8;

        const Square square_from = m.get_from();
        const Square square_to   = m.get_to();
        const PieceType piece    = m.get_piece();
        const MoveType movetype  = m.get_move_type();

        unset_piece<our_color>(square_from, piece);
        switch (movetype) {
            case QUIET:
                set_piece<our_color>(square_to, piece);
                break;
            case DOUBLE_PAWN_PUSH:
                set_piece<our_color>(square_to, PAWN);
                enpassant = (Square) (square_to + down);
                break;
            case KING_CASTLE:
                unset_piece<our_color>(our_rook_H_square,ROOK);
                set_piece<our_color>(our_F_square, ROOK);
                set_piece<our_color>(our_G_square, KING);
                hv_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][ROOK];
                break;
            case QUEEN_CASTLE:
                unset_piece<our_color>(our_rook_A_square,ROOK);
                set_piece<our_color>(our_D_square, ROOK);
                set_piece<our_color>(our_C_square, KING);
                hv_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][ROOK];
                break;
            case CAPTURE:
                fifty_move_clock = 0;
                replace_piece<our_color, their_color, true>(square_to, piece);
                if(square_to == opponent_rook_H_square) {
                    castling_rights &= forbid_opponent_kingside_castling;
                } else if (square_to == opponent_rook_A_square) {
                    castling_rights &= forbid_opponent_queenside_castling;
                }
                break;
            case EN_PASSANT:
                set_piece<our_color>(square_to, piece);
                unset_piece<their_color>((Square) (square_to + down), piece);
                break;
            case KNIGHT_PROMOTION:
                set_piece<our_color>(square_to, KNIGHT);
                break;
            case BISHOP_PROMOTION:
                set_piece<our_color>(square_to, BISHOP);
                ad_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][BISHOP];
                break;
            case ROOK_PROMOTION:
                set_piece<our_color>(square_to, ROOK);
                hv_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][ROOK];
                break;
            case QUEEN_PROMOTION:
                set_piece<our_color>(square_to, QUEEN);
                hv_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][ROOK];
                ad_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][BISHOP];
                break;
            case KNIGHT_PROMOTION_CAPTURE:
                replace_piece<our_color, their_color, true>(square_to, KNIGHT);
                if(square_to == opponent_rook_H_square) {
                    castling_rights &= forbid_opponent_kingside_castling;
                } else if (square_to == opponent_rook_A_square) {
                    castling_rights &= forbid_opponent_queenside_castling;
                }
                break;
            case BISHOP_PROMOTION_CAPTURE:
                replace_piece<our_color, their_color, true>(square_to, BISHOP);
                ad_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][BISHOP];
                if(square_to == opponent_rook_H_square) {
                    castling_rights &= forbid_opponent_kingside_castling;
                } else if (square_to == opponent_rook_A_square) {
                    castling_rights &= forbid_opponent_queenside_castling;
                }
                break;
            case ROOK_PROMOTION_CAPTURE:
                replace_piece<our_color, their_color, true>(square_to, ROOK);
                hv_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][ROOK];
                if(square_to == opponent_rook_H_square) {
                    castling_rights &= forbid_opponent_kingside_castling;
                } else if (square_to == opponent_rook_A_square) {
                    castling_rights &= forbid_opponent_queenside_castling;
                }
                break;
            case QUEEN_PROMOTION_CAPTURE:
                replace_piece<our_color, their_color, true>(square_to, QUEEN);
                hv_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][ROOK];
                ad_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][BISHOP];
                if(square_to == opponent_rook_H_square) {
                    castling_rights &= forbid_opponent_kingside_castling;
                } else if (square_to == opponent_rook_A_square) {
                    castling_rights &= forbid_opponent_queenside_castling;
                }
                break;
        }

        switch (piece) {
            case PAWN:
                fifty_move_clock = 0;
                break;
            case BISHOP:
                ad_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][BISHOP];
                break;
            case ROOK:
                hv_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][ROOK];
                if (square_from == our_rook_A_square) {
                    castling_rights &= forbid_player_queenside_castling;
                } else if (square_from == our_rook_H_square) {
                    castling_rights &= forbid_player_kingside_castling;
                }
                break;
            case QUEEN:
                hv_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][ROOK];
                ad_occupancy[our_color] = bitboards[our_color][QUEEN] | bitboards[our_color][BISHOP];
                break;
            case KING:
                castling_rights &= forbid_player_castling;
                break;
        }

        side = their_color;
        history.emplace_back(castling_rights, enpassant, fifty_move_clock, full_move_counter, m, hv_occupancy[WHITE], hv_occupancy[BLACK], ad_occupancy[WHITE], ad_occupancy[BLACK]);
    }

    void make_move(move_t m) {
        side == WHITE ? make_move<WHITE>(m) : make_move<BLACK>(m);
    }

    template <Color our_color>
    void undo_move() {
        board_info b_info = history.back();
        const move_t played_move = b_info.m_move;

        side = our_color;

        constexpr Color their_color = (our_color) == WHITE ? BLACK : WHITE;
        constexpr Direction down = (our_color == WHITE) ? SOUTH : NORTH;

        const Square square_from = played_move.get_from();
        const Square square_to = played_move.get_to();
        PieceType piece = played_move.get_piece();
        const MoveType movetype = played_move.get_move_type();
        const PieceType captured_piece = played_move.get_captured_piece();

        set_piece<our_color>(square_from, piece);
        switch (movetype) {
            case QUIET:
            case DOUBLE_PAWN_PUSH:
                unset_piece<our_color>(square_to, piece);
                break;
            case KING_CASTLE:
                if constexpr (our_color == WHITE){
                    set_piece<our_color>(H1,ROOK);
                    unset_piece<our_color>(F1, ROOK);
                    unset_piece<our_color>(G1, KING);
                }
                if constexpr (our_color == BLACK){
                    set_piece<our_color>(H8,ROOK);
                    unset_piece<our_color>(F8, ROOK);
                    unset_piece<our_color>(G8, KING);
                }
                break;
            case QUEEN_CASTLE:
                if(our_color == WHITE){
                    set_piece<our_color>(A1,ROOK);
                    unset_piece<our_color>(D1, ROOK);
                    unset_piece<our_color>(C1, KING);
                } else {
                    set_piece<our_color>(A8,ROOK);
                    unset_piece<our_color>(D8, ROOK);
                    unset_piece<our_color>(C8, KING);
                }
                break;
            case CAPTURE:
                replace_piece<their_color, our_color, false>(square_to, captured_piece);
                break;
            case EN_PASSANT:
                unset_piece<our_color>(square_to, PAWN);
                set_piece<their_color>((Square) (square_to + down), PAWN);
                break;
            case KNIGHT_PROMOTION:
                unset_piece<our_color>(square_to, KNIGHT);
                break;
            case BISHOP_PROMOTION:
                unset_piece<our_color>(square_to, BISHOP);
                break;
            case ROOK_PROMOTION:
                unset_piece<our_color>(square_to, ROOK);
                break;
            case QUEEN_PROMOTION:
                unset_piece<our_color>(square_to, QUEEN);
                break;
            case KNIGHT_PROMOTION_CAPTURE:
            case BISHOP_PROMOTION_CAPTURE:
            case ROOK_PROMOTION_CAPTURE:
            case QUEEN_PROMOTION_CAPTURE:
                replace_piece<their_color, our_color, false>(square_to, captured_piece);
                break;
        }

        history.pop_back();
        b_info = history.back();
        enpassant = b_info.enpassant;
        fifty_move_clock  = b_info.fifty_move_clock;
        full_move_counter = b_info.full_move_counter;
        castling_rights = b_info.castling_rights;
        hv_occupancy[WHITE] = b_info.hv_occ_white;
        hv_occupancy[BLACK] = b_info.hv_occ_black;
        ad_occupancy[WHITE] = b_info.ad_occ_white;
        ad_occupancy[BLACK] = b_info.ad_occ_black;
    }

    void undo_move() {
        if(side == WHITE) {
            undo_move<BLACK>();
        } else {
            undo_move<WHITE>();
        }
    }
};

#endif //MOTOR_BOARD_H