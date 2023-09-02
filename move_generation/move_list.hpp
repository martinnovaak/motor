#ifndef MOTOR_MOVE_LIST_HPP
#define MOTOR_MOVE_LIST_HPP

#include <utility>
#include <iterator>
#include "../chess_board/chess_move.hpp"

class move_list {
private:
    chess_move list[218];
    std::uint8_t count;
public:
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

    chess_move * begin() {return std::begin(list);}
    chess_move * end() {return std::next(std::begin(list), count);}
};

#endif //MOTOR_MOVE_LIST_HPP
