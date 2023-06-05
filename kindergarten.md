# Kindergarten bitboards

Motor uses "Kindergarten Magic" for generating sliding moves. 
Kindergarten bitboards are used to generate rook attacks and ray attacks, whereas magic bitboards are used to generate bishop attacks.

Therefore, to generate queen attacks, I get a combination of kindergarten rook + magic bishop &rarr; Kindergarten Magic.

This is how motor can find pinners on each ray.

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

## Horizontal attacks
TODO

## Usage in pinner generation
```
uint64_t horizontal_pinners   = attacks<Ray::HORIZONTAL>(king_square, occupied)   & seen_enemy_hv_pieces;
uint64_t vertical_pinners     = attacks<Ray::VERTICAL>(king_square, occupied)     & seen_enemy_hv_pieces;
uint64_t antidiagonal_pinners = attacks<Ray::ANTIDIAGONAL>(king_square, occupied) & seen_enemy_ad_pieces;
uint64_t diagonal_pinners     = attacks<Ray::DIAGONAL>(king_square, occupied)     & seen_enemy_ad_pieces;
```
