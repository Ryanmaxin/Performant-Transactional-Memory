# Set the C++ compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -fPIC -shared -Wall

# Paths
SO_DIR := ../394984
SO_FILE := ../394984.so
MAIN_CPP := ./concurrent.cpp
EXECUTABLE := test

.PHONY: all clean run

# Get all source files in ../394984 to track changes
SO_SOURCES := $(shell find $(SO_DIR) -type f -name '*.cpp' -or -name '*.hpp')

# Default target: Build the executable
all: $(EXECUTABLE)

# Step 1: Make ../394984/Makefile and produce ../394984.so
$(SO_FILE): $(SO_SOURCES)
	$(MAKE) -C $(SO_DIR)

# Step 2: Compile ./main.cpp and link with ../394984.so
$(EXECUTABLE): $(MAIN_CPP) $(SO_FILE)
	$(CXX) -std=c++17 -o $(EXECUTABLE) $(MAIN_CPP) $(SO_FILE)

# Step 3: Build and run in one step
run: all
	./$(EXECUTABLE)

# Clean the build
clean:
	$(MAKE) -C $(SO_DIR) clean
	rm -f $(EXECUTABLE)
