.DEFAULT_GOAL := all
CXXFLAGS = -std=c++20 -march=native -O3 -Wunused -Wall -Wextra -DNDEBUG
SUFFIX =

.DELETE_ON_ERROR:
# clang >= 19 is required for #embed (used to embed nnue.bin)
ifeq ($(OS), Windows_NT)
NNUE_URL = $(shell type net.txt)
	EXE ?= Motor
	CLANG_PLUS_PLUS_19 = $(shell where clang++-19 > NUL 2>&1)
	CXXFLAGS += -Wl,/STACK:16777216

	ENDS_WITH := exe
	ifneq ($(patsubst %$(ENDS_WITH),,$(lastword $(EXE))),)
		SUFFIX = .exe
	endif
else
NNUE_URL = $(shell cat net.txt)
	EXE ?= motor
	CLANG_PLUS_PLUS_19 = $(shell command -v clang++-19 2>/dev/null)
	CXXFLAGS += -lstdc++ -lm
endif

ifeq ($(strip $(CLANG_PLUS_PLUS_19)),)
    COMPILER = clang++
else
    COMPILER = clang++-19
endif

EVALFILE ?= nnue.bin
NNUE_DIRECTORY = $(dir $(abspath $(EVALFILE)))
NNUE_FILENAME = $(notdir $(EVALFILE))
CXXFLAGS += -I"$(NNUE_DIRECTORY)" -DNNUE_FILE=\"$(NNUE_FILENAME)\"

nnue.bin: net.txt
	curl --location --fail --retry 3 --output "$@" "$(NNUE_URL)"

all: $(EVALFILE)
	$(COMPILER) $(CXXFLAGS) main.cpp -o $(EXE)$(SUFFIX)
