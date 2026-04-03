CXX := g++
CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -pedantic
LDFLAGS := -lX11 -lGL

TARGET := build/voxelcraft
TEST_BIN := build/voxelcraft_tests

SRC := src/main.cpp src/game.cpp src/world.cpp src/platform_x11.cpp src/renderer.cpp src/session.cpp src/persistence.cpp
OBJ := build/main.o build/game.o build/world.o build/platform_x11.o build/renderer.o build/session.o build/persistence.o
TEST_OBJ := build/test_main.o build/world.o build/session.o build/persistence.o

.PHONY: all run test clean

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f build/*.o $(TARGET) $(TEST_BIN)

$(TARGET): $(OBJ) | build
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

$(TEST_BIN): $(TEST_OBJ) | build
	$(CXX) $(TEST_OBJ) -o $@

build/main.o: src/main.cpp include/game.hpp include/world.hpp include/platform.hpp include/common.hpp include/renderer.hpp include/session.hpp include/input.hpp include/persistence.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/game.o: src/game.cpp include/game.hpp include/world.hpp include/platform.hpp include/common.hpp include/renderer.hpp include/session.hpp include/input.hpp include/persistence.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/world.o: src/world.cpp include/world.hpp include/common.hpp include/persistence.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/platform_x11.o: src/platform_x11.cpp include/platform.hpp include/input.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/renderer.o: src/renderer.cpp include/renderer.hpp include/session.hpp include/world.hpp include/common.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/session.o: src/session.cpp include/session.hpp include/world.hpp include/common.hpp include/input.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/persistence.o: src/persistence.cpp include/persistence.hpp include/common.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/test_main.o: tests/test_main.cpp include/world.hpp include/common.hpp include/session.hpp include/input.hpp include/persistence.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build
