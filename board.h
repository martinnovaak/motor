#ifndef MOTOR_BOARD_H
#define MOTOR_BOARD_H

#include "bitboard.h"
#include "attacks.h"
#include "move_t.h"
#include <sstream>
#include <tuple>
#include <vector>
#include <array>
#include <bitset>
#include "zobrist.h"

struct board_info {
    int castling_rights;
    Square enpassant;
    int fifty_move_clock;
    move_t m_move;
    uint64_t hash_key;
};

class board {
    Color side; // side to move
    Square enpassant;
    int castling_rights;

    std::array<PieceType, N_SQUARES> pieces;
    std::array<std::array<uint64_t, N_PIECE_TYPES>, N_COLORS> bitboards;
    std::array<uint64_t, N_COLORS>  side_occupancy; // occupancy bitboards
    uint64_t  occupancy;

    int fifty_move_clock;
    uint64_t hash_key;

    std::vector<board_info> history;
public:
    board(const std::string & fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
            : bitboards{}, side_occupancy{}, occupancy{}, enpassant(N_SQUARES) {
        castling_rights = fifty_move_clock, hash_key = 0;
        for (auto& piece : pieces) {
            piece = N_PIECE_TYPES;
        }
        side = WHITE;
        fen_to_board(fen);
    }

    char piece_at(int square) const {
        PieceType piece = pieces[square];
        Color color = (side_occupancy[WHITE] & (1ull << square)) ? WHITE : BLACK;
        return piece_to_char[color][piece];
    }

    void print_board() const {
        for (int rank = 7; rank >= 0; rank--) {
            std::cout << rank + 1 << " ";
            for(int file = 0; file < 8 ; file++) {
                int square = rank * 8 + file;
                std::cout << piece_at(square) << " ";
            }
            std::cout << "\n";
        }
        std::cout << "  ";

        // print file letters under board
        for(int file = 0; file < 8; file++) {
            std::cout << char('a' + file) << " ";
        }

        std::cout << ((side == WHITE) ? "\n\nWHITE" : "\n\nBLACK") << " to move ";

        if(enpassant != EMPTY) {
            std::cout << "\n\nEnpassant square: " << square_to_string[enpassant];
        }

        std::cout << "\n\nCastling: " << std::bitset<4>(castling_rights) <<"\n\n\n";

        std::cout << "\n\n Hash key: " << hash_key << "\n\n\n";
    }

    void fen_to_board(const std::string& fen) {
        // Reset bitboards
        for(auto &color_bitboard : bitboards) {
            for (auto &bitboard: color_bitboard) {
                bitboard = 0ull;
            }
        }
        for (auto & piece : pieces) {
            piece = N_PIECE_TYPES;
        }
        side_occupancy[WHITE] = side_occupancy[BLACK] = occupancy = 0ull;
        history.clear();

        // FEN fields
        std::string board_str, side_str, castling_str, enpassant_str; //, fifty_move_clock, full_move_number

        // Split the FEN string into fields
        std::stringstream ss(fen);
        ss >> board_str >> side_str >> castling_str >> enpassant_str >> fifty_move_clock ;//>> full_move_counter;

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
                    hash_key ^= piece_keys[color][piece][square];
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
        hash_key ^= castling_keys[castling_rights];

        // Parse the en passant square
        enpassant = stringToSquare.at(enpassant_str);
        if(enpassant != EMPTY) {
            hash_key ^= enpassant_keys[enpassant];
        }

        if(side == BLACK) {
            hash_key ^= side_key;
        }

        for(int piece = PAWN; piece < N_PIECE_TYPES; piece ++) {
            side_occupancy[WHITE] |= bitboards[WHITE][piece];
            side_occupancy[BLACK] |= bitboards[BLACK][piece];
        }

        occupancy = (side_occupancy[WHITE] | side_occupancy[BLACK]);
        history.emplace_back(castling_rights, enpassant, fifty_move_clock, move_t(), hash_key);
    }

    std::string fen() const {
        std::ostringstream oss;
        for (int rank = RANK_8; rank >= RANK_1; rank--) {
            int empty = 0;
            if ((ranks[rank] & occupancy) == 0ull) {
                oss << "8/";
                continue;
            }

            for (int file = FILE_A; file <= FILE_H; ++file) {
                auto square = Square(rank * 8 + file);
                char piece = piece_at(square);
                if (piece == '.') {
                    empty++;
                } else {
                    if (empty > 0) {
                        oss << empty;
                        empty = 0;
                    }
                    oss << piece;
                }
            }
            if (empty > 0) {
                oss << empty;
                empty = 0;
            }
            if (rank > RANK_1) {
                oss << '/';
            }
        }

        oss << ' ';
        oss << (get_side() == WHITE ? 'w' : 'b');
        oss << ' ';
        if (castling_rights & 1) {
            oss << 'K';
        }
        if (castling_rights & 2) {
            oss << 'Q';
        }
        if (castling_rights & 4) {
            oss << 'k';
        }
        if (castling_rights & 8) {
            oss << 'q';
        }
        if (castling_rights == 0) {
            oss << '-';
        }

        oss << ' ';
        if (enpassant == N_SQUARES) {
            oss << '-';
        }
        else {
            oss << square_to_string[enpassant];
        }

        oss << ' ';
        oss << fifty_move_clock;

        return oss.str();
    }

    bool in_check() const {
        if(side == WHITE) {
            return attackers<BLACK>(get_king_square());
        } else {
            return attackers<WHITE>(get_king_square());
        }
    }

    template<Color their_color>
    uint64_t attackers(int square) const {
        return     (attacks<Ray::ROOK >(square, occupancy) & (bitboards[their_color][ROOK] | bitboards[their_color][QUEEN]))
                 | (attacks<Ray::BISHOP>(square, occupancy) & (bitboards[their_color][BISHOP] | bitboards[their_color][QUEEN]))
                 | (KING_ATTACKS[square]   & bitboards[their_color][KING])
                 | (KNIGHT_ATTACKS[square] & bitboards[their_color][KNIGHT])
                 | (PAWN_ATTACKS_TABLE[side][square] & bitboards[their_color][PAWN]);
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

        uint64_t ad_pieces = bitboards[their_color][BISHOP] | bitboards[their_color][QUEEN];
        while(ad_pieces) {
            attacked_squares |= attacks<Ray::BISHOP>(pop_lsb(ad_pieces), occupancy);
        }

        uint64_t hv_pieces = bitboards[their_color][ROOK] | bitboards[their_color][QUEEN];
        while(hv_pieces) {
            attacked_squares |= attacks<Ray::ROOK>(pop_lsb(hv_pieces), occupancy);
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
        if(checkers == 0) {
            return full_board;
        }
        int sq = pop_lsb(checkers);
        if(checkers) {
            return 0ull;
        }
        return pinmask[square][sq];


        /* Probably slower
        unsigned int number_of_checks = popcount(checkers);
        if(number_of_checks == 0) {
            return full_board;
        } else if(number_of_checks == 1) {
            return pinmask[square][lsb(checkers)];
        } else {
            return 0ull;
        }
         */
    }

    PieceType get_piece(int square) const {
        return pieces[square];
    }

    Color get_side() const { return side; }

    uint64_t get_pawn_bitboard() const {
        return bitboards[side][PAWN];
    }

    uint64_t get_knight_bitboard() const {
        return bitboards[side][KNIGHT];
    }

    template <Color our_color>
    std::tuple<uint64_t, uint64_t, uint64_t> get_slider_bitboards() const {
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

    uint64_t get_pieces (int color, int piece) const {
        return bitboards[color][piece];
    }

    template<Color our_color, Color their_color>
    std::tuple<uint64_t, uint64_t,uint64_t, uint64_t> get_pinners(int king_square) const {
        uint64_t seen_squares = attacks<Ray::QUEEN>(king_square, occupancy);
        uint64_t possibly_pinned_pieces = seen_squares & side_occupancy[our_color];
        uint64_t occupied = occupancy ^ possibly_pinned_pieces;

        uint64_t seen_enemy_hv_pieces = ~seen_squares & (bitboards[their_color][ROOK] | bitboards[their_color][QUEEN]);
        uint64_t seen_enemy_ad_pieces = ~seen_squares & (bitboards[their_color][BISHOP] | bitboards[their_color][QUEEN]);
        uint64_t horizontal_pinners   = attacks<Ray::HORIZONTAL>(king_square, occupied)   & seen_enemy_hv_pieces;
        uint64_t vertical_pinners     = attacks<Ray::VERTICAL>(king_square, occupied)     & seen_enemy_hv_pieces;
        uint64_t antidiagonal_pinners = attacks<Ray::ANTIDIAGONAL>(king_square, occupied) & seen_enemy_ad_pieces;
        uint64_t diagonal_pinners     = attacks<Ray::DIAGONAL>(king_square, occupied)     & seen_enemy_ad_pieces;

        uint64_t horizontal_pinmask{}, vertical_pinmask{}, antidiagonal_pinmask{}, diagonal_pinmask{};

        while (horizontal_pinners) {
            horizontal_pinmask |= pinmask[king_square][pop_lsb(horizontal_pinners)];
        }

        while (vertical_pinners) {
            vertical_pinmask |= pinmask[king_square][pop_lsb(vertical_pinners)];
        }

        while (antidiagonal_pinners) {
            antidiagonal_pinmask |= pinmask[king_square][pop_lsb(antidiagonal_pinners)];
        }

        while (diagonal_pinners) {
            diagonal_pinmask |= pinmask[king_square][pop_lsb(diagonal_pinners)];
        }

        return { horizontal_pinmask, vertical_pinmask, antidiagonal_pinmask, diagonal_pinmask };
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
        return !(attacks<Ray::HORIZONTAL>(king_square, pop_bits(occupancy, square_from, enpassant_pawn)) & (bitboards[their_color][ROOK] | bitboards[their_color][QUEEN]));
    }

    bool pawn_endgame() const {
        return side_occupancy[side] == (bitboards[side][PAWN] | bitboards[side][KING]);
    }

    Square make_null_move() {
        Square enpas = enpassant;
        side = static_cast<Color>(side ^ 1);
        enpassant = N_SQUARES;
        history.back().enpassant = N_SQUARES;
        return enpas;
    }

    void unmake_null_move(Square enpas) {
        side = static_cast<Color>(side ^ 1);
        enpassant = enpas;
        history.back().enpassant = enpas;
    }

    template<Color our_color>
    void set_piece(Square square, PieceType piece) {
        pieces[square] = piece;

        const uint64_t bitboard = (1ull << square);

        bitboards[our_color][piece] |= bitboard;
        side_occupancy[our_color] |= bitboard;
        occupancy |= bitboard;

        hash_key ^= piece_keys[our_color][piece][square];
    }

    template<Color our_color>
    void unset_piece(Square square, PieceType piece) {
        pieces[square] = N_PIECE_TYPES;

        const uint64_t bitboard = ~(1ull << square);

        bitboards[our_color][piece] &= bitboard;
        side_occupancy[our_color] &= bitboard;
        occupancy &= bitboard;

        hash_key ^= piece_keys[our_color][piece][square];
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

        hash_key ^= piece_keys[our_color][piece][square];
        hash_key ^= piece_keys[their_color][captured_piece][square];
    }

    template<Color our_color>
    void make_move(move_t m) {
        fifty_move_clock++;

        enpassant = N_SQUARES;

        hash_key ^= enpassant_keys[enpassant];
        hash_key ^= side_key;

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
        const MoveType movetype = m.get_move_type();

        unset_piece<our_color>(square_from, piece);
        switch (movetype) {
            case QUIET:
                set_piece<our_color>(square_to, piece);
                break;
            case DOUBLE_PAWN_PUSH:
                set_piece<our_color>(square_to, PAWN);
                enpassant = (Square) (square_to + down);
                hash_key ^= enpassant_keys[enpassant];
                break;
            case KING_CASTLE:
                unset_piece<our_color>(our_rook_H_square,ROOK);
                set_piece<our_color>(our_F_square, ROOK);
                set_piece<our_color>(our_G_square, KING);
                break;
            case QUEEN_CASTLE:
                unset_piece<our_color>(our_rook_A_square,ROOK);
                set_piece<our_color>(our_D_square, ROOK);
                set_piece<our_color>(our_C_square, KING);
                break;
            case CAPTURE:
                fifty_move_clock = 0;
                replace_piece<our_color, their_color, true>(square_to, piece);
                if(square_to == opponent_rook_H_square) {
                    hash_key ^= castling_keys[castling_rights];
                    castling_rights &= forbid_opponent_kingside_castling;
                    hash_key ^= castling_keys[castling_rights];
                } else if (square_to == opponent_rook_A_square) {
                    hash_key ^= castling_keys[castling_rights];
                    castling_rights &= forbid_opponent_queenside_castling;
                    hash_key ^= castling_keys[castling_rights];
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
                break;
            case ROOK_PROMOTION:
                set_piece<our_color>(square_to, ROOK);
                break;
            case QUEEN_PROMOTION:
                set_piece<our_color>(square_to, QUEEN);
                break;
            case KNIGHT_PROMOTION_CAPTURE:
                replace_piece<our_color, their_color, true>(square_to, KNIGHT);
                if(square_to == opponent_rook_H_square) {
                    hash_key ^= castling_keys[castling_rights];
                    castling_rights &= forbid_opponent_kingside_castling;
                    hash_key ^= castling_keys[castling_rights];
                } else if (square_to == opponent_rook_A_square) {
                    hash_key ^= castling_keys[castling_rights];
                    castling_rights &= forbid_opponent_queenside_castling;
                    hash_key ^= castling_keys[castling_rights];
                }
                break;
            case BISHOP_PROMOTION_CAPTURE:
                replace_piece<our_color, their_color, true>(square_to, BISHOP);
                if(square_to == opponent_rook_H_square) {
                    hash_key ^= castling_keys[castling_rights];
                    castling_rights &= forbid_opponent_kingside_castling;
                    hash_key ^= castling_keys[castling_rights];
                } else if (square_to == opponent_rook_A_square) {
                    hash_key ^= castling_keys[castling_rights];
                    castling_rights &= forbid_opponent_queenside_castling;
                    hash_key ^= castling_keys[castling_rights];
                }
                break;
            case ROOK_PROMOTION_CAPTURE:
                replace_piece<our_color, their_color, true>(square_to, ROOK);
                if(square_to == opponent_rook_H_square) {
                    hash_key ^= castling_keys[castling_rights];
                    castling_rights &= forbid_opponent_kingside_castling;
                    hash_key ^= castling_keys[castling_rights];
                } else if (square_to == opponent_rook_A_square) {
                    hash_key ^= castling_keys[castling_rights];
                    castling_rights &= forbid_opponent_queenside_castling;
                    hash_key ^= castling_keys[castling_rights];
                }
                break;
            case QUEEN_PROMOTION_CAPTURE:
                replace_piece<our_color, their_color, true>(square_to, QUEEN);
                if(square_to == opponent_rook_H_square) {
                    hash_key ^= castling_keys[castling_rights];
                    castling_rights &= forbid_opponent_kingside_castling;
                    hash_key ^= castling_keys[castling_rights];
                } else if (square_to == opponent_rook_A_square) {
                    hash_key ^= castling_keys[castling_rights];
                    castling_rights &= forbid_opponent_queenside_castling;
                    hash_key ^= castling_keys[castling_rights];
                }
                break;
        }

        switch (piece) {
            case PAWN:
                fifty_move_clock = 0;
                break;
            case ROOK:
                if (square_from == our_rook_A_square) {
                    hash_key ^= castling_keys[castling_rights];
                    castling_rights &= forbid_player_queenside_castling;
                    hash_key ^= castling_keys[castling_rights];
                } else if (square_from == our_rook_H_square) {
                    hash_key ^= castling_keys[castling_rights];
                    castling_rights &= forbid_player_kingside_castling;
                    hash_key ^= castling_keys[castling_rights];
                }
                break;
            case KING:
                hash_key ^= castling_keys[castling_rights];
                castling_rights &= forbid_player_castling;
                hash_key ^= castling_keys[castling_rights];
                break;
        }

        side = their_color;
        history.emplace_back(castling_rights, enpassant, fifty_move_clock, m, hash_key);
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

        constexpr Square our_rook_A_square = (our_color == WHITE) ? A1 : A8;
        constexpr Square our_rook_H_square = (our_color == WHITE) ? H1 : H8;
        constexpr Square our_C_square = (our_color == WHITE) ? C1 : C8;
        constexpr Square our_D_square = (our_color == WHITE) ? D1 : D8;
        constexpr Square our_F_square = (our_color == WHITE) ? F1 : F8;
        constexpr Square our_G_square = (our_color == WHITE) ? G1 : G8;

        const Square square_from = played_move.get_from();
        const Square square_to = played_move.get_to();
        PieceType piece = played_move.get_piece();
        const MoveType movetype = played_move.get_move_type();

        set_piece<our_color>(square_from, piece);
        switch (movetype) {
            case QUIET:
            case DOUBLE_PAWN_PUSH:
                unset_piece<our_color>(square_to, piece);
                break;
            case KING_CASTLE:
                set_piece<our_color>(our_rook_H_square,ROOK);
                unset_piece<our_color>(our_F_square, ROOK);
                unset_piece<our_color>(our_G_square, KING);
                break;
            case QUEEN_CASTLE:
                set_piece<our_color>(our_rook_A_square,ROOK);
                unset_piece<our_color>(our_D_square, ROOK);
                unset_piece<our_color>(our_C_square, KING);
                break;
            case CAPTURE:
                replace_piece<their_color, our_color, false>(square_to, played_move.get_captured_piece());
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
                replace_piece<their_color, our_color, false>(square_to, played_move.get_captured_piece());
                break;
        }


        history.pop_back();
        b_info = history.back();
        enpassant = b_info.enpassant;
        fifty_move_clock  = b_info.fifty_move_clock;
        castling_rights   = b_info.castling_rights;
        hash_key = b_info.hash_key;
    }

    void undo_move() {
        side == WHITE ? undo_move<BLACK>() : undo_move<WHITE>();
    }
};

#endif //MOTOR_BOARD_H