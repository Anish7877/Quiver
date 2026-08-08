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
REQUIRED_TOOLS="autoconf automake libtool cmake python3 git newuidmap newgidmap fuse-overlayfs passt pkg-config"

for tool in $REQUIRED_TOOLS; do
        if ! command -v $tool >/dev/null 2>&1; then
                echo "Warning: '$tool' is not installed. Attempting to install automatically..."

                PKG_NAME=$tool

        # Detect package manager and adjust package names if necessary
        if command -v apt-get >/dev/null 2>&1; then
                if [ "$tool" = "newuidmap" ] || [ "$tool" = "newgidmap" ]; then PKG_NAME="uidmap"; fi
                sudo apt-get update && sudo apt-get install -y $PKG_NAME

        elif command -v dnf >/dev/null 2>&1; then
                if [ "$tool" = "newuidmap" ] || [ "$tool" = "newgidmap" ]; then PKG_NAME="shadow-utils"; fi
                sudo dnf install -y $PKG_NAME

        elif command -v pacman >/dev/null 2>&1; then
                if [ "$tool" = "newuidmap" ] || [ "$tool" = "newgidmap" ]; then PKG_NAME="shadow"; fi
                sudo pacman -S --noconfirm $PKG_NAME

        else
                echo "Error: '$tool' is not installed and package manager is not supported."
                exit 1
        fi
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
conan install "$BUILD_ROOT" --output-folder="$BUILD_DIR" --build=missing -s compiler.cppstd=gnu20

echo "=== Step 4: Building Local System Dependencies ==="
mkdir -p "$BUILD_ROOT/third_party/src" "$LOCAL_INSTALL/lib/pkgconfig"

if pkg-config --exists libseccomp; then
        echo "--- libseccomp is already installed. Skipping. ---"
else
        echo "--- Building libseccomp ---"
        if [ ! -d "$BUILD_ROOT/third_party/src/libseccomp" ]; then
                git clone https://github.com/seccomp/libseccomp.git "$BUILD_ROOT/third_party/src/libseccomp"
        fi
        cd "$BUILD_ROOT/third_party/src/libseccomp"
        git checkout v2.5.5
        ./autogen.sh
        ./configure --enable-static --disable-shared
        make -j$(nproc)
        sudo make install
fi

# 2. Build libattr (Added automatically before libacl)
if pkg-config --exists libattr; then
        echo "--- libattr is already installed. Skipping. ---"
else
        echo "--- Building libattr ---"
        if [ ! -d "$BUILD_ROOT/third_party/src/attr" ]; then
                git clone https://git.savannah.nongnu.org/git/attr.git "$BUILD_ROOT/third_party/src/attr"
        fi
        cd "$BUILD_ROOT/third_party/src/attr"
        git checkout v2.5.1
        ./autogen.sh
        ./configure --enable-static --disable-shared
        make -j$(nproc)
        sudo make install
fi

# 3. Build libacl
if pkg-config --exists libacl; then
        echo "--- libacl is already installed. Skipping. ---"
else
        echo "--- Building libacl ---"
        if [ ! -d "$BUILD_ROOT/third_party/src/acl" ]; then
                git clone https://git.savannah.nongnu.org/git/acl.git "$BUILD_ROOT/third_party/src/acl"
        fi
        cd "$BUILD_ROOT/third_party/src/acl"
        git checkout v2.3.2
        ./autogen.sh
        ./configure --enable-static --disable-shared
        make -j$(nproc)
        sudo make install
fi

# 4. Build sdbus-c++
if pkg-config --exists sdbus-c++; then
        echo "--- sdbus-c++ is already installed. Skipping. ---"
else
        echo "--- Building sdbus-c++ ---"
        if [ ! -d "$BUILD_ROOT/third_party/src/sdbus-cpp" ]; then
                git clone https://github.com/Kistler-Group/sdbus-cpp.git "$BUILD_ROOT/third_party/src/sdbus-cpp"
        fi
        cd "$BUILD_ROOT/third_party/src/sdbus-cpp"
        git checkout v2.3.1
        rm -rf build

        cmake -S . -B build -DBUILD_SHARED_LIBS=OFF -DSDBUSCPP_BUILD_DOCS=OFF

        cmake --build build -j$(nproc)
        sudo cmake --install build
fi

# 5. Build nlohmann-json
if pkg-config --exists nlohmann_json; then
        echo "--- nlohmann-json is already installed. Skipping. ---"
else
        echo "--- Building nlohmann-json ---"
        if [ ! -d "$BUILD_ROOT/third_party/src/nlohmann-json" ]; then
                git clone https://github.com/nlohmann/json.git "$BUILD_ROOT/third_party/src/nlohmann-json"
        fi
        cd "$BUILD_ROOT/third_party/src/nlohmann-json"
        # Checkout the latest stable release (3.11.3)
        git checkout v3.11.3
        mkdir -p build && cd build
        cmake .. -DJSON_BuildTests=OFF
        make -j$(nproc)
        sudo make install
fi

echo "=== Step 5: Syncing Pkg-Config for Makefile ==="
find "$LOCAL_INSTALL" -name "*.pc" -exec cp {} "$BUILD_DIR/" \;

if [ "$HAS_SPACES" = true ]; then
        echo "=== Step 6: Cleaning up symlink and patching configurations ==="
        find "$ROOT_DIR" -type f \( -name "*.pc" -o -name "*.la" -o -name "*.cmake" \) -exec sed -i "s|$BUILD_ROOT|$ROOT_DIR|g" {} +
        rm -f "$SAFE_LINK"
fi

echo "=== Step 6: Setting up Rootless Cgroups (Universal Shell) ==="

mkdir -p ~/.config/systemd
touch ~/.config/systemd/user.conf

sudo mkdir -p /etc/systemd/system/user@.service.d/
echo -e "[Service]\nDelegate=cpu cpuset io memory pids" | sudo tee /etc/systemd/system/user@.service.d/delegate.conf > /dev/null

sudo systemctl daemon-reload
sudo systemctl restart user@$(id -u).service


mkdir -p ~/.config/environment.d
echo "DBUS_SESSION_BUS_ADDRESS=\"unix:path=/run/user/$(id -u)/bus\"" > ~/.config/environment.d/dbus.conf

sudo tee /etc/profile.d/rootless-dbus.sh > /dev/null << 'EOF'
export DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u)/bus"
EOF
sudo chmod +x /etc/profile.d/rootless-dbus.sh

echo "=== Setup Complete! ==="
echo "Note: Because these are system-wide session changes, you need to completely close your terminal and open a new one (or log out and back in) for them to take effect."
