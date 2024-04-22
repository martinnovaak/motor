#ifndef MOTOR_SEE_HPP
#define MOTOR_SEE_HPP

#include <cstdint>
#include "../../chess_board/board.hpp"

constexpr std::int32_t SEE_VALUES[7] = { 100, 300, 300, 500, 900, 0, 0 };

template <Color color>
static bool see(board& chessboard, const chess_move& chessmove, int threshold = 0) {
    Square from = chessmove.get_from();
    Square to = chessmove.get_to();

    int balance = -threshold;

    Piece captured_piece = chessmove.get_move_type() == EN_PASSANT ? Pawn : chessboard.get_piece(to);
    balance += SEE_VALUES[captured_piece];
    if (balance < 0) {
        return false;
    }

    balance -= SEE_VALUES[chessboard.get_piece(from)];
    if (balance >= 0) {
        return true;
    }

    int side_to_capture = 1 - color;

    const std::uint64_t queens = chessboard.get_pieces(White, Queen) | chessboard.get_pieces(Black, Queen);
    const std::uint64_t rooks = chessboard.get_pieces(White, Rook) | chessboard.get_pieces(Black, Rook) | queens;
    const std::uint64_t bishops = chessboard.get_pieces(White, Bishop) | chessboard.get_pieces(Black, Bishop) | queens;

    std::uint64_t occupancy = chessboard.get_occupancy() ^ (1ull << from) ^ (1ull << to);

    std::uint64_t attackers = chessboard.attackers<White>(to, occupancy) | chessboard.attackers<Black>(to, occupancy);

    std::uint64_t side_occupancy[2] = { chessboard.get_side_occupancy<White>(), chessboard.get_side_occupancy<Black>() };

    Piece piece;
    while (true) {
        attackers &= occupancy;

        //attackers &= side_occupancy[side_to_capture];
        if ((attackers & side_occupancy[side_to_capture]) == 0ull) {
            break;
        }

        for (Piece pt : {Pawn, Knight, Bishop, Rook, Queen, King}) {
            std::uint64_t bitboard = attackers & chessboard.get_pieces(side_to_capture, pt);
            if (bitboard) {
                occupancy ^= (1ull << lsb(bitboard));
                piece = pt;
                break;
            }
        }

        balance = -balance - 1 - SEE_VALUES[piece];

        side_to_capture = 1 - side_to_capture;

        if (balance >= 0) {
            if (piece == King && (attackers & side_occupancy[side_to_capture])) {
                side_to_capture = 1 - side_to_capture;
            }
            break;
        }


        if (piece == Pawn || piece == Bishop || piece == Queen) {
            attackers |= (attacks<Ray::BISHOP>(to, occupancy) & bishops);
        }

        if (piece == Rook || piece == Queen) {
            attackers |= (attacks<Ray::ROOK>(to, occupancy) & rooks);
        }
    }

    return color != side_to_capture;
}



void see_test_suite() {
    auto convertValue = [](int value) { return value >= 0; };

    std::vector<std::tuple<std::string, std::string, int, bool>> see_data = {
        {"6k1/1pp4p/p1pb4/6q1/3P1pRr/2P4P/PP1Br1P1/5RKN w - -", "f1f4", -100, convertValue(-100)},
        {"5rk1/1pp2q1p/p1pb4/8/3P1NP1/2P5/1P1BQ1P1/5RK1 b - -", "d6f4", 0, convertValue(0)},
        {"4R3/2r3p1/5bk1/1p1r3p/p2PR1P1/P1BK1P2/1P6/8 b - -", "h5g4", 0, convertValue(0)},
        {"4R3/2r3p1/5bk1/1p1r1p1p/p2PR1P1/P1BK1P2/1P6/8 b - -", "h5g4", 0, convertValue(0)},
        {"4r1k1/5pp1/nbp4p/1p2p2q/1P2P1b1/1BP2N1P/1B2QPPK/3R4 b - -", "g4f3", 0, convertValue(0)},
        {"2r1r1k1/pp1bppbp/3p1np1/q3P3/2P2P2/1P2B3/P1N1B1PP/2RQ1RK1 b - -", "d6e5", 100, convertValue(100)},
        {"7r/5qpk/p1Qp1b1p/3r3n/BB3p2/5p2/P1P2P2/4RK1R w - -", "e1e8", 0, convertValue(0)},
        {"6rr/6pk/p1Qp1b1p/2n5/BB3p2/5p2/P1P2P2/4RK1R w - -", "e1e8", -500, convertValue(-500)},
        {"7r/5qpk/2Qp1b1p/1N1r3n/BB3p2/5p2/P1P2P2/4RK1R w - -", "e1e8", -500, convertValue(-500)},
        {"6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -", "f7f8q", 200, convertValue(200)},
        {"6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -", "f7f8n", 200, convertValue(200)},
        {"7R/5P2/8/8/6r1/3K4/5p2/4k3 w - -", "f7f8q", 800, convertValue(800)},
        {"7R/5P2/8/8/6r1/3K4/5p2/4k3 w - -", "f7f8b", 200, convertValue(200)},
        {"7R/4bP2/8/8/1q6/3K4/5p2/4k3 w - -", "f7f8r", -100, convertValue(-100)},
        {"8/4kp2/2npp3/1Nn5/1p2PQP1/7q/1PP1B3/4KR1r b - -", "h1f1", 0, convertValue(0)},
        {"8/4kp2/2npp3/1Nn5/1p2P1P1/7q/1PP1B3/4KR1r b - -", "h1f1", 0, convertValue(0)},
        {"2r2r1k/6bp/p7/2q2p1Q/3PpP2/1B6/P5PP/2RR3K b - -", "c5c1", 100, convertValue(100)},
        {"r2qk1nr/ppp1ppbp/2b3p1/2p1p3/8/2N2N2/PPPP1PPP/R1BQR1K1 w kq -", "f3e5", 100, convertValue(100)},
        {"6r1/4kq2/b2p1p2/p1pPb3/p1P2B1Q/2P4P/2B1R1P1/6K1 w - -", "f4e5", 0, convertValue(0)},
        {"3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R4B/PQ3P1P/3R2K1 w - h6", "g5h6", 0, convertValue(0)},
        {"3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R1B2B/PQ3P1P/3R2K1 w - h6", "g5h6", 100, convertValue(100)},
        {"2r4r/1P4pk/p2p1b1p/7n/BB3p2/2R2p2/P1P2P2/4RK2 w - -", "c3c8", 500, convertValue(500)},
        {"2r5/1P4pk/p2p1b1p/5b1n/BB3p2/2R2p2/P1P2P2/4RK2 w - -", "c3c8", 500, convertValue(500)},
        {"2r4k/2r4p/p7/2b2p1b/4pP2/1BR5/P1R3PP/2Q4K w - -", "c3c5", 300, convertValue(300)},
        {"8/pp6/2pkp3/4bp2/2R3b1/2P5/PP4B1/1K6 w - -", "g2c6", -200, convertValue(-200)},
        {"4q3/1p1pr1k1/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -", "e6e4", -400, convertValue(-400)},
        {"4q3/1p1pr1kb/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -", "h7e4", 100, convertValue(100)},
        {"3r3k/3r4/2n1n3/8/3p4/2PR4/1B1Q4/3R3K w - -", "d3d4", -100, convertValue(-100)},
        {"1k1r4/1ppn3p/p4b2/4n3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -", "d3e5", 100, convertValue(100)},
        {"1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -", "d3e5", -200, convertValue(-200)},
        {"rnb2b1r/ppp2kpp/5n2/4P3/q2P3B/5R2/PPP2PPP/RN1QKB2 w Q -", "h4f6", 100, convertValue(100)},
        {"r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 b - -", "g4f3", 0, convertValue(0)},
        {"r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq -", "c6d4", 0, convertValue(0)},
        {"r1bq1r2/pp1ppkbp/4N1p1/n3P1B1/8/2N5/PPP2PPP/R2QK2R w KQ -", "e6g7", 0, convertValue(0)},
        {"r1bq1r2/pp1ppkbp/4N1pB/n3P3/8/2N5/PPP2PPP/R2QK2R w KQ -", "e6g7", 300, convertValue(300)},
        {"rnq1k2r/1b3ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq -", "d6h2", -200, convertValue(-200)},
        {"rn2k2r/1bq2ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq -", "d6h2", 100, convertValue(100)},
        {"r2qkbn1/ppp1pp1p/3p1rp1/3Pn3/4P1b1/2N2N2/PPP2PPP/R1BQKB1R b KQq -", "g4f3", 100, convertValue(100)},
        {"rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/P1N5/1PQ1PPPP/R1B1KBNR b KQ -", "b4c3", 0, convertValue(0)},
        {"r4rk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - -", "b6b2", -800, convertValue(-800)},
        {"r4rk1/1q1nppbp/b2p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - -", "f6d5", -200, convertValue(-200)},
        {"1r3r2/5p2/4p2p/2k1n1P1/2PN1nP1/1P3P2/8/2KR1B1R b - -", "b8b3", -400, convertValue(-400)},
        {"1r3r2/5p2/4p2p/4n1P1/2PN1nP1/1P3P2/8/2KR1B1R b - -", "b8b4", 100, convertValue(100)},
        {"2r2rk1/5pp1/pp5p/q2p4/P3n3/1Q3NP1/1P2PP1P/2RR2K1 b - -", "c8c1", 0, convertValue(0)},
        {"5rk1/5pp1/2r4p/5b2/2R5/6Q1/R1P1qPP1/5NK1 b - -", "f5c2", -100, convertValue(-100)},
        {"1r3r1k/p4pp1/2p1p2p/qpQP3P/2P5/3R4/PP3PP1/1K1R4 b - -", "a5a2", -800, convertValue(-800)},
        {"1r5k/p4pp1/2p1p2p/qpQP3P/2P2P2/1P1R4/P4rP1/1K1R4 b - -", "a5a2", 100, convertValue(100)},
        {"r2qk1nr/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 w kq -", "d5e7", 0, convertValue(0)},
        {"rnbqrbn1/pp3ppp/3p4/2p2k2/4p3/3B1K2/PPP2PPP/RNB1Q1NR w - -", "d3e4", 100, convertValue(100)},
        {"r1b1k2r/p3p1pp/1p3p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq -", "f5e4", 0, convertValue(0)},
        {"2r5/p1p1b1pk/3pp1np/q1p5/2P5/P1N1P2P/1P1B1PP1/R2QK2R w KQ -", "e3c5", -300, convertValue(-300)},
        {"r2qkbnr/pppb2pp/2npp3/5p2/2B1P3/2NP1N2/PPP2PPP/R1BQK2R w KQkq -", "d2d4", 0, convertValue(0)},
        {"1r3r2/5p2/4p2p/3kn1P1/2PN1nP1/1P3P2/8/2KR1B1R b - -", "b8b3", 0, convertValue(0)},
        {"1r3r2/5p2/4p2p/3kn1P1/2PN1nP1/1P3P2/8/2KR1B1R b - -", "b8b4", -300, convertValue(-300)},
        {"1r3r2/5p2/4p2p/3kn1P1/2PN1nP1/1P3P2/8/2KR1B1R b - -", "b8b4", -300, convertValue(-300)},
        {"1r3r2/5p2/4p2p/3kn1P1/2PN1nP1/1P3P2/8/2KR1B1R b - -", "b8b4", -300, convertValue(-300)},
        {"1r3r2/5p2/4p2p/3kn1P1/2PN1nP1/1P3P2/8/2KR1B1R b - -", "b8b4", -300, convertValue(-300)}
    };

    int counter = 0;
    std::vector<std::string> errors;
    for (auto [fen, move_string, value, good_capture] : see_data) {
        board b(fen);
        move_list ml;
        if (b.get_side() == White) {
            generate_all_moves<White, false>(b, ml);
        }
        else {
            generate_all_moves<Black, false>(b, ml);
        }
        
        for (const auto& move : ml) {
            if (move.to_string() == move_string) {
                bool result;
                if (b.get_side() == White) {
                    result = see<White>(b, move);
                    
                }
                else {
                    result = see<Black>(b, move);
                }
                if (good_capture != result) {
                    counter += 1;
                    errors.push_back(fen);
                }
                std::cout << fen << " | " << move_string << " | " << result << " | " << good_capture << std::endl;
            }
        }
    }
    std::cout << "FAULTS: " << counter << std::endl;

    for (const auto& fen : errors) {
        std::cout << fen << std::endl;
    }
}

#endif //MOTOR_SEE_HPP
