#ifndef MOTOR_MOVE_LIST_HPP
#define MOTOR_MOVE_LIST_HPP

#include <utility>
#include "../chess_board/chess_move.hpp"

class move_list {
private:
    std::array<chess_move, 218> list;
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
            if(list[i] > list[best]) {
                best = i;
            }
        }
        std::swap(list[index], list[best]);
        return list[index];
    }

    const chess_move & operator[](int index) {
        return list[index];
    }

    [[nodiscard]] iterator begin() {
        return list.begin();
    }
    [[nodiscard]] iterator end() {
        return list.begin() + count;
    }
};

#endif //MOTOR_MOVE_LIST_HPP