#ifndef MOVEGEN_MOVELIST_H
#define MOVEGEN_MOVELIST_H

#include "move_t.h"

class movelist {
private:
    move_t list[218];
    unsigned int count;
public:
    movelist() : list{}, count(0) {}
    unsigned int size() const {return count;}
    void add(move_t && m) {list[count++] = m;}
    void add(const move_t & m) {list[count++] = m;}
    void clear() {count = 0;}
    move_t& operator[](uint32_t index) {
        uint32_t best = index;
        for(unsigned int i = index + 1; i < count; i++) {
            if(list[i].m_move > list[best].m_move) {
                best = i;
            }
        }
        std::swap(list[index], list[best]);
        return list[index];
    }
    move_t * begin() {return std::begin(list);}
    move_t * end() {return std::next(std::begin(list), count);}
    //move_t * begin() {return list;}
    //move_t * end() {return list + count;};
};

#endif //MOVEGEN_MOVELIST_H
