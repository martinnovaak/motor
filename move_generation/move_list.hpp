#ifndef MOTOR_MOVE_LIST_HPP
#define MOTOR_MOVE_LIST_HPP

#include <utility>
#include "../chess_board/chess_move.hpp"

class move_list {
private:
    std::array<chess_move, 256> list;
    std::uint8_t count;
public:
    using const_iterator = typename std::array<chess_move, 256>::const_iterator;

    move_list() : list{}, count{} {}

    [[nodiscard]] std::uint8_t size() const {
        return count;
    }

    void add(chess_move m) {
        list[count] = m;
        count++;
    }

    chess_move & operator[](uint32_t index) {
        return list[index];
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

    [[nodiscard]] const_iterator begin() const {
        return list.begin();
    }
    [[nodiscard]] const_iterator end() const {
        return std::next(list.begin(), count);
    }
};

#endif //MOTOR_MOVE_LIST_HPP
