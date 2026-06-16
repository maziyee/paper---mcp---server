#!/bin/bash
set -e

echo "========================================"
echo " MCP Framework - Linux Setup"
echo "========================================"
echo ""

# ============================================
# Step 1: Check prerequisites
# ============================================
echo "[1/4] Checking prerequisites..."

for cmd in git cmake g++ python3; do
    if ! command -v $cmd &>/dev/null; then
        echo "  ERROR: $cmd not found. Please install it first."
        echo "  Ubuntu/Debian: sudo apt install build-essential cmake git python3"
        exit 1
    fi
    echo "  $cmd: OK"
done

# ============================================
# Step 2: Setup vcpkg
# ============================================
echo ""
echo "[2/4] Setting up vcpkg..."

if [ -z "$VCPKG_ROOT" ]; then
    VCPKG_ROOT="$HOME/vcpkg"
fi

if [ ! -f "$VCPKG_ROOT/vcpkg" ]; then
    echo "  Cloning vcpkg to $VCPKG_ROOT..."
    git clone https://github.com/Microsoft/vcpkg.git "$VCPKG_ROOT"
    "$VCPKG_ROOT/bootstrap-vcpkg.sh"
fi

echo "  VCPKG_ROOT=$VCPKG_ROOT"

# ============================================
# Step 3: Install C++ dependencies
# ============================================
echo ""
echo "[3/4] Installing C++ dependencies..."

"$VCPKG_ROOT/vcpkg" install spdlog nlohmann-json httplib

# ============================================
# Step 4: Build
# ============================================
echo ""
echo "[4/4] Building project..."

rm -rf build
mkdir build
cd build

cmake .. -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build . -j$(nproc)

cd ..

# ============================================
# Done
# ============================================
echo ""
echo "========================================"
echo " Setup complete!"
echo ""
echo " Test:  ./build/test_mcp"
echo " Run:   ./build/server --no-http"
echo "========================================"
