# Kindergarten bitboards

Motor uses Kindergarten Bitboards for generating sliding moves. 


## Vertical attacks


```
 a-file occupancy       c7-h2 diag
 . . . . . . . .     . . . . . . . .     . . a b c d e f            . . . . . . . .
 f . . . . . . .     . . 1 . . . . .     . . . a b c d e            . . . . . . . .
 e . . . . . . .     . . . 1 . . . .     . . . . a b c d            . . . . . . . .
 d . . . . . . .     . . . . 1 . . .     . . . . . a b c            . . . . . . . .
 c . . . . . . .  x  . . . . . 1 . .  =  . . . . . . a b  >> 58  =  . . . . . . . . 
 b . . . . . . .     . . . . . . 1 .     . . . . . . . a            . . . . . . . .
 a . . . . . . .     . . . . . . . 1     . . . . . . . .            . . . . . . . .
 . . . . . . . .     . . . . . . . .     . . . . . . . .            a b c d e f . .
```
```
std::uint64_t rook_vertical(Square square, std::uint64_t occupancy) {
    return vertical_subset[square][(((occupancy & vertical_mask[square]) * vertical_multiplier[square]) >> 58)];
}
```

## Diagonal attacks
```
 diagonal occupancy       c7-h2 diag
 . . . . . . . .     . . . . . . . .     . . a b c d e f            . . . . . . . .
 . . . . . . f .     . 1 . . . . . .     . . a b c d e .            . . . . . . . .
 . . . . . e . .     . 1 . . . . . .     . . a b c d . .            . . . . . . . .
 . . . . d . . .     . 1 . . . . . .     . . a b c . . .            . . . . . . . .
 . . . c . . . .  x  . 1 . . . . . .  =  . . a b . . . .  >> 58  =  . . . . . . . . 
 . . b . . . . .     . 1 . . . . . .     . . a . . . . .            . . . . . . . .
 . a . . . . . .     . 1 . . . . . .     . . . . . . . .            . . . . . . . .
 . . . . . . . .     . . . . . . . .     . . . . . . . .            a b c d e f . .
```
```
constexpr std::uint64_t file_b2_b7 = 0x0002020202020200ull;

std::uint64_t bishop_diagonal(std::uint8_t square, std::uint64_t occupancy) {
    return diagonal_subset[square][(((occupancy & diagonal_mask[square]) * file_b2_b7) >> 58)];
}
```

## Antidiagonal attacks
```
 diagonal occupancy       c7-h2 diag
 . . . . . . . .     . . . . . . . .     . . f e d c b a            . . . . . . . .
 . f . . . . . .     . 1 . . . . . .     . . . e d c b a            . . . . . . . .
 . . e . . . . .     . 1 . . . . . .     . . . . d c b a            . . . . . . . .
 . . . d . . . .     . 1 . . . . . .     . . . . . c b a            . . . . . . . .
 . . . . c . . .  x  . 1 . . . . . .  =  . . . . . . b a  >> 58  =  . . . . . . . . 
 . . . . . b . .     . 1 . . . . . .     . . . . . . . a            . . . . . . . .
 . . . . . . a .     . 1 . . . . . .     . . . . . . . .            . . . . . . . .
 . . . . . . . .     . . . . . . . .     . . . . . . . .            f e d c b a . .
```

```
std::uint64_t bishop_antidiagonal(Square square, std::uint64_t occupancy) {
    return antidiagonal_subset[square][(((occupancy & antidiagonal_mask[square]) * file_b2_b7) >> 58)];
}
```