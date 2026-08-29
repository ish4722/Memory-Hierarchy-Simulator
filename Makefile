CXX = g++
CXXFLAGS = -O3 -std=c++17 -Iinclude -g
SRC = src/main.cpp src/physical.cpp src/allocator.cpp src/vm.cpp src/cache.cpp
OBJ = $(SRC:.cpp=.o)

all: sim

sim: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f sim $(OBJ)
