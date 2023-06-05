#ifndef MOTOR_SEARCH_H
#define MOTOR_SEARCH_H

#include "eval.h"
#include "movegen.h"
#include "stopwatch_t.h"

constexpr int MAX_DEPTH = 64;

struct search_info {
    int wtime = -1, btime = -1, winc = 0, binc = 0, movestogo = 0, movetime = -1;
    bool infinite = false;
};

struct search_data {
    int ply = 0;
    int nodes_searched = 0;
    move_t killer_moves[MAX_DEPTH][2] = {};
    int history_moves[64][64] = {};
    int pv_length[MAX_DEPTH] = {};
    move_t pv_table[MAX_DEPTH][MAX_DEPTH] = {};
    int square_of_last_move = N_SQUARES;
    bool follow_pv = false, score_pv = false;

    void update_killer(move_t move) {
        killer_moves[ply][0] = killer_moves[ply][1];
        killer_moves[ply][1] = move;
    }

    void update_history(move_t move, int depth) {
        history_moves[move.get_from()][move.get_to()] = depth;
    }

    void update_pv(move_t move, int depth) {
        pv_table[ply][ply] = move;
        for (int next_ply = ply + 1; next_ply < pv_length[ply + 1]; next_ply++) {
            pv_table[ply][next_ply] = pv_table[ply + 1][next_ply];
        }
        pv_length[ply] = pv_length[ply + 1];
    }

    std::string best_move() const {
        return pv_table[0][0].to_string();
    }
};

constexpr int INF = 50'000;

//                      [victim][attacker]
constexpr static int mvv_lva[6][6] = {
//attacker:   P,  N,  B,  R,  Q, NONE
            { 5,  4,  3,  2,  1,  0},    // victim PAWN
            {10,  9,  8,  7,  6,  0},   // victim KNIGHT
            {15, 14, 13, 12, 11,  0},    // victim BISHOP
            {20, 19, 18, 17, 16,  0},    // victim ROOK
            {25, 24, 23, 22, 21,  0},    // victim QUEEN
            { 0,  0,  0,  0,  0,  0},    // victim NONE
};


static void score_moves(movelist & ml, const search_data & data) {
    for(move_t & move : ml) {
        Square from = move.get_from();
        Square to   = move.get_to();

        if(move.is_capture()) {
            if(to == data.square_of_last_move) {
                move.set_score(360 + mvv_lva[move.get_captured_piece()][move.get_piece()]);
            } else {
                move.set_score(320 + mvv_lva[move.get_captured_piece()][move.get_piece()]);
            }
        } else if (data.killer_moves[data.ply][0] == move) {
            move.set_score(310);
        } else if (data.killer_moves[data.ply][1] == move) {
            move.set_score(300);
        } else {
            move.set_score(data.history_moves[from][to]);
        }
    }
}

static void sort_moves(movelist& ml, const search_data& data) {
    score_moves(ml, data);
    std::vector<move_t> moves(ml.begin(), ml.end());
    std::sort(moves.begin(), moves.end(), [](const move_t & a, const move_t & b){return a.m_move > b.m_move;});
    for(unsigned int i = 0; i < ml.size(); i++) {
        ml[i] = moves[i];
    }
}

static int quiescence(board & chessboard, int alpha, int beta, search_data & data, stopwatch_t & stopwatch) {
    if(stopwatch.should_end(data.nodes_searched)) {
        return alpha;
    }
    data.nodes_searched++;

    int stand_pat = evaluate(chessboard);  // Evaluate the current position statically

    if(data.ply >= MAX_DEPTH) {
        return stand_pat;
    }

    // Check if the stand-pat score is already better than the alpha value
    if (stand_pat >= beta) {
        return beta;
    }

    if (stand_pat > alpha) {
        alpha = stand_pat;
    }

    movelist ml;
    generate_captures(chessboard, ml);

    sort_moves(ml, data);

    for (const move_t& m : ml) {
        chessboard.make_move(m);
        data.ply++;
        int score = -quiescence(chessboard, -beta, -alpha, data, stopwatch);
        data.ply--;
        chessboard.undo_move();

        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    return alpha;
}

int alphabeta(board& chessboard, int alpha, int beta, search_data & data, stopwatch_t & stopwatch, int depth) {
    if(stopwatch.should_end(data.nodes_searched)) {
        return alpha;
    }
    if (data.ply >= MAX_DEPTH ) {
        return evaluate(chessboard);
    }
    
    data.pv_length[data.ply] = data.ply;

    if (depth == 0) {
        return quiescence(chessboard, alpha, beta, data, stopwatch);
    }

    data.nodes_searched++;

    bool in_check = chessboard.in_check();

    if(depth >= 3 && !in_check && data.ply) {
        int R = 2;
        Square enpassant = chessboard.make_null_move();
        int score = -alphabeta(chessboard, -beta, -beta + 1, data, stopwatch, depth - 1 - R);
        chessboard.unmake_null_move(enpassant);
        if(score >= beta) {
            return beta;
        }
    }

    movelist ml;
    generate_moves(chessboard, ml);

    if(ml.size() == 0) {
        if(in_check) {
            return -INF + data.ply;
        } else {
            return 0;
        }
    }

    if(in_check) {
        depth++;
    }

    sort_moves(ml, data);

    for (const move_t& m : ml) {
        chessboard.make_move(m);
        data.square_of_last_move = m.get_from();
        data.ply++;

        int score = -alphabeta(chessboard, -beta, -alpha, data, stopwatch, depth - 1);

        chessboard.undo_move();
        data.ply--;
        if (score >= beta) {
            if (!m.is_capture()) {
                data.update_killer(m);
            }
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            if(!m.is_capture()) {
                data.update_history(m, depth);
            }
            data.update_pv(m, depth);
        }
    }

    return alpha;
}

void iterative_deepening(board& chessboard, search_data & data, stopwatch_t & stopwatch) {
    std::string best_move;
    for(int current_depth = 1; current_depth <= MAX_DEPTH; current_depth++) {
        int score = alphabeta(chessboard, -INF, INF, data, stopwatch, current_depth);

        if(stopwatch.stopped()) {
            break;
        }

        std::cout << "info score cp " << score << " depth " << current_depth << " nodes " << data.nodes_searched << " pv ";
        for(int i = 0; i < data.pv_length[0]; i++) {
            std::cout << data.pv_table[0][i].to_string() << " ";
        }
        std::cout << "\n";
        best_move = data.best_move();
    }
    std::cout << "bestmove " << best_move << "\n";
}

void find_best_move(board & chessboard, search_info & info) {
    search_data data;
    stopwatch_t stopwatch;
    if(chessboard.get_side() == WHITE) {
        stopwatch.reset(info.wtime, info.winc, info.movestogo, info.movetime);
    } else {
        stopwatch.reset(info.btime, info.binc, info.movestogo, info.movetime);
    }
    iterative_deepening(chessboard, data, stopwatch);
}

void find_best_move(board & chessboard, int depth) {
    search_data data;
    stopwatch_t stopwatch;
    alphabeta(chessboard, -INF, INF, data, stopwatch, depth);
    std::cout << "bestmove " << data.best_move() << "\n";
}

#endif //MOTOR_SEARCH_H
