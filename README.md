# Motor

UCI chess engine written in C++ 17 with (768 -> 32) x 2 -> 1 NNUE trained with [forge](https://github.com/martinnovaak/forge) and tested with custom [testing tool](https://github.com/martinnovaak/engineduel). 

Currently requires Clang or GCC/MinGW compiler.

Engine contains legal movegen with [kidergarten bitboards](https://github.com/martinnovaak/motor/blob/main/chess_board/slider_attacks/kindergarten.md).
