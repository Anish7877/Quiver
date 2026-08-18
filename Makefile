CXX ?= g++

MIN_CXX_VER := 13

STD := -std=c++20
OPTFLAGS := -O3
DEBUG_FLAGS := -g

CXXFLAGS := -Wall -Wextra -Wpedantic -Wno-interference-size -march=native
DEPFLAGS := -MMD -MP

PKG_CONFIG_PATH := ./build:./third_party/install/lib/pkgconfig:./third_party/install/lib64/pkgconfig
export PKG_CONFIG_PATH

CONAN_PACKAGES := \
		  cpr \
		  rocksdb \
		  libblake3 \
		  libcap \
		  flatbuffers \
		  libarchive

LOCAL_INSTALL := ./third_party/install

CPPFLAGS := \
	$(STD) \
	$(CXXFLAGS) \
	$(OPTFLAGS) \
	$(DEPFLAGS) \
	-I./include \
	-I./third_party \
	-I./gen_schemas \
	-I$(LOCAL_INSTALL)/include \
	$(shell pkg-config --cflags $(CONAN_PACKAGES))

DEBUG_CPPFLAGS := \
		  $(STD) \
		  $(CXXFLAGS) \
		  $(DEBUG_FLAGS) \
		  $(DEPFLAGS) \
		  -I./include \
		  -I./third_party \
		  -I./gen_schemas \
		  -I$(LOCAL_INSTALL)/include \
		  $(shell pkg-config --cflags $(CONAN_PACKAGES))

LDFLAGS := \
	   -L$(LOCAL_INSTALL)/lib -L$(LOCAL_INSTALL)/lib64 \
	   $(shell pkg-config --libs $(CONAN_PACKAGES)) \
	   -lsdbus-c++ \
	   -lsystemd \
	   -lseccomp \
	   -lutil \
	   -lacl \
	   -lcriu

FLATC_CC := flatc
FLATC_CCFLAGS := --cpp

INCLUDE_DIRS := ./include
THIRD_PARTY_DIR := ./third_party
SRCDIR := ./src
TEST_DIR := ./tests

BINARIES := ./bin/release
DEBUG_BINARIES := ./bin/debug
TEST_BINARIES := ./bin/test

BUILDDIR := ./build/release
DEBUG_BUILDDIR := ./build/debug
TEST_OUT := ./test_out

FLATBUFFERS_SCHEMAS_DIR := ./flatbuffer_schemas
GENERATED_SCHEMAS_DIR := ./gen_schemas

FLATBUFFERS_SCHEMAS := $(wildcard $(FLATBUFFERS_SCHEMAS_DIR)/*.fbs)

CPPFILES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS := $(CPPFILES:$(SRCDIR)/%.cpp=$(BINARIES)/%.o)
DEBUG_OBJECTS := $(CPPFILES:$(SRCDIR)/%.cpp=$(DEBUG_BINARIES)/%.o)

MAIN_OBJ := $(BINARIES)/main.o
TESTABLE_OBJS := $(filter-out $(MAIN_OBJ),$(OBJECTS))

TEST_FILES := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS := $(TEST_FILES:$(TEST_DIR)/%.cpp=$(TEST_BINARIES)/%.o)
TEST_EXEC := $(TEST_OUT)/run_tests

CXX_PATH := $(shell command -v $(CXX) 2>/dev/null)
CXX_VERSION := $(shell $(CXX) -dumpversion | cut -f1 -d.)
IS_SUPPORTED := $(shell [ "$(CXX_VERSION)" -ge "$(MIN_CXX_VER)" ] && echo true || echo false)

FLATC_CC_PATH := $(shell command -v $(FLATC_CC) 2>/dev/null)

ifeq ($(CXX_PATH),)
$(error "$(CXX) not found")
endif

ifeq ($(IS_SUPPORTED),false)
$(error GCC/G++ >= $(MIN_CXX_VER) required)
endif

ifeq ($(FLATC_CC_PATH),)
$(error flatc not found)
endif

all: check-deps generate-schemas release

release: $(OBJECTS)
	@mkdir -p $(BUILDDIR)
	$(CXX) -o $(BUILDDIR)/quiver $^ $(LDFLAGS)

debug: $(DEBUG_OBJECTS)
	@mkdir -p $(DEBUG_BUILDDIR)
	$(CXX) -o $(DEBUG_BUILDDIR)/quiver $^ $(LDFLAGS)

$(BINARIES)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BINARIES)
	$(CXX) $(CPPFLAGS) -c $< -o $@

$(DEBUG_BINARIES)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(DEBUG_BINARIES)
	$(CXX) $(DEBUG_CPPFLAGS) -c $< -o $@

generate-schemas: $(FLATBUFFERS_SCHEMAS)
	@mkdir -p $(GENERATED_SCHEMAS_DIR)
	$(FLATC_CC) $(FLATC_CCFLAGS) -o $(GENERATED_SCHEMAS_DIR) $^

check-deps:
	@pkg-config --exists $(CONAN_PACKAGES) || { \
		echo "Run: conan install . --output-folder build --build=missing"; \
		exit 1; \
		}
	@pkg-config --exists sdbus-c++ || { \
		echo "Missing sdbus-c++"; \
		exit 1; \
		}
	@ld -L./third_party/install/lib -L./third_party/install/lib64 -lseccomp -o /dev/null >/dev/null 2>&1 || { \
		echo "Missing libseccomp"; \
		exit 1; \
		}
	@command -v flatc >/dev/null || { \
		echo "flatc not found"; \
		exit 1; \
		}
	@echo "Dependencies OK"

test: $(TEST_EXEC)
	./$(TEST_EXEC)

$(TEST_EXEC): $(TEST_OBJS) $(TESTABLE_OBJS)
	@mkdir -p $(TEST_OUT)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(TEST_BINARIES)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(TEST_BINARIES)
	$(CXX) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf \
		./bin \
		./build/release \
		./build/debug \
		$(TEST_OUT) \
		$(GENERATED_SCHEMAS_DIR)

.PHONY: \
	all \
	release \
	debug \
	test \
	clean \
	check-deps \
	generate-schemas

-include $(OBJECTS:.o=.d)
-include $(DEBUG_OBJECTS:.o=.d)
-include $(TEST_OBJS:.o=.d)
