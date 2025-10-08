CXX=g++
STD=-std=c++17
OPTFLAGS=-O3
CXXFLAGS=-Wall -Wextra
LDFLAGS=-lcpr -lcurl -lssl -lcrypto -pthread -lsqlite3 
DEBUG_FLAGS=-g
INCLUDE_DIRS=./include
SRCDIR=./src
BINARIES=./bin/release
DEBUG_BINARIES=./bin/debug
BUILDDIR=./build/release
DEBUG_BUILDDIR=./build/debug

DEBUG_CPPFLAGS=$(STD) $(CXXFLAGS) $(DEBUG_FLAGS) -I$(INCLUDE_DIRS) $(OPTFLAGS)
CPPFLAGS=$(STD) $(CXXFLAGS) -I$(INCLUDE_DIRS) $(OPTFLAGS)

CPPFILES=$(wildcard $(SRCDIR)/*.cpp)
OBJECTS=$(patsubst $(SRCDIR)/%.cpp,$(BINARIES)/%.o,$(CPPFILES))
DEBUG_OBJECTS=$(patsubst $(SRCDIR)/%.cpp,$(DEBUG_BINARIES)/%.o,$(CPPFILES))

all:
	@echo "Usage: make [options]"
	@echo "options:"
	@echo "build-release -> release build without debug flags"
	@echo "build-debug -> debug build"
	@echo "clean -> remove executable and objects"
	@echo "run-debug -> run debug build"
	@echo "run -> run release build"

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
	@$(CXX) $(DEBUG_CPPFLAGS) -c -o $@ $<

clean:
	@echo "Cleaning build..."
	@rm -rf $(BINARIES) $(DEBUG_BINARIES) $(BUILDDIR) $(DEBUG_BUILDDIR)

run: build-release
	@$(BUILDDIR)/quiver

run-debug: build-debug
	@$(DEBUG_BUILDDIR)/quiver