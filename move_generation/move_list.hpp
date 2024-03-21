#ifndef MOTOR_MOVE_LIST_HPP
#define MOTOR_MOVE_LIST_HPP

#include <utility>
#include "../chess_board/chess_move.hpp"

class move_list {
private:
    std::array<chess_move, 218> list;
    std::array<std::int32_t, 218> move_score;
    std::uint8_t count;
public:
    using iterator = typename std::array<chess_move, 218>::iterator;

    move_list() : list{}, count{} {}

    [[nodiscard]] std::uint8_t size() const {
        return count;
    }

    void add(chess_move m) {
        list[count] = m;
        count++;
    }

    // partial insertion sort
    chess_move & get_next_move(const std::uint8_t index) {
        uint8_t best = index;
        for(unsigned int i = index + 1; i < count; i++) {
            if(move_score[i] > move_score[best]) {
                best = i;
            }
        }
        std::swap(list[index], list[best]);
        std::swap(move_score[index], move_score[best]);
        return list[index];
    }

    std::int32_t & operator[](int index) {
        return move_score[index];
    }

    [[nodiscard]] iterator begin() {
        return list.begin();
    }
    [[nodiscard]] iterator end() {
        return list.begin() + count;
    }
};

#endif //MOTOR_MOVE_LIST_HPP