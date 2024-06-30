#ifndef MOTOR_MOVE_LIST_HPP
#define MOTOR_MOVE_LIST_HPP

#include <utility>
#include "../chess_board/chess_move.hpp"

class move_list {
private:
    std::array<chess_move, 256> list;
    std::array<std::int32_t, 256> move_score;
    std::uint8_t count;
public:
    move_list() : list{}, count{} {}

    [[nodiscard]] std::uint8_t size() const {
        return count;
    }

    void push_back(chess_move && m) {
        list[count] = m;
        count++;
    }

    void push_back(const chess_move & m) {
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

    std::int32_t get_move_score(int index) {
        return move_score[index];
    }

    typedef chess_move* iterator;
    typedef const chess_move* const_iterator;

    iterator begin() { return &list[0]; }
    const_iterator begin() const { return &list[0]; }
    iterator end() { return &list[count]; }
    const_iterator end() const { return &list[count]; }
};

#endif //MOTOR_MOVE_LIST_HPP