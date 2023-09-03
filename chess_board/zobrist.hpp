#ifndef MOTOR_ZOBRIST_HPP
#define MOTOR_ZOBRIST_HPP

#include "types.hpp"
#include "zobrist_keys.hpp"

class zobrist {
public:
    zobrist() : hash_key(0ull) {}

    void update_castling_hash(std::uint8_t right) {
        hash_key ^= zobrist_keys::castling_keys[right];
    }

    void update_side_hash() {
        hash_key ^= zobrist_keys::side_key;
    }

    void update_enpassant_hash(Square enpassant_square) {
        hash_key ^= zobrist_keys::enpassant_keys[enpassant_square];
    }

    void update_psqt_hash(Color color, Piece piece, Square square) {
        hash_key ^= zobrist_keys::psqt_keys[color][piece][square];
    }

    [[nodiscard]] std::uint64_t get_key() const {
        return hash_key;
    }

    bool operator==(const zobrist & zobrist_key) const{
        return zobrist_key.hash_key == hash_key;
    }

private:
    std::uint64_t hash_key;
};

#endif //MOTOR_ZOBRIST_HPP
