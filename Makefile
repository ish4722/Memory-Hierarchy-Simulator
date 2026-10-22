CXX = g++
CXXFLAGS = -O3 -std=c++17 -Iinclude -g -Wall -Wextra -pedantic
SRC = src/main.cpp src/physical.cpp src/allocator.cpp src/vm.cpp src/cache.cpp src/tlb.cpp src/cache_hierarchy.cpp
OBJ = $(SRC:.cpp=.o)
BENCH_SRC = src/benchmark.cpp src/physical.cpp src/allocator.cpp src/vm.cpp src/tlb.cpp src/cache_hierarchy.cpp

all: sim

sim: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

bench: $(BENCH_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $(BENCH_SRC)

clean:
	rm -f sim bench $(OBJ)

.PHONY: all clean bench
