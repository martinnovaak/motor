#ifndef MOTOR_KGSB_H
#define MOTOR_KGSB_H

#include "bitboard.h"

namespace KGSSB
{
    static uint64_t hMask[64];
    static uint64_t dMask[64];
    static uint64_t aMask[64];
    static uint64_t vSubset[64][64];
    static uint64_t hSubset[64][64];
    static uint64_t dSubset[64][64];
    static uint64_t aSubset[64][64];
    static uint32_t shift_horizontal_table[64];

    static void InitSquare(int sq)
    {
        int ts, dx, dy;
        uint64_t occ, index;
        int x = sq % 8;
        int y = sq / 8;

        // Initialize Kindergarten Super SISSY Bitboards
        // diagonals
        for (ts = sq + 9, dx = x + 1, dy = y + 1; dx < FILE_H && dy < RANK_8; dMask[sq] |= 1ull << ts, ts += 9, dx++, dy++);
        for (ts = sq - 9, dx = x - 1, dy = y - 1; dx > FILE_A && dy > RANK_1; dMask[sq] |= 1ull << ts, ts -= 9, dx--, dy--);

        // anti diagonals
        for (ts = sq + 7, dx = x - 1, dy = y + 1; dx > FILE_A && dy < RANK_8; aMask[sq] |= 1ull << ts, ts += 7, dx--, dy++);
        for (ts = sq - 7, dx = x + 1, dy = y - 1; dx < FILE_H && dy > RANK_1; aMask[sq] |= 1ull << ts, ts -= 7, dx++, dy--);

        // diagonal indexes
        for (index = 0; index < 64; index++) {
            dSubset[sq][index] = 0;
            occ = index << 1;
            if ((sq & 7) != FILE_H && (sq >> 3) != RANK_8) {
                for (ts = sq + 9; ; ts += 9) {
                    dSubset[sq][index] |= (1ull << ts);
                    if (occ & (1ull << (ts & 7))) break;
                    if ((ts & 7) == FILE_H || (ts >> 3) == RANK_8) break;
                }
            }
            if ((sq & 7) != FILE_A && (sq >> 3) != RANK_1) {
                for (ts = sq - 9; ; ts -= 9) {
                    dSubset[sq][index] |= (1ull << ts);
                    if (occ & (1ull << (ts & 7))) break;
                    if ((ts & 7) == FILE_A || (ts >> 3) == RANK_1) break;
                }
            }
        }

        // anti diagonal indexes
        for (index = 0; index < 64; index++) {
            aSubset[sq][index] = 0;
            occ = index << 1;
            if ((sq & 7) != FILE_A && (sq >> 3) != RANK_8) {
                for (ts = sq + 7; ; ts += 7) {
                    aSubset[sq][index] |= (1ull << ts);
                    if (occ & (1ull << (ts & 7))) break;
                    if ((ts & 7) == FILE_A || (ts >> 3) == RANK_8) break;
                }
            }
            if ((sq & 7) != FILE_H && (sq >> 3) != RANK_1) {
                for (ts = sq - 7; ; ts -= 7) {
                    aSubset[sq][index] |= (1ull << ts);
                    if (occ & (1ull << (ts & 7))) break;
                    if ((ts & 7) == FILE_H || (ts >> 3) == RANK_1) break;
                }
            }
        }

        // horizontal lines
        for (ts = sq + 1, dx = x + 1; dx < FILE_H; hMask[sq] |= 1ull << ts, ts += 1, dx++);
        for (ts = sq - 1, dx = x - 1; dx > FILE_A; hMask[sq] |= 1ull << ts, ts -= 1, dx--);

        // vertical indexes
        for (index = 0; index < 64; index++) {
            vSubset[sq][index] = 0;
            uint64_t blockers = 0;
            for (int i = 0; i <= 5; i++) {
                if (index & (1ull << i)) {
                    blockers |= (1ull << (((5 - i) << 3) + 8));
                }
            }
            if ((sq >> 3) != RANK_8) {
                for (ts = sq + 8; ; ts += 8) {
                    vSubset[sq][index] |= (1ull << ts);
                    if (blockers & (1ull << (ts - (ts & 7)))) break;
                    if ((ts >> 3) == RANK_8) break;
                }
            }
            if ((sq >> 3) != RANK_1) {
                for (ts = sq - 8; ; ts -= 8) {
                    vSubset[sq][index] |= (1ull << ts);
                    if (blockers & (1ull << (ts - (ts & 7)))) break;
                    if ((ts >> 3) == RANK_1) break;
                }
            }
        }

        // horizontal indexes
        for (index = 0; index < 64; index++) {
            hSubset[sq][index] = 0;
            occ = index << 1;
            if ((sq & 7) != FILE_H) {
                for (ts = sq + 1; ; ts += 1) {
                    hSubset[sq][index] |= (1ull << ts);
                    if (occ & (1ull << (ts & 7))) break;
                    if ((ts & 7) == FILE_H) break;
                }
            }
            if ((sq & 7) != FILE_A) {
                for (ts = sq - 1; ; ts -= 1) {
                    hSubset[sq][index] |= (1ull << ts);
                    if (occ & (1ull << (ts & 7))) break;
                    if ((ts & 7) == FILE_A) break;
                }
            }
        }
    }

    static void Init()
    {
        for (int i = 0; i < 64; i++) {
            InitSquare(i);
        }

        for (int i = 0; i < 64; i++) {
            shift_horizontal_table[i] = (i & 56) + 1;
        }
    }

    static constexpr uint64_t file_b2_b7 = 0x0002020202020200ull;
    static constexpr uint64_t file_a2_a7 = 0x0001010101010100ull;
    static constexpr uint64_t diag_c2h7  = 0x0080402010080400ull;

    static uint64_t bishop_diagonal(int sq, uint64_t occ) {
        return  dSubset[sq][(((occ & dMask[sq]) * file_b2_b7) >> 58)];
    }

    static uint64_t bishop_antidiagonal(int sq, uint64_t occ) {
        return  aSubset[sq][(((occ & aMask[sq]) * file_b2_b7) >> 58)];
    }

    static uint64_t Bishop(int sq, uint64_t occ)
    {
        return  dSubset[sq][(((occ & dMask[sq]) * file_b2_b7) >> 58)] |
                aSubset[sq][(((occ & aMask[sq]) * file_b2_b7) >> 58)];
    }

    static uint64_t rook_horizontal(int sq, uint64_t occ) {
        return hSubset[sq][(occ >> shift_horizontal_table[sq]) & 63];
    }

    static uint64_t rook_vertical(int sq, uint64_t occ) {
        return vSubset[sq][((((occ >> (sq & 7)) & file_a2_a7) * diag_c2h7) >> 58)];
    }

    static uint64_t Rook(int sq, uint64_t occ)
    {
        return hSubset[sq][(occ >> shift_horizontal_table[sq]) & 63] |
               vSubset[sq][((((occ >> (sq & 7)) & file_a2_a7) * diag_c2h7) >> 58)];
    }

    static uint64_t Queen(int sq, uint64_t occ)
    {
        return Rook(sq, occ) | Bishop(sq, occ);
    }
}

#endif //MOTOR_KGSB_H