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
REQUIRED_TOOLS="autoconf automake libtool cmake python3 git newuidmap newgidmap fuse-overlayfs pkg-config curl make tar"

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

if ! pkg-config --exists libsystemd; then
        echo "Warning: 'libsystemd-dev' is not installed. Attempting to install automatically..."
        if command -v apt-get >/dev/null 2>&1; then
                sudo apt-get update && sudo apt-get install -y libsystemd-dev
        elif command -v dnf >/dev/null 2>&1; then
                sudo dnf install -y systemd-devel
        elif command -v pacman >/dev/null 2>&1; then
                sudo pacman -S --noconfirm systemd-libs
        else
                echo "Error: libsystemd development files (libsystemd-dev / systemd-devel) are not installed and package manager is not supported."
                exit 1
        fi
fi

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

echo "=== Step 5: Installing pasta from Git ==="
if command -v pasta >/dev/null 2>&1; then
        echo "--- pasta is already installed. Skipping. ---"
else
        echo "--- Building and installing pasta ---"
        mkdir -p "$BUILD_ROOT/third_party/src"
        if [ ! -d "$BUILD_ROOT/third_party/src/passt" ]; then
                git clone https://passt.top/passt "$BUILD_ROOT/third_party/src/passt"
        fi
        cd "$BUILD_ROOT/third_party/src/passt"
        make -j$(nproc)
        sudo make install
        cd "$BUILD_ROOT"
fi

if ! command -v pasta >/dev/null 2>&1; then
        echo "Error: pasta installation failed."
        exit 1
fi

echo "=== Step 6: Installing umoci ==="
if command -v umoci >/dev/null 2>&1; then
        echo "--- umoci is already installed. Skipping. ---"
else
        echo "--- Installing umoci ---"
        INSTALLED=false

        if command -v apt-get >/dev/null 2>&1; then
                echo "Attempting to install umoci via apt-get..."
                if sudo apt-get update && sudo apt-get install -y umoci; then
                        INSTALLED=true
                fi
        elif command -v pacman >/dev/null 2>&1; then
                echo "Attempting to install umoci via pacman..."
                if sudo pacman -S --noconfirm umoci; then
                        INSTALLED=true
                fi
        elif command -v dnf >/dev/null 2>&1; then
                echo "Attempting to install umoci via dnf..."
                if sudo dnf install -y umoci; then
                        INSTALLED=true
                fi
        fi

        if [ "$INSTALLED" = false ] || ! command -v umoci >/dev/null 2>&1; then
                echo "Installing umoci standalone binary from GitHub releases..."
                ARCH="$(uname -m)"
                case "$ARCH" in
                        x86_64)        UMOCI_ARCH="amd64" ;;
                        aarch64|arm64) UMOCI_ARCH="arm64" ;;
                        *)             echo "Error: Unsupported architecture for umoci: $ARCH"; exit 1 ;;
                esac
                sudo curl -fsSL -o /usr/local/bin/umoci "https://github.com/opencontainers/umoci/releases/download/v0.6.0/umoci.linux.${UMOCI_ARCH}"
                sudo chmod +x /usr/local/bin/umoci
        fi
fi

if ! command -v umoci >/dev/null 2>&1; then
        echo "Error: umoci installation failed."
        exit 1
fi

echo "=== Step 7: Installing crun ==="
if command -v crun >/dev/null 2>&1; then
        echo "--- crun is already installed. Skipping. ---"
else
        echo "--- Installing crun ---"
        INSTALLED=false

        if command -v apt-get >/dev/null 2>&1; then
                echo "Attempting to install crun via apt-get..."
                if sudo apt-get update && sudo apt-get install -y crun; then
                        INSTALLED=true
                fi
        elif command -v pacman >/dev/null 2>&1; then
                echo "Attempting to install crun via pacman..."
                if sudo pacman -S --noconfirm crun; then
                        INSTALLED=true
                fi
        elif command -v dnf >/dev/null 2>&1; then
                echo "Attempting to install crun via dnf..."
                if sudo dnf install -y crun; then
                        INSTALLED=true
                fi
        fi

        if [ "$INSTALLED" = false ] || ! command -v crun >/dev/null 2>&1; then
                echo "Installing crun standalone binary from GitHub releases..."
                ARCH="$(uname -m)"
                case "$ARCH" in
                        x86_64)        CRUN_ARCH="linux-amd64" ;;
                        aarch64|arm64) CRUN_ARCH="linux-arm64" ;;
                        *)             echo "Error: Unsupported architecture for crun: $ARCH"; exit 1 ;;
                esac
                sudo curl -fsSL -o /usr/local/bin/crun "https://github.com/containers/crun/releases/download/1.20/crun-1.20-${CRUN_ARCH}"
                sudo chmod +x /usr/local/bin/crun
        fi
fi

if ! command -v crun >/dev/null 2>&1; then
        echo "Error: crun installation failed."
        exit 1
fi

echo "=== Step 8: Installing rootlesskit ==="
if command -v rootlesskit >/dev/null 2>&1; then
        echo "--- rootlesskit is already installed. Skipping. ---"
else
        echo "--- Installing rootlesskit ---"
        INSTALLED=false

        if command -v apt-get >/dev/null 2>&1; then
                echo "Attempting to install rootlesskit via apt-get..."
                if sudo apt-get update && sudo apt-get install -y rootlesskit; then
                        INSTALLED=true
                fi
        elif command -v pacman >/dev/null 2>&1; then
                echo "Attempting to install rootlesskit via pacman..."
                if sudo pacman -S --noconfirm rootlesskit; then
                        INSTALLED=true
                fi
        elif command -v dnf >/dev/null 2>&1; then
                echo "Attempting to install rootlesskit via dnf..."
                if sudo dnf install -y rootlesskit; then
                        INSTALLED=true
                fi
        fi

        if [ "$INSTALLED" = false ] || ! command -v rootlesskit >/dev/null 2>&1; then
                echo "Installing rootlesskit from GitHub releases..."
                ARCH="$(uname -m)"
                case "$ARCH" in
                        x86_64)        RLK_ARCH="x86_64" ;;
                        aarch64|arm64) RLK_ARCH="aarch64" ;;
                        *)             echo "Error: Unsupported architecture for rootlesskit: $ARCH"; exit 1 ;;
                esac
                sudo curl -fsSL "https://github.com/rootless-containers/rootlesskit/releases/download/v2.3.2/rootlesskit-${RLK_ARCH}.tar.gz" | sudo tar -C /usr/local/bin -xz
        fi
fi

if ! command -v rootlesskit >/dev/null 2>&1; then
        echo "Error: rootlesskit installation failed."
        exit 1
fi

echo "=== Step 9: Installing buildkit ==="
if command -v buildkitd >/dev/null 2>&1 && command -v buildctl >/dev/null 2>&1; then
        echo "--- buildkit (buildkitd and buildctl) is already installed. Skipping. ---"
else
        echo "--- Installing buildkit ---"
        INSTALLED=false

        if command -v pacman >/dev/null 2>&1; then
                echo "Attempting to install buildkit via pacman..."
                if sudo pacman -S --noconfirm buildkit; then
                        INSTALLED=true
                fi
        elif command -v apt-get >/dev/null 2>&1; then
                echo "Attempting to install buildkit via apt-get..."
                if sudo apt-get update && sudo apt-get install -y buildkit; then
                        INSTALLED=true
                fi
        elif command -v dnf >/dev/null 2>&1; then
                echo "Attempting to install buildkit via dnf..."
                if sudo dnf install -y buildkit; then
                        INSTALLED=true
                fi
        fi

        if [ "$INSTALLED" = false ] || ! command -v buildkitd >/dev/null 2>&1 || ! command -v buildctl >/dev/null 2>&1; then
                echo "Installing buildkit from GitHub releases..."
                ARCH="$(uname -m)"
                case "$ARCH" in
                        x86_64)        BK_ARCH="linux-amd64" ;;
                        aarch64|arm64) BK_ARCH="linux-arm64" ;;
                        *)             echo "Error: Unsupported architecture for buildkit: $ARCH"; exit 1 ;;
                esac
                sudo curl -fsSL "https://github.com/moby/buildkit/releases/download/v0.20.0/buildkit-v0.20.0.${BK_ARCH}.tar.gz" | sudo tar -C /usr/local -xz
        fi
fi

if ! command -v buildkitd >/dev/null 2>&1 || ! command -v buildctl >/dev/null 2>&1; then
        echo "Error: buildkit installation failed."
        exit 1
fi

echo "=== Step 10: Syncing Pkg-Config for Makefile ==="
find "$LOCAL_INSTALL" -name "*.pc" -exec cp {} "$BUILD_DIR/" \;

if [ "$HAS_SPACES" = true ]; then
        echo "=== Step 11: Cleaning up symlink and patching configurations ==="
        find "$ROOT_DIR" -type f \( -name "*.pc" -o -name "*.la" -o -name "*.cmake" \) -exec sed -i "s|$BUILD_ROOT|$ROOT_DIR|g" {} +
        rm -f "$SAFE_LINK"
fi

echo "=== Step 12: Setting up Rootless Cgroups (Universal Shell) ==="

mkdir -p ~/.config/systemd
touch ~/.config/systemd/user.conf

sudo mkdir -p /etc/systemd/system/user@.service.d/
echo -e "[Service]\nDelegate=cpu cpuset io memory pids" | sudo tee /etc/systemd/system/user@.service.d/delegate.conf > /dev/null



mkdir -p ~/.config/environment.d
echo "DBUS_SESSION_BUS_ADDRESS=\"unix:path=/run/user/$(id -u)/bus\"" > ~/.config/environment.d/dbus.conf

sudo tee /etc/profile.d/rootless-dbus.sh > /dev/null << 'EOF'
export DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u)/bus"
EOF
sudo chmod +x /etc/profile.d/rootless-dbus.sh

echo "=== Setup Complete! ==="
echo "Note: Because these are system-wide session changes, you need to completely close your terminal and open a new one (or log out and back in) for them to take effect."
