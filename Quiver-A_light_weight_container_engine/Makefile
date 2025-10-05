CXX=g++
STD=-std=c++17
OPTFLAGS=-O3
# Add yaml-cpp to the linker flags. This is required for the parser.
LDFLAGS=-lcpr -lcurl -lssl -lcrypto -pthread -lsqlite3 -lyaml-cpp
DEBUG_FLAGS=-g
INCLUDE_DIRS=./include
SRCDIR=./src
BINARIES=./bin/release
DEBUG_BINARIES=./bin/debug
BUILDDIR=./build/release
DEBUG_BUILDDIR=./build/debug

DEBUG_CPPFLAGS=$(STD) $(CXXFLAGS) $(DEBUG_FLAGS) -I$(INCLUDE_DIRS) $(OPTFLAGS)
# Corrected typo from CPPLAGS to CXXFLAGS and added standard flags.
CXXFLAGS+=-Wall -Wextra -I$(INCLUDE_DIRS) $(OPTFLAGS) $(STD)

# This will automatically find all .cpp files, including the new orchestrator and parser files.
CPPFILES=$(wildcard $(SRCDIR)/*.cpp)
OBJECTS=$(patsubst $(SRCDIR)/%.cpp,$(BINARIES)/%.o,$(CPPFILES))
DEBUG_OBJECTS=$(patsubst $(SRCDIR)/%.cpp,$(DEBUG_BINARIES)/%.o,$(CPPFILES))

all:
	@echo "Usage: make [options]"
	@echo "options:"
	@echo "  build-release -> release build without debug flags"
	@echo "  build-debug   -> debug build"
	@echo "  clean         -> remove executable and objects"
	@echo "  run-debug     -> run debug build"
	@echo "  run           -> run release build"

build-release: $(OBJECTS)
	@mkdir -p $(BUILDDIR)
	@$(CXX) -o $(BUILDDIR)/quiver $^ $(LDFLAGS)

$(BINARIES)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BINARIES)
	@echo "Compiling $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

build-debug: $(DEBUG_OBJECTS)
	@mkdir -p $(DEBUG_BUILDDIR)
	@$(CXX) -o $(DEBUG_BUILDDIR)/quiver $^ $(LDFLAGS)

$(DEBUG_BINARIES)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(DEBUG_BINARIES)
	@echo "Compiling $<"
	@$(CXX) $(DEBUG_CPPFLAGS) -c $< -o $@

clean:
	@echo "Cleaning build..."
	@rm -rf $(BINARIES) $(DEBUG_BINARIES) $(BUILDDIR) $(DEBUG_BUILDDIR)

# Updated run commands to show the new usage for .qivr files.
run: build-release
	@echo "To run, use: $(BUILDDIR)/quiver run <your_file.qivr>"

run-debug: build-debug
	@echo "To run, use: $(DEBUG_BUILDDIR)/quiver run <your_file.qivr>"