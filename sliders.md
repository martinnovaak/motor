# Slider move generation
Motor uses Kindergarten bitboards or Split pext for generating sliders moves. 

Here is short and simple explanation of how kindergarten bitboards and split pext work:

# Kindergarten Bitboards

## Vertical attacks

```
 a-file occupancy       c7-h2 diag
 0 0 0 0 0 0 0 0     0 0 0 0 0 0 0 0     0 0 a b c d e f            0 0 0 0 0 0 0 0
 f 0 0 0 0 0 0 0     0 0 1 0 0 0 0 0     0 0 0 a b c d e            0 0 0 0 0 0 0 0
 e 0 0 0 0 0 0 0     0 0 0 1 0 0 0 0     0 0 0 0 a b c d            0 0 0 0 0 0 0 0
 d 0 0 0 0 0 0 0     0 0 0 0 1 0 0 0     0 0 0 0 0 a b c            0 0 0 0 0 0 0 0
 c 0 0 0 0 0 0 0  x  0 0 0 0 0 1 0 0  =  0 0 0 0 0 0 a b  >> 58  =  0 0 0 0 0 0 0 0 
 b 0 0 0 0 0 0 0     0 0 0 0 0 0 1 0     0 0 0 0 0 0 0 a            0 0 0 0 0 0 0 0
 a 0 0 0 0 0 0 0     0 0 0 0 0 0 0 1     0 0 0 0 0 0 0 0            0 0 0 0 0 0 0 0
 0 0 0 0 0 0 0 0     0 0 0 0 0 0 0 0     0 0 0 0 0 0 0 0            a b c d e f 0 0
```
```
static std::uint64_t rook_vertical(int square, std::uint64_t occupancy) {
    return vertical_subset[square][((((occupancy >> (square & 7)) & file_a2_a7) * diag_c2h7) >> 58)];
}
```
or other possible implementation using more lookup tables: 
```
static std::uint64_t rook_vertical(int square, std::uint64_t occupancy) {
    return vertical_subset[square][(((occupancy & vertical_mask[square]) * vertical_multiplier[square]) >> 58)];
}
```
## Horizontal attacks
```
 4th-rank occupancy       c7-h2 diag
 . . . . . . . .          0 0 0 0 0 0 0 0                0 0 0 0 0 0 0 0
 . . . . . . . .          0 0 0 0 0 0 0 0                0 0 0 0 0 0 0 0
 . . . . . . . .          0 0 0 0 0 0 0 0                0 0 0 0 0 0 0 0
 . . . . . . . .          . . . . . . . 0                0 0 0 0 0 0 0 0
 . b c d e f g . >> 25 =  . . . . . . . .  & 0b111111 =  0 0 0 0 0 0 0 0 
 . . . . . . . .          . . . . . . . .                0 0 0 0 0 0 0 0
 . . . . . . . .          . . . . . . . .                0 0 0 0 0 0 0 0
 . . . . . . . .          b c d e f g . .                a b c d e f 0 0
```

```
static std::uint64_t rook_horizontal(int square, std::uint64_t occupancy) {
    return horizontal_subset[square][(occupancy >> horizontal_shift_table[square]) & 63]; 
}
```

## Diagonal attacks

```
 main-diagonal occupancy    b2-b7 file
 0 0 0 0 0 0 0 0        0 0 0 0 0 0 0 0       0 0 b c d e f g            0 0 0 0 0 0 0 0
 0 0 0 0 0 0 g 0        0 1 0 0 0 0 0 0       0 0 b c d e f 0            0 0 0 0 0 0 0 0
 0 0 0 0 0 f 0 0        0 1 0 0 0 0 0 0       0 0 b c d e 0 0            0 0 0 0 0 0 0 0
 0 0 0 0 e 0 0 0        0 1 0 0 0 0 0 0       0 0 b c d 0 0 0            0 0 0 0 0 0 0 0
 0 0 0 d 0 0 0 0   x    0 1 0 0 0 0 0 0   =   0 0 b c 0 0 0 0  >> 58  =  0 0 0 0 0 0 0 0 
 0 0 c 0 0 0 0 0        0 1 0 0 0 0 0 0       0 0 b 0 0 0 0 0            0 0 0 0 0 0 0 0
 0 b 0 0 0 0 0 0        0 1 0 0 0 0 0 0       0 0 0 0 0 0 0 0            0 0 0 0 0 0 0 0
 0 0 0 0 0 0 0 0        0 0 0 0 0 0 0 0       0 0 0 0 0 0 0 0            b c d e f g 0 0
```

```
static std::uint64_t bishop_diagonal(int square, std::uint64_t occupancy) {
    return diagonal_subset[square][(((occupancy & diagonal_mask[square]) * file_b2_b7) >> 58)];
}
```

## Antidiagonal attacks
the same method as in diagonal attacks

```
 main-antidiagonal occupancy  b2-b7 file
 0 0 0 0 0 0 0 0         0 0 0 0 0 0 0 0     0 0 b c d e f g            0 0 0 0 0 0 0 0
 0 b 0 0 0 0 0 0         0 1 0 0 0 0 0 0     0 0 0 c d e f g            0 0 0 0 0 0 0 0
 0 0 c 0 0 0 0 0         0 1 0 0 0 0 0 0     0 0 0 0 d e f g            0 0 0 0 0 0 0 0
 0 0 0 d 0 0 0 0         0 1 0 0 0 0 0 0     0 0 0 0 0 e f g            0 0 0 0 0 0 0 0
 0 0 0 0 e 0 0 0    x    0 1 0 0 0 0 0 0  =  0 0 0 0 0 0 f g  >> 58  =  0 0 0 0 0 0 0 0 
 0 0 0 0 0 f 0 0         0 1 0 0 0 0 0 0     0 0 0 0 0 0 0 g            0 0 0 0 0 0 0 0
 0 0 0 0 0 0 g 0         0 1 0 0 0 0 0 0     0 0 0 0 0 0 0 0            0 0 0 0 0 0 0 0
 0 0 0 0 0 0 0 0         0 0 0 0 0 0 0 0     0 0 0 0 0 0 0 0            b c d e f g 0 0
```

```
static std::uint64_t bishop_antidiagonal(int square, std::uint64_t occupancy) {
    return antidiagonal_subset[square][(((occupancy & antidiagonal_mask[square]) * file_b2_b7) >> 58)];
}
```

## Rook attacks
All rook attacks are just simple union of vertical attack and horizontal attack

```
static std::uint64_t rook(int square, std::uint64_t occupancy) {
    return rook_vertical(square, occupancy) | rook_horizontal(square, occupancy);
}
```

## Bishop attacks
All bishop attacks are just simple union of diagonal attack and antidiagonal attack

```
static std::uint64_t bishop(int square, std::uint64_t occupancy) {
    return bishop_diagonal(square, occupancy) | bishop_antidiagonal(square, occupancy);
}
```

## Queen attacks
All Queen attacks are just simple union of rook attacks and bishop attacks

```
static std::uint64_t queen(int square, std::uint64_t occupancy) {
    return  rook(square, occupancy) | bishop(square, occupancy);
}
```

# Split Pext

```
static std::uint64_t rook_horizontal(int square, std::uint64_t occupancy) {
    return horizontal_subset[square][(occupancy >> horizontal_shift_table[square]) & 63]; // horizontal kindergarten
    //return horizontal_subset[square][_pext_u64(occupancy, horizontal_mask[square])];    // horizontal pext 
}

static std::uint64_t rook_vertical(int square, std::uint64_t occupancy) {
    return vertical_subset[square][_pext_u64(occupancy, vertical_mask[square])];
}

static std::uint64_t bishop_antidiagonal(int square, std::uint64_t occupancy) {
    return antidiagonal_subset[square][_pext_u64(occupancy, antidiagonal_mask[square])];
}

static std::uint64_t bishop_diagonal(int square, std::uint64_t occupancy) {
    return diagonal_subset[square][_pext_u64(occupancy, diagonal_mask[square])];
}

static std::uint64_t rook(int square, std::uint64_t occupancy) {
    return rook_horizontal(square, occupancy) | rook_vertical(square, occupancy);
}

static std::uint64_t bishop(int square, std::uint64_t occupancy) {
    return bishop_diagonal(square, occupancy) | bishop_antidiagonal(square, occupancy);
}

static std::uint64_t queen(int square, std::uint64_t occupancy) {
    return bishop(square, occupancy) | rook(square, occupancy);
}
```

It is important to mentioned that kindergarten horizontal attacks are probably faster than horizontal pext 

## Usage in pinner generation
```
uint64_t horizontal_pinners   = attacks<Ray::HORIZONTAL>(king_square, occupied)   & seen_enemy_hv_pieces;
uint64_t vertical_pinners     = attacks<Ray::VERTICAL>(king_square, occupied)     & seen_enemy_hv_pieces;
uint64_t antidiagonal_pinners = attacks<Ray::ANTIDIAGONAL>(king_square, occupied) & seen_enemy_ad_pieces;
uint64_t diagonal_pinners     = attacks<Ray::DIAGONAL>(king_square, occupied)     & seen_enemy_ad_pieces;
```
