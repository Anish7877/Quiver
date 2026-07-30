#!/usr/bin/env bash
set -e

ROOT_DIR="$(pwd)"
BUILD_ROOT="$ROOT_DIR"
HAS_SPACES=false

echo "=== Step 0: Path Space Check ==="
if [[ "$ROOT_DIR" == *" "* ]]; then
    HAS_SPACES=true
    SAFE_LINK="/tmp/quiver_build_$$"
    echo "Spaces detected in project path."
    echo "Creating a safe symlink at $SAFE_LINK to bypass libtool limitations..."
    rm -f "$SAFE_LINK"
    ln -s "$ROOT_DIR" "$SAFE_LINK"
    BUILD_ROOT="$SAFE_LINK"
fi

BUILD_DIR="$BUILD_ROOT/build"
LOCAL_INSTALL="$BUILD_ROOT/third_party/install"
CONAN_VENV="$BUILD_ROOT/.conan_venv"

echo "=== Step 1: Checking Host Build Tools ==="
REQUIRED_TOOLS="autoconf automake libtool cmake python3 git"
for tool in $REQUIRED_TOOLS; do
    if ! command -v $tool >/dev/null 2>&1; then
        echo "Error: '$tool' is not installed. Please install it via your package manager."
        exit 1
    fi
done

echo "=== Step 2: Setting up Conan Environment ==="
if [ ! -d "$CONAN_VENV" ]; then
    python3 -m venv "$CONAN_VENV"
fi
source "$CONAN_VENV/bin/activate"
pip install --upgrade pip
pip install conan

conan profile detect --force

echo "=== Step 3: Fetching Conan Dependencies ==="
mkdir -p "$BUILD_DIR"
conan install "$BUILD_ROOT" --output-folder="$BUILD_DIR" --build=missing

echo "=== Step 4: Building Local System Dependencies ==="
mkdir -p "$BUILD_ROOT/third_party/src" "$LOCAL_INSTALL/lib/pkgconfig"

# 1. Build libseccomp
echo "--- Building libseccomp ---"
if [ ! -d "$BUILD_ROOT/third_party/src/libseccomp" ]; then
    git clone https://github.com/seccomp/libseccomp.git "$BUILD_ROOT/third_party/src/libseccomp"
fi
cd "$BUILD_ROOT/third_party/src/libseccomp"
git checkout v2.5.5
./autogen.sh
./configure --prefix="$LOCAL_INSTALL" --enable-static --disable-shared
make -j$(nproc)
make install

# 2. Build libacl
echo "--- Building libacl ---"
if [ ! -d "$BUILD_ROOT/third_party/src/acl" ]; then
    git clone https://git.savannah.nongnu.org/git/acl.git "$BUILD_ROOT/third_party/src/acl"
fi
cd "$BUILD_ROOT/third_party/src/acl"
git checkout v2.3.2
./autogen.sh
./configure --prefix="$LOCAL_INSTALL" --enable-static --disable-shared
make -j$(nproc)
make install

# 3. Build sdbus-c++
echo "--- Building sdbus-c++ ---"
if [ ! -d "$BUILD_ROOT/third_party/src/sdbus-cpp" ]; then
    git clone https://github.com/Kistler-Group/sdbus-cpp.git "$BUILD_ROOT/third_party/src/sdbus-cpp"
fi
cd "$BUILD_ROOT/third_party/src/sdbus-cpp"
git checkout v2.3.1
mkdir -p build && cd build
cmake .. -DBUILD_SHARED_LIBS=OFF -DSDBUSCPP_BUILD_DOCS=OFF -DCMAKE_INSTALL_PREFIX="$LOCAL_INSTALL"
make -j$(nproc)
make install

echo "=== Step 5: Syncing Pkg-Config for Makefile ==="
find "$LOCAL_INSTALL" -name "*.pc" -exec cp {} "$BUILD_DIR/" \;

if [ "$HAS_SPACES" = true ]; then
    echo "=== Step 6: Cleaning up symlink and patching configurations ==="
    # Replace the temporary symlink path with the real path in all text-based config files
    find "$ROOT_DIR" -type f \( -name "*.pc" -o -name "*.la" -o -name "*.cmake" \) -exec sed -i "s|$BUILD_ROOT|$ROOT_DIR|g" {} +
    rm -f "$SAFE_LINK"
fi

echo "=== Setup Complete! ==="
