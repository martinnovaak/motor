#ifndef MOVEGEN_MOVELIST_H
#define MOVEGEN_MOVELIST_H

#include "move_t.h"
#include <utility>

class movelist {
private:
    std::array<move_t, 256> list;
    size_t count;
public:
    using const_iterator = typename std::array<move_t, 256>::const_iterator;

    constexpr movelist() : count(0) {}

    constexpr size_t size() const {
        return count;
    }

    constexpr void add(move_t && m) {
        list[count] = m;
        count++;
    }

    constexpr void clear() {
        count = 0;
    }

    constexpr move_t& operator[](uint32_t index) {
        return list[index];
    }

    constexpr const_iterator begin() const {
        return list.begin();
    }

    constexpr const_iterator end() const {
        return std::next(list.begin(), count);
    }
};

#endif //MOVEGEN_MOVELIST_H
