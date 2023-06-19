#ifndef MOTOR_SEARCH_H
#define MOTOR_SEARCH_H

#include "movegen.h"
#include "stopwatch_t.h"
#include "transposition_table.h"
#include "eval.h"
#include <iostream>

constexpr int MAX_DEPTH = 64;
constexpr int full_depth_moves = 4;
constexpr int reduction_limit = 3;

enum node_type : int {
    PV_node, NON_PV_node, ROOT_node
};

struct search_info {
    int wtime = -1, btime = -1, winc = 0, binc = 0, movestogo = 0, movetime = -1;
    bool infinite = false;
};

struct search_data {
    int ply = 0;
    uint64_t nodes_searched = 0;
    move_t killer_moves[MAX_DEPTH][2] = {};
    move_t counter_moves[64][64] = {};
    int history_moves[64][64] = {};
    int pv_length[MAX_DEPTH] = {};
    move_t pv_table[MAX_DEPTH][MAX_DEPTH] = {};
    int square_of_last_move = N_SQUARES;
    //int eval_grandfather;
    //int eval_father;
    move_t previous_move;

    void update_killer(move_t move) {
        killer_moves[ply][0] = killer_moves[ply][1];
        killer_moves[ply][1] = move;
    }

    void update_history(move_t move, int depth) {
        history_moves[move.get_from()][move.get_to()] += depth * depth;
    }

    void reduce_history(move_t move, int depth) {
        history_moves[move.get_from()][move.get_to()] -= depth;
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
constexpr int MATE = INF - MAX_DEPTH;

//                      [victim][attacker]
constexpr static int mvv_lva[6][6] = {
//attacker:  P, N,  B,  R,  Q,  NONE
        {8,  4, 3, 2, 1, 26},    // victim PAWN
        {12, 11, 10, 9, 6, 27},   // victim KNIGHT
        {16, 15, 14, 13, 7, 28},    // victim BISHOP
        {20, 19, 18, 17, 8, 29},    // victim ROOK
        {25, 24, 23, 22, 21, 30},    // victim QUEEN
        {0,  0,  0,  0,  0,  0},    // victim NONE
};


static void score_moves(movelist & ml, const search_data & data, move_t tt_move) {
    for(move_t & move : ml) {
        Square from    = move.get_from();
        Square to    = move.get_to();

        if(tt_move == move) {
            move.set_score(100'000);
        } else if(move.is_capture()) {
            if(to == data.square_of_last_move) {
                move.set_score(90'000 + mvv_lva[move.get_captured_piece()][move.get_piece()]);
            } else {
                move.set_score(85'000 + mvv_lva[move.get_captured_piece()][move.get_piece()]);
            }
        } else if (move.is_promotion()) {
            move.set_score(80'000);
        } else if (data.killer_moves[data.ply][0] == move) {
            move.set_score(70'000);
        } else if (data.killer_moves[data.ply][1] == move) {
            move.set_score(60'000);
        } else if (data.counter_moves[data.previous_move.get_from()][data.previous_move.get_to()] == move) {
            move.set_score(50'000);
        } else {
            move.set_score(data.history_moves[from][to]);
        }
    }
}

static void sort_moves(movelist& ml, const search_data& data, move_t tt_move) {
    score_moves(ml, data, tt_move);
    std::vector<move_t> moves(ml.begin(), ml.end());
    std::sort(moves.begin(), moves.end(), [](const move_t & a, const move_t & b){return a.m_score > b.m_score;});
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

    // Check if the stand-pat score is already better than the alpha value
    if (stand_pat >= beta) {
        return beta;
    }

    if (stand_pat > alpha) {
        alpha = stand_pat;
    }

    movelist ml;

    generate_captures(chessboard, ml);

    sort_moves(ml, data, {});

    for (const move_t& m : ml) {
        chessboard.make_move(m);
        data.square_of_last_move = m.get_from();
        data.ply++;
        int score = -quiescence(chessboard, -beta, -alpha, data, stopwatch);
        data.ply--;
        chessboard.undo_move();

        if (score >= beta) {
            return beta;
        }
        alpha = std::max(score, alpha);
    }

    return alpha;
}

template <node_type nodetype>
int alphabeta(board& chessboard, int alpha, int beta, search_data & data, stopwatch_t & stopwatch, int depth) {
    constexpr bool is_pv = nodetype != NON_PV_node;
    constexpr bool is_root = nodetype == ROOT_node;

    if(stopwatch.should_end(data.nodes_searched)) {
        return alpha;
    }

    data.pv_length[data.ply] = data.ply;

    data.nodes_searched++;

    int old_alpha = alpha;
    int eval = evaluate(chessboard);

    if constexpr (!is_root) {
        if (depth <= 0) {
            return quiescence(chessboard, alpha, beta, data, stopwatch);
        }

        if (chessboard.is_draw()) {
            return 0;
        }

        if (data.ply >= MAX_DEPTH ) {
            return eval;
        }

        // Mate distance pruning
        alpha = std::max(-INF + data.ply, alpha);
        beta  = std::min( INF + data.ply, beta);
        if(alpha >= beta) {
            return alpha;
        }
    }

    bool in_check = chessboard.in_check();

    BOUND flag = BOUND::UPPER;
    move_t best_move = {};

    uint64_t key = chessboard.get_hash_key();

    // Transposition table
    bool tt_hit = false;
    const tt_info & tt_info_t = ttable.probe(key);
    if (tt_info_t.type != BOUND::INVALID && tt_info_t.hash_key == key) {
        best_move = tt_info_t.best_move;
        tt_hit = true;
    } else if(depth >= 4) {
        // Internal iterative deepening
        if constexpr (is_pv) {
            alphabeta<PV_node>(chessboard, alpha, beta, data, stopwatch, depth - 3);
            best_move = tt_info_t.best_move;
        } else {
            depth--;
        }
    }

    if constexpr (!is_root) {
        if(!in_check && !tt_hit) {
            // razoring
            if (depth <= 3 && eval + 200 * depth <= alpha) {
                eval = quiescence(chessboard, alpha, beta, data, stopwatch);
                if(eval <= alpha) {
                    return eval;
                }
            }

            if constexpr (!is_pv) {
                //bool improving = (data.ply >= 2 && eval >= data.eval_grandfather);

                // reverse futility pruning
                if(depth < 7 && eval - 50 * depth  /*+ improving * 80*/  >= beta) {
                    return beta;
                }

                // NULL MOVE PRUNING
                if (data.ply && depth >= 3 && eval >= beta && !chessboard.pawn_endgame()) {
                    Square enpassant = chessboard.make_null_move();
                    int R = 3 + depth / 4;
                    data.ply++;
                    int nullmove_score = -alphabeta<NON_PV_node>(chessboard, -beta, -beta + 1, data, stopwatch, depth - R);
                    data.ply--;
                    chessboard.unmake_null_move(enpassant);
                    if (nullmove_score >= beta) {
                        return nullmove_score;
                    }
                }
            }
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

    if constexpr (is_root) {
        // if we have only one legal move in root search end the search
        if (ml.size() == 1) {
            data.update_pv(ml[0], depth);
            return eval;
            stopwatch.stop_timer();
        }
    }

    if constexpr (!is_root) {
        // check extension
        if (in_check) {
            depth++;
        }
    }

    eval = -INF;

    sort_moves(ml, data,  best_move);
    int moves_searched = 0;
    //int grandfather = data.eval_father;
    move_t previous_move = data.previous_move;
    for (const move_t& m : ml) {
        chessboard.make_move(m);

        if constexpr (!is_pv) {
            if(!in_check && m.is_quiet() && eval > -49'000) {
                if (depth <= 4 && moves_searched >= 5 + depth * depth ) {
                    chessboard.undo_move();
                    continue;
                }
            }
        }

        data.square_of_last_move = m.get_from();
        data.ply++;
        //data.eval_grandfather = grandfather;
        //data.eval_father = eval;
        data.previous_move = m;

        // Principal variation search
        int score;
        if(moves_searched == 0) {
            score = -alphabeta<PV_node>(chessboard, -beta, -alpha, data, stopwatch, depth - 1);
        } else {
            // late move reduction
            if(moves_searched >= full_depth_moves && depth >= reduction_limit && !is_pv && !in_check && !chessboard.in_check() && m.is_quiet()) {
                int reduction = data.ply > 7 ? 3 : 2;
                score = -alphabeta<NON_PV_node>(chessboard, -alpha - 1, -alpha, data, stopwatch, depth - reduction);
            } else {
                score = alpha + 1;
            }

            if(score > alpha) {
                score = -alphabeta<NON_PV_node>(chessboard, -alpha - 1, -alpha, data, stopwatch, depth - 1);
                if(score > alpha && score < beta) {
                    score = -alphabeta<PV_node>(chessboard, -beta, -alpha, data, stopwatch, depth - 1);
                }
            }
        }

        chessboard.undo_move();
        moves_searched++;
        data.ply--;

        if(score > eval) {
            eval = score;
            best_move = m;
            data.update_pv(m, depth);
        }

        if (score >= beta) {
            if (m.is_quiet()) {
                data.update_killer(m);
                if constexpr (!is_root) {
                    data.counter_moves[previous_move.get_from()][previous_move.get_to()] = m;
                }
                data.update_history(m, depth);
            }

            ttable.insert(key, beta, best_move, depth, BOUND::LOWER);
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            flag = BOUND::EXACT;
        }
    }

    ttable.insert(key, alpha, best_move, depth, flag);
    return alpha;
}

int aspiration_window(board & chessboard, int score, search_data & data, stopwatch_t & stopwatch, int depth) {
    int alpha = std::max(-INF, score - 50);
    int beta = std::min(INF, score + 50);

    score = alphabeta<ROOT_node>(chessboard, alpha, beta, data, stopwatch, depth);

    if(score <= alpha || score >= beta) {
        score = alphabeta<ROOT_node>(chessboard, -INF, INF, data, stopwatch, depth);
    }

    return score;
}

void iterative_deepening(board& chessboard, search_data & data, stopwatch_t & stopwatch) {
    std::string best_move;
    int score;
    for(int current_depth = 1; current_depth <= MAX_DEPTH; current_depth++) {
        data.ply = 0;
        data.nodes_searched = 0;
        //score = alphabeta<ROOT_node>(chessboard, -INF, INF, data, stopwatch, current_depth);

        if(current_depth < 6) {
            score = alphabeta<ROOT_node>(chessboard, -INF, INF, data, stopwatch, current_depth);
        } else {
            score = aspiration_window(chessboard, score, data, stopwatch, current_depth);
        }


        if(stopwatch.stopped()) {
            break;
        }

        std::cout << "info score cp " << score
                  << " depth " << current_depth
                  << " nodes " << data.nodes_searched
                  << " nps "   << stopwatch.NPS(data.nodes_searched)
                  << " pv ";
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
    alphabeta<ROOT_node>(chessboard, -INF, INF, data, stopwatch, depth);
    std::cout << "bestmove " << data.best_move() << "\n";
}

#endif //MOTOR_SEARCH_H