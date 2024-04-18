# Motor

UCI chess engine written in C++ 20 with (768 -> 512) x 2 -> 1 NNUE trained with [forge](https://github.com/martinnovaak/forge) and tested with custom [testing tool](https://github.com/martinnovaak/engineduel). 

Engine contains legal movegen with [kidergarten bitboards](https://github.com/martinnovaak/motor/blob/main/chess_board/slider_attacks/kindergarten.md).

## Compiling

### MSBuild
`msbuild.exe" motor.sln /p:Configuration=Release /p:Platform=x64`

### CMake
`cmake -B Build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -G Ninja`

`cmake --build Build --config Release`

### CLang 
`clang++ -std=c++20 -march=native -O3 -DNDEBUG main.cpp -o motor.exe`
