#ifndef MOTOR_MOVELIST_H
#define MOTOR_MOVELIST_H

#include "move.h"

class movelist {
private:
    move list[256];
    uint32_t count;
public:
    class iterator {
    private:
        move* ptr;
    public:
        iterator(move* p) : ptr(p) {}
        iterator& operator++() {++ptr; return *this;}
        iterator operator++(int) {iterator tmp(*this); operator++(); return tmp;}
        bool operator==(const iterator& other) const {return ptr == other.ptr;}
        bool operator!=(const iterator& other) const {return !(*this == other);}
        move& operator*() {return *ptr;}
    };
    constexpr movelist() : count(0) {}
    uint32_t size() const {return count;}
    void add(move m) {list[count++] = m;}
    void clear() {count = 0;}
    move& operator[](uint32_t index) {return list[index];}
    iterator begin() {return iterator(list);}
    iterator end() {return iterator(list + count);}
};

#endif //MOTOR_MOVELIST_H
