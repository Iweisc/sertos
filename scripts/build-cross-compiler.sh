#!/bin/bash

set -e

export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

BINUTILS_VERSION="2.41"
GCC_VERSION="13.2.0"

mkdir -p "$HOME/src"
cd "$HOME/src"

echo "=== Building i686-elf cross compiler ==="
echo "This will install to: $PREFIX"
echo ""

if [ ! -f "binutils-${BINUTILS_VERSION}.tar.xz" ]; then
    echo "Downloading binutils..."
    wget "https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz"
fi

if [ ! -f "gcc-${GCC_VERSION}.tar.xz" ]; then
    echo "Downloading GCC..."
    wget "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz"
fi

if [ ! -d "binutils-${BINUTILS_VERSION}" ]; then
    echo "Extracting binutils..."
    tar -xf "binutils-${BINUTILS_VERSION}.tar.xz"
fi

if [ ! -d "gcc-${GCC_VERSION}" ]; then
    echo "Extracting GCC..."
    tar -xf "gcc-${GCC_VERSION}.tar.xz"
fi

echo "Building binutils..."
mkdir -p build-binutils
cd build-binutils
../binutils-${BINUTILS_VERSION}/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
make install
cd ..

echo "Building GCC..."
mkdir -p build-gcc
cd build-gcc
../gcc-${GCC_VERSION}/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c --without-headers
make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc
make install-gcc
make install-target-libgcc
cd ..

echo ""
echo "=== Cross compiler built successfully! ==="
echo ""
echo "Add this to your ~/.bashrc or ~/.zshrc:"
echo "  export PATH=\"\$HOME/opt/cross/bin:\$PATH\""
echo ""
echo "Then run: source ~/.bashrc"
