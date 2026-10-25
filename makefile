CXXFLAGS = -std=c++20 -march=native -O3 -Wunused -Wall -Wextra -DNDEBUG
SUFFIX =

# clang >= 19 is required for #embed (used to embed nnue.bin)
ifeq ($(OS), Windows_NT)
	EXE ?= Motor
	CLANG_PLUS_PLUS_19 = $(shell where clang++-19 > NUL 2>&1)
	CXXFLAGS += -Wl,/STACK:16777216

	ENDS_WITH := exe
	ifneq ($(patsubst %$(ENDS_WITH),,$(lastword $(EXE))),)
		SUFFIX = .exe
	endif
else
	EXE ?= motor
	CLANG_PLUS_PLUS_19 = $(shell command -v clang++-19 2>/dev/null)
	CXXFLAGS += -lstdc++ -lm
endif

ifeq ($(strip $(CLANG_PLUS_PLUS_19)),)
    COMPILER = clang++
else
    COMPILER = clang++-19
endif

all:
	$(COMPILER) $(CXXFLAGS) main.cpp -o $(EXE)$(SUFFIX)
