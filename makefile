ifeq ($(OS), Windows_NT)
	EXE ?= Motor

	ENDS_WITH := exe
	SUFFIX :=
	ifneq ($(patsubst %$(ENDS_WITH),,$(lastword $(EXE))),)
		SUFFIX := .exe
	endif

	command := clang++ -O3 -std=c++20 -march=x86-64-v3 -Wl,/STACK:16777216 -Wunused -Wall -Wextra -DNDEBUG main.cpp -o $(EXE)$(SUFFIX)
else
	EXE ?= motor
	command := clang++ -O3 -std=c++20 -lstdc++ -lm -march=x86-64-v3 -Wunused -Wall -Wextra -DNDEBUG main.cpp -o $(EXE)
endif

# Build rule
all:
	$(command)