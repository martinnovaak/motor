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

    const_iterator begin() const {
        return list.begin();
    }
    const_iterator end() const {
        return std::next(list.begin(), count);
    }
};

#endif //MOTOR_MOVE_LIST_HPP
