#ifndef MOTOR_MOVE_LIST_HPP
#define MOTOR_MOVE_LIST_HPP

#include <array>
#include <utility>
#include "../chess/chessmove.hpp"


class movelist {
private:
    std::array<std::pair<chessmove, std::int32_t>, 256> pseudolegal_moves;
    std::uint8_t count;
public:
    using iterator = typename std::array<std::pair<chessmove, std::int32_t>, 256>::iterator;

    movelist() : pseudolegal_moves{}, count{} {}

    [[nodiscard]] int size() const {
        return count;
    }

    void add(chessmove m) {
        pseudolegal_moves[count].first = m;
        count++;
    }

    // partial insertion sort
    [[nodiscard]] std::pair<chessmove, std::int32_t> & get_next_move(const std::uint8_t index) {
        std::uint8_t best = index;
        for (unsigned int i = index + 1; i < count; i++) {
            if (pseudolegal_moves[i].second > pseudolegal_moves[best].second) {
                best = i;
            }
        }
        std::swap(pseudolegal_moves[index], pseudolegal_moves[best]);
        return pseudolegal_moves[index];
    }

    [[nodiscard]] iterator begin() {
        return pseudolegal_moves.begin();
    }

    [[nodiscard]] iterator end() {
        return std::next(pseudolegal_moves.begin(), count);
    }
};

#endif //MOTOR_MOVE_LIST_HPP