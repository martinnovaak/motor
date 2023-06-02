#ifndef MOTOR_SEARCH_H
#define MOTOR_SEARCH_H

#include "eval.h"
#include "movegen.h"

constexpr int MAX_DEPTH = 64;
constexpr int INF = 50'000;

struct search_info {
    int wtime = -1, btime = -1, winc = 0, binc = 0, movestogo = 0, movetime = -1;
    int max_depth = 100;
    bool ucimode = true;
    bool infinite = false;
};

struct search_data {
    int ply = 0;
    int nodes_searched = 0;
    int pv_length[MAX_DEPTH] = {};
    move_t pv_table[MAX_DEPTH][MAX_DEPTH] = {};

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

int alphabeta(board& chessboard, int alpha, int beta, search_data & data, int depth) {
    if (data.ply >= MAX_DEPTH) {
        return evaluate(chessboard);
    }

    data.pv_length[data.ply] = data.ply;

    if (depth == 0) {
        //return quiescence(chessboard, alpha, beta, data);
        return evaluate(chessboard);
    }

    data.nodes_searched++;

    bool in_check = chessboard.in_check();

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

    for (const move_t& m : ml) {
        chessboard.make_move(m);
        data.ply++;

        int score = -alphabeta(chessboard, -beta, -alpha, data, depth - 1);

        chessboard.undo_move();
        data.ply--;
        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            data.update_pv(m, depth);
        }
    }

    return alpha;
}

void find_best_move(board & chessboard, search_info & info) {
    search_data data;
    alphabeta(chessboard, -INF, INF, data, 5);
    std::cout << "bestmove " << data.best_move() << "\n";
}

void find_best_move(board & chessboard, int depth) {
    search_data data;
    alphabeta(chessboard, -INF, INF, data, depth);
    std::cout << "bestmove " << data.best_move() << "\n";
}

#endif //MOTOR_SEARCH_H
