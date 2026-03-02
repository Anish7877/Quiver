CXX = g++
STD = -std=c++20
OPTFLAGS = -O3
CXXFLAGS = -Wall -Wextra -Wpedantic -march=native
LDFLAGS = -lcpr -lcurl -lssl -lcrypto -pthread -lsqlite3 -lutil
DEBUG_FLAGS = -g

INCLUDE_DIRS = ./include
SRCDIR = ./src
TEST_DIR = ./tests

BINARIES = ./bin/release
DEBUG_BINARIES = ./bin/debug
TEST_BINARIES = ./bin/test

BUILDDIR = ./build/release
DEBUG_BUILDDIR = ./build/debug
TEST_OUT = ./test_out

DEPFLAGS = -MMD -MP
DEBUG_CPPFLAGS = $(STD) $(CXXFLAGS) $(DEBUG_FLAGS) -I$(INCLUDE_DIRS) $(OPTFLAGS) $(DEPFLAGS)
CPPFLAGS = $(STD) $(CXXFLAGS) -I$(INCLUDE_DIRS) $(OPTFLAGS) $(DEPFLAGS)

CPPFILES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(CPPFILES:$(SRCDIR)/%.cpp=$(BINARIES)/%.o)
DEBUG_OBJECTS = $(CPPFILES:$(SRCDIR)/%.cpp=$(DEBUG_BINARIES)/%.o)

MAIN_OBJ = $(BINARIES)/main.o
TESTABLE_OBJS = $(filter-out $(MAIN_OBJ), $(OBJECTS))

TEST_FILES = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS = $(TEST_FILES:$(TEST_DIR)/%.cpp=$(TEST_BINARIES)/%.o)
TEST_EXEC = $(TEST_OUT)/run_tests

all: build-release

build-release: $(OBJECTS)
	@mkdir -p $(BUILDDIR)
	@$(CXX) -o $(BUILDDIR)/quiver $^ $(LDFLAGS)

$(BINARIES)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BINARIES)
	@echo "Compiling $<"
	@$(CXX) $(CPPFLAGS) -c $< -o $@

build-debug: $(DEBUG_OBJECTS)
	@mkdir -p $(DEBUG_BUILDDIR)
	@$(CXX) -o $(DEBUG_BUILDDIR)/quiver $^ $(LDFLAGS)

$(DEBUG_BINARIES)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(DEBUG_BINARIES)
	@echo "Compiling $<"
	@$(CXX) $(DEBUG_CPPFLAGS) -c $< -o $@

test: $(TEST_EXEC)
	@echo "Running all tests..."
	@./$(TEST_EXEC)

$(TEST_EXEC): $(TEST_OBJS) $(TESTABLE_OBJS)
	@mkdir -p $(TEST_OUT)
	@$(CXX) $(CPPFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_BINARIES)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(TEST_BINARIES)
	@echo "Compiling Test: $<"
	@$(CXX) $(CPPFLAGS) -c $< -o $@

clean:
	@echo "Cleaning build..."
	@rm -rf ./bin ./build $(TEST_OUT)

run: build-release
	@$(BUILDDIR)/quiver

run-debug: build-debug
	@$(DEBUG_BUILDDIR)/quiver

.PHONY: all build-release build-debug test clean run run-debug

-include $(OBJECTS:.o=.d)
-include $(DEBUG_OBJECTS:.o=.d)
-include $(TEST_OBJS:.o=.d)
