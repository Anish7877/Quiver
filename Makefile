CXX ?= g++
MIN_CXX_VER = 15
STD = -std=c++20
OPTFLAGS = -O3
CXXFLAGS = -Wall -Wextra -Wpedantic -march=native
LDFLAGS = -lcpr -lcurl -lssl -lcrypto -pthread -lsqlite3 -lutil -lrocksdb
DEBUG_FLAGS = -g
DEPFLAGS = -MMD -MP

FLATC_CC = flatc
FLATC_CCFLAGS = --cpp

INCLUDE_DIRS = ./include
SRCDIR = ./src
TEST_DIR = ./tests

BINARIES = ./bin/release
DEBUG_BINARIES = ./bin/debug
TEST_BINARIES = ./bin/test

BUILDDIR = ./build/release
DEBUG_BUILDDIR = ./build/debug
TEST_OUT = ./test_out

FLATBUFFERS_SCHEMAS_DIR = ./flatbuffer_schemas
GENERATED_SCHEMAS_DIR = ./gen_schemas

DEBUG_CPPFLAGS = $(STD) $(CXXFLAGS) $(DEBUG_FLAGS) -I$(INCLUDE_DIRS) -I$(GENERATED_SCHEMAS_DIR) $(DEPFLAGS)
CPPFLAGS = $(STD) $(CXXFLAGS) -I$(INCLUDE_DIRS) -I$(GENERATED_SCHEMAS_DIR) $(OPTFLAGS) $(DEPFLAGS)

FLATBUFFERS_SCHEMAS = $(wildcard $(FLATBUFFERS_SCHEMAS_DIR)/*.fbs)

CPPFILES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(CPPFILES:$(SRCDIR)/%.cpp=$(BINARIES)/%.o)
DEBUG_OBJECTS = $(CPPFILES:$(SRCDIR)/%.cpp=$(DEBUG_BINARIES)/%.o)

MAIN_OBJ = $(BINARIES)/main.o
TESTABLE_OBJS = $(filter-out $(MAIN_OBJ), $(OBJECTS))

TEST_FILES = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS = $(TEST_FILES:$(TEST_DIR)/%.cpp=$(TEST_BINARIES)/%.o)
TEST_EXEC = $(TEST_OUT)/run_tests

CXX_PATH = $(shell command -v $(CXX) 2> /dev/null)
CXX_VERSION := $(shell $(CXX) -dumpversion | cut -f1 -d.)
IS_SUPPORTED := $(shell [ "$(CXX_VERSION)" -ge "$(MIN_CXX_VER)" ] && echo true || echo false)
FLATC_CC_PATH = $(shell command -v $(FLATC_CC) 2> /dev/null)

ifeq ($(CXX_PATH),)
	$(error "Error: '$(CXX)' not found.")
endif

ifeq ($(IS_SUPPORTED),false)
	$(error "Error: '$(CXX)' >= $(MIN_CXX_VER)")
endif

ifeq ($(FLATC_CC_PATH),)
	$(error "Error: '$(FLATC_CC)' not found.")
endif

all: generate_schemas build-release

build-release: $(OBJECTS)
	@mkdir -p $(BUILDDIR)
	$(CXX) -o $(BUILDDIR)/quiver $^ $(LDFLAGS)

$(BINARIES)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BINARIES)
	$(CXX) $(CPPFLAGS) -c $< -o $@

build-debug: $(DEBUG_OBJECTS)
	@mkdir -p $(DEBUG_BUILDDIR)
	$(CXX) -o $(DEBUG_BUILDDIR)/quiver $^ $(LDFLAGS)

$(DEBUG_BINARIES)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(DEBUG_BINARIES)
	$(CXX) $(DEBUG_CPPFLAGS) -c $< -o $@

generate_schemas: $(FLATBUFFERS_SCHEMAS)
	@mkdir -p $(GENERATED_SCHEMAS_DIR)
	$(FLATC_CC) $(FLATC_CCFLAGS) -o $(GENERATED_SCHEMAS_DIR) $^

test: $(TEST_EXEC)
	@./$(TEST_EXEC)

$(TEST_EXEC): $(TEST_OBJS) $(TESTABLE_OBJS)
	@mkdir -p $(TEST_OUT)
	$(CXX) $(CPPFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_BINARIES)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(TEST_BINARIES)
	$(CXX) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf ./bin ./build $(TEST_OUT) $(GENERATED_SCHEMAS_DIR)

.PHONY: all build-release build-debug generate_schemas test clean

-include $(OBJECTS:.o=.d)
-include $(DEBUG_OBJECTS:.o=.d)
-include $(TEST_OBJS:.o=.d)
