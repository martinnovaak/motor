#ifndef MOVEGEN_MOVELIST_H
#define MOVEGEN_MOVELIST_H

#include "move_t.h"
#include <array>

class movelist {
private:
    std::array<move_t, 256> list;
    size_t count;
public:
    using const_iterator = typename std::array<move_t, 256>::const_iterator;

    movelist() : count(0) {}

    size_t size() const {
        return count;
    }

    void add(move_t m) {
        list[count] = m;
        count++;
    }

    void clear() {
        count = 0;
    }

    move_t& operator[](uint32_t index) {
        return list[index];
    }

    const_iterator begin() const {
        return list.begin();
    }

    const_iterator end() const {
        return std::next(list.begin(), count);
    }
};
#endif //MOVEGEN_MOVELIST_H
