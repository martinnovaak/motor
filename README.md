# Motor

UCI chess engine written in C++ 20 with (768 x 8 -> 1536) x 2 -> 1 NNUE trained with [bullet](https://github.com/jw1912/bullet). Previous nets were trained with custom trainer [forge](https://github.com/martinnovaak/forge).
A custom test [testing tool](https://github.com/martinnovaak/engineduel) was previously used to develop the engine. From June 2024, [Openbench](https://github.com/AndyGrant/OpenBench) is used instead for testing.

Engine contains legal movegen with [kidergarten bitboards](https://github.com/martinnovaak/motor/blob/main/chess_board/slider_attacks/kindergarten.md).

## Compiling
Requirements: CLang
### MSBuild
`msbuild.exe" motor.sln /p:Configuration=Release /p:Platform=x64`

### CMake version >= 3.28
`cmake -B Build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -G Ninja`

`cmake --build Build --config Release`

### CLang 
`clang++ -std=c++20 -march=native -O3 -DNDEBUG main.cpp -o motor.exe`
