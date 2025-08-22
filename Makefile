CXX=g++
STD=-std=c++17
OPTFLAGS=-O3
CXXFLAGS=-Wall -Wextra
DEBUGFLAGS=-g
INCLUDE_DIRS=./include
SRCDIR=./src
BINARIES=./bin/release
DEBUGBINARIES=./bin/debug
BUILDDIR=./build/release
DEBUGBUILDDIR=./build/debug

DEBUGCPPFLAGS=$(STD) $(CXXFLAGS) $(DEBUGFLAGS) -I$(INCLUDE_DIRS) $(OPTFLAGS)
CPPLAGS=$(STD) $(CXXFLAGS) -I$(INCLUDE_DIRS) $(OPTFLAGS)

CPPFILES=$(wildcard $(SRCDIR)/*.cpp)
OBJECTS=$(patsubst $(SRCDIR)/%.cpp,$(BINARIES)/%.o,$(CPPFILES))
DEBUGOBJECTS=$(patsubst $(SRCDIR)/%.cpp,$(DEBUGBINARIES)/%.o,$(CPPFILES))

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
	@$(CXX) -o $(BUILDDIR)/quiver $^

$(BINARIES)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BINARIES)
	@echo "Compiling $<"
	@$(CXX) $(CPPFLAGS) -c $< -o $@

build-debug: $(DEBUGOBJECTS)
	@mkdir -p $(DEBUGBUILDDIR)
	@$(CXX) -o $(DEBUGBUILDDIR)quiver $^

$(DEBUGBINARIES)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(DEBUGBINARIES)
	@echo "Compiling $<"
	@$(CXX) $(DEBUGCPPFLAGS) -c -o $@ $<

clean:
	@echo "Cleaning build..."
	@rm -rf $(BINARIES) $(DEBUGBINARIES) $(BUILDDIR) $(DEBUGBUILDDIR)

run: build-release
	@$(BUILDDIR)/quiver

run-debug: build-debug
	@$(BUILDDIR)/quiver
