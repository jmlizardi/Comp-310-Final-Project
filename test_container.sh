#!/bin/bash
#
# Test script for Baby Docker container runtime
# This script creates a minimal rootfs and tests the container
#

set -e

echo "===== Baby Docker Test Script ====="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Error: This test script must be run as root"
    echo "Usage: sudo ./test_container.sh"
    exit 1
fi

# Build the container if needed
if [ ! -f ./container ]; then
    echo "Building container runtime..."
    make
    echo ""
fi

# Create a minimal test rootfs
echo "Creating minimal test rootfs..."
ROOTFS_DIR="./test_rootfs"

# Clean up any existing test rootfs
if [ -d "$ROOTFS_DIR" ]; then
    echo "Cleaning up existing test rootfs..."
    rm -rf "$ROOTFS_DIR"
fi

# Create directory structure
mkdir -p "$ROOTFS_DIR"/{bin,lib,lib64,proc,dev,etc,tmp}

# Copy essential binaries
echo "Copying binaries..."
cp /bin/sh "$ROOTFS_DIR/bin/" 2>/dev/null || cp /usr/bin/sh "$ROOTFS_DIR/bin/"
cp /bin/ls "$ROOTFS_DIR/bin/" 2>/dev/null || cp /usr/bin/ls "$ROOTFS_DIR/bin/"
cp /bin/echo "$ROOTFS_DIR/bin/" 2>/dev/null || cp /usr/bin/echo "$ROOTFS_DIR/bin/"
cp /bin/ps "$ROOTFS_DIR/bin/" 2>/dev/null || cp /usr/bin/ps "$ROOTFS_DIR/bin/" || echo "Warning: ps not copied"

# Copy required shared libraries
echo "Copying shared libraries..."

copy_libs() {
    local binary=$1
    # Get list of libraries and copy them
    ldd "$binary" 2>/dev/null | grep "=> /" | awk '{print $3}' | while read lib; do
        if [ -f "$lib" ]; then
            # Determine target directory based on source path
            if [[ "$lib" == *"/lib/x86_64-linux-gnu/"* ]] || [[ "$lib" == *"/lib/"* ]]; then
                cp -L "$lib" "$ROOTFS_DIR/lib/" 2>/dev/null || true
            elif [[ "$lib" == *"/lib64/"* ]]; then
                cp -L "$lib" "$ROOTFS_DIR/lib64/" 2>/dev/null || true
            else
                # Default to lib
                cp -L "$lib" "$ROOTFS_DIR/lib/" 2>/dev/null || true
            fi
        fi
    done
    
    # Copy dynamic linker (interpreter)
    local interpreter=$(ldd "$binary" 2>/dev/null | grep -o '/lib.*ld-.*\.so[^ ]*' | head -1)
    if [ -n "$interpreter" ] && [ -f "$interpreter" ]; then
        # Create directory structure for the interpreter
        local interp_dir=$(dirname "$interpreter")
        mkdir -p "$ROOTFS_DIR$interp_dir"
        cp -L "$interpreter" "$ROOTFS_DIR$interpreter" 2>/dev/null || true
    fi
}

for binary in "$ROOTFS_DIR/bin/"*; do
    if [ -f "$binary" ]; then
        copy_libs "$binary"
    fi
done

echo ""
echo "Test rootfs created at: $ROOTFS_DIR"
echo ""

# Run tests
echo "===== Running Container Tests ====="
echo ""

echo "Test 1: Run echo command in container"
echo "Command: ./container $ROOTFS_DIR /bin/echo 'Hello from container!'"
./container "$ROOTFS_DIR" /bin/echo "Hello from container!"
echo ""

echo "Test 2: List files in container"
echo "Command: ./container $ROOTFS_DIR /bin/ls -la /"
./container "$ROOTFS_DIR" /bin/ls -la /
echo ""

echo "Test 3: Run shell command"
echo "Command: ./container $ROOTFS_DIR /bin/sh -c 'echo Container shell works!'"
./container "$ROOTFS_DIR" /bin/sh -c "echo Container shell works!"
echo ""

echo "===== Tests Complete ====="
echo ""
echo "To run an interactive shell in the container, use:"
echo "  sudo ./container $ROOTFS_DIR /bin/sh"
echo ""
echo "To clean up the test rootfs:"
echo "  sudo rm -rf $ROOTFS_DIR"
