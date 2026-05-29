# qm-minimizer — Makefile
#
# Quick start:
#   make            # builds build/qm
#   make test       # runs the small PLAs (sanity check)
#   make bench      # runs the IWLS benchmark, writes tempos.csv
#   make clean
#
# Variables:
#   DEBUG=1         # -O0 -g instead of -O3 -march=native
#   CXX=g++         # override compiler (default: clang++)

CXX      ?= clang++
CPPFLAGS  = -Iinclude -MMD -MP
CXXFLAGS  = -std=c++17 -Wall -Wextra -Wpedantic -Wno-unused-parameter
LDFLAGS   = -pthread

ifdef DEBUG
  CXXFLAGS += -O0 -g -DDEBUG
else
  CXXFLAGS += -O3 -march=native -DNDEBUG
endif

BUILD_DIR := build
SRCS      := $(wildcard src/*.cc)
OBJS      := $(SRCS:src/%.cc=$(BUILD_DIR)/%.o)
DEPS      := $(OBJS:.o=.d)
BIN       := $(BUILD_DIR)/qm

.PHONY: all clean rebuild demo test bench run help

all: $(BIN)

# Always-from-scratch build. Idempotent: rebuilds identically every time.
rebuild: clean
	@$(MAKE) --no-print-directory all

# Live demo: wipe state, build, run the smoke tests. One command.
demo: clean
	@$(MAKE) --no-print-directory test

$(BIN): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: src/%.cc
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) tempos.csv

run: $(BIN)
	./$(BIN) data/tests/ex01_funcao3var.pla --stats

test: $(BIN)
	@echo "=== AND ==="
	@./$(BIN) data/tests/ex_and.pla
	@echo "=== 3 vars ==="
	@./$(BIN) data/tests/ex01_funcao3var.pla
	@echo "=== 4 vars ==="
	@./$(BIN) data/tests/ex02_funcao4var.pla
	@echo "=== 5 vars ==="
	@./$(BIN) data/tests/ex03_funcao5var.pla
	@echo "=== 6 vars ==="
	@./$(BIN) data/tests/ex04_funcao6var.pla

bench: $(BIN)
	./$(BIN) --bench data/benchmark/ --csv tempos.csv

help:
	@echo "Targets:"
	@echo "  all (default)  Build $(BIN)"
	@echo "  clean          Remove $(BUILD_DIR)/ and tempos.csv"
	@echo "  rebuild        clean + all (always from scratch)"
	@echo "  demo           clean + test (full from-scratch run for demos)"
	@echo "  test           Build + minimize the 5 small PLAs in data/tests/"
	@echo "  bench          Build + run on data/benchmark/, write tempos.csv"
	@echo "  run            Build + minimize a single sample PLA with --stats"
	@echo ""
	@echo "Variables:"
	@echo "  DEBUG=1        Build with -O0 -g (default: -O3 -march=native)"
	@echo "  CXX=<bin>      Compiler (default: clang++)"

-include $(DEPS)
