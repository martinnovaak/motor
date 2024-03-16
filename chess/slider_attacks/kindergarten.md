# Kindergarten bitboards

Motor uses Kindergarten Bitboards for generating sliding moves. 

## Vertical attacks

```
 a-file occupancy       c7-h2 diag
 . . . . . . . .     . . . . . . . .     . . a b c d e f            0 0 0 0 0 0 0 0
 f . . . . . . .     . . 1 . . . . .     . . . a b c d e            0 0 0 0 0 0 0 0
 e . . . . . . .     . . . 1 . . . .     . . . . a b c d            0 0 0 0 0 0 0 0
 d . . . . . . .     . . . . 1 . . .     . . . . . a b c            0 0 0 0 0 0 0 0
 c . . . . . . .  x  . . . . . 1 . .  =  . . . . . . a b  >> 58  =  0 0 0 0 0 0 0 0 
 b . . . . . . .     . . . . . . 1 .     . . . . . . . a            0 0 0 0 0 0 0 0
 a . . . . . . .     . . . . . . . 1     . . . . . . . .            0 0 0 0 0 0 0 0
 . . . . . . . .     . . . . . . . .     . . . . . . . .            a b c d e f 0 0
```
```
std::uint64_t rook_vertical(Square square, std::uint64_t occupancy) {
    return vertical_subset[square][(((occupancy & vertical_mask[square]) * vertical_multiplier[square]) >> 58)];
}
```

## Diagonal attacks
```
 diagonal occupancy     b2-b7 file
 . . . . . . . .     . . . . . . . .     . . a b c d e f            0 0 0 0 0 0 0 0
 . . . . . . f .     . 1 . . . . . .     . . a b c d e .            0 0 0 0 0 0 0 0
 . . . . . e . .     . 1 . . . . . .     . . a b c d . .            0 0 0 0 0 0 0 0
 . . . . d . . .     . 1 . . . . . .     . . a b c . . .            0 0 0 0 0 0 0 0
 . . . c . . . .  x  . 1 . . . . . .  =  . . a b . . . .  >> 58  =  0 0 0 0 0 0 0 0
 . . b . . . . .     . 1 . . . . . .     . . a . . . . .            0 0 0 0 0 0 0 0
 . a . . . . . .     . 1 . . . . . .     . . . . . . . .            0 0 0 0 0 0 0 0
 . . . . . . . .     . . . . . . . .     . . . . . . . .            a b c d e f 0 0
```
```
constexpr std::uint64_t file_b2_b7 = 0x0002020202020200ull;

std::uint64_t bishop_diagonal(std::uint8_t square, std::uint64_t occupancy) {
    return diagonal_subset[square][(((occupancy & diagonal_mask[square]) * file_b2_b7) >> 58)];
}
```

## Antidiagonal attacks
```
 diagonal occupancy     b2-b7 file
 . . . . . . . .     . . . . . . . .     . . f e d c b a            0 0 0 0 0 0 0 0
 . f . . . . . .     . 1 . . . . . .     . . . e d c b a            0 0 0 0 0 0 0 0
 . . e . . . . .     . 1 . . . . . .     . . . . d c b a            0 0 0 0 0 0 0 0
 . . . d . . . .     . 1 . . . . . .     . . . . . c b a            0 0 0 0 0 0 0 0
 . . . . c . . .  x  . 1 . . . . . .  =  . . . . . . b a  >> 58  =  0 0 0 0 0 0 0 0
 . . . . . b . .     . 1 . . . . . .     . . . . . . . a            0 0 0 0 0 0 0 0
 . . . . . . a .     . 1 . . . . . .     . . . . . . . .            0 0 0 0 0 0 0 0
 . . . . . . . .     . . . . . . . .     . . . . . . . .            f e d c b a 0 0
```

```
std::uint64_t bishop_antidiagonal(Square square, std::uint64_t occupancy) {
    return antidiagonal_subset[square][(((occupancy & antidiagonal_mask[square]) * file_b2_b7) >> 58)];
}
```

## Horizontal attacks 

Horizontal attacks are much simpler. If we are interested in occupancy of n-th rank, we just need to shift the occupancy bit board by `n * 8 + 1` bits. It is also necessary to make an intersection with the first 6 bits.

```
 a-file occupancy       
 . . . . . . . .            . . . . . . . .              0 0 0 0 0 0 0 0
 . . . . . . . .            . . . . . . . .              0 0 0 0 0 0 0 0
 . . . . . . . .            . . . . . . . .              0 0 0 0 0 0 0 0
 . a b c d e f .            . . . . . . . .              0 0 0 0 0 0 0 0
 . . . . . . . .  >> 33  =  . . . . . . . . & 0b111111 = 0 0 0 0 0 0 0 0
 . . . . . . . .            . . . . . . . .              0 0 0 0 0 0 0 0
 . . . . . . . .            . . . . . . . .              0 0 0 0 0 0 0 0
 . . . . . . . .            a b c d e f . .              a b c d e f 0 0
```

The generation of horizontal attacks can also be sped up a bit by precomputing `n * 32 + 1` in the lookup table.

```
std::uint64_t rook_horizontal(Square square, std::uint64_t occupancy) {
    return horizontal_subset[square][(occupancy >> horizontal_shift_table[square]) & 0b111111];
}
```

## Sliders attacks
Bishop attacks can be simply calculated as the union of diagonal and anti-diagonal attacks. In the same way we can obtain the rook attacks as the union of horizontal and vertical attacks. And queen attacks can be obtained by unifying rook and bishop attacks.

```
std::uint64_t bishop(Square square, std::uint64_t occupancy) {
    return bishop_diagonal(square, occupancy) | bishop_antidiagonal(square, occupancy);
}

std::uint64_t rook(Square square, std::uint64_t occupancy) {
    return rook_horizontal(square, occupancy) | rook_vertical(square, occupancy);
}

std::uint64_t queen(Square square, std::uint64_t occupancy) {
    return rook(square, occupancy) | bishop(square, occupancy);
}
```
