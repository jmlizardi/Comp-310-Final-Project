# Baby Docker - A Stripped Down Container Runtime

A minimal container runtime implementation in C that demonstrates core containerization concepts without using Docker.

## Features

This implementation provides:

- **Namespace Isolation**
  - PID namespace: Isolated process tree (container process becomes PID 1)
  - Mount namespace: Isolated filesystem mounts
  - Network namespace: Isolated network stack
  - UTS namespace: Isolated hostname
  - IPC namespace: Isolated inter-process communication

- **Filesystem Isolation**
  - Uses `pivot_root` to change the root filesystem
  - Provides complete filesystem isolation from the host

- **Resource Limiting**
  - Basic cgroup support for memory limits (when available)
  - Limits container to 100MB of memory

## Prerequisites

- Linux kernel with namespace support (Linux 3.8+)
- Root privileges (required for namespace creation and mount operations)
- GCC compiler
- Make build tool

## Building

```bash
make
```

This will compile the `container` binary.

To clean build artifacts:
```bash
make clean
```

## Usage

```bash
sudo ./container <rootfs-path> <command> [args...]
```

### Arguments

- `<rootfs-path>`: Path to the root filesystem directory for the container
- `<command>`: Command to execute inside the container
- `[args...]`: Optional arguments to pass to the command

### Example

```bash
# Run a shell in a container
sudo ./container ./rootfs /bin/sh

# Run a specific command
sudo ./container ./rootfs /bin/ls -la /

# Run with arguments
sudo ./container ./rootfs /bin/echo "Hello from container"
```

## Setting Up a Root Filesystem

To test the container, you need to create a minimal root filesystem. Here are a few options:

### Option 1: Using debootstrap (Debian/Ubuntu)

```bash
# Install debootstrap
sudo apt-get install debootstrap

# Create a minimal Debian rootfs
sudo debootstrap --variant=minbase stable ./rootfs http://deb.debian.org/debian/
```

### Option 2: Using Alpine Linux (Lightweight)

```bash
# Download Alpine mini root filesystem
mkdir -p rootfs
cd rootfs
wget https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/x86_64/alpine-minirootfs-3.18.4-x86_64.tar.gz
sudo tar xzf alpine-minirootfs-3.18.4-x86_64.tar.gz
rm alpine-minirootfs-3.18.4-x86_64.tar.gz
cd ..
```

### Option 3: Manual minimal rootfs

```bash
mkdir -p rootfs/{bin,lib,lib64,proc}

# Copy necessary binaries
sudo cp /bin/sh rootfs/bin/
sudo cp /bin/ls rootfs/bin/
sudo cp /bin/echo rootfs/bin/

# Copy required libraries
# You'll need to find dependencies using ldd and copy them
ldd /bin/sh | grep "=> /" | awk '{print $3}' | xargs -I '{}' sudo cp '{}' rootfs/lib/ 2>/dev/null || true
ldd /bin/sh | grep "/lib64/" | awk '{print $1}' | xargs -I '{}' sudo cp '{}' rootfs/lib64/ 2>/dev/null || true
```

## How It Works

### 1. Namespace Creation

The container uses Linux namespaces to isolate resources:

```c
int flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | 
            CLONE_NEWUTS | CLONE_NEWIPC | SIGCHLD;
pid_t pid = clone(child_func, stack_top, flags, &config);
```

### 2. Filesystem Isolation

The container uses `pivot_root` to change the root filesystem:

1. Bind mount the new rootfs to itself
2. Change to the new rootfs directory
3. Call `pivot_root` to swap root filesystems
4. Unmount the old root
5. Mount `/proc` inside the container

### 3. Process Execution

Once isolated, the container executes the requested command using `execvp`.

### 4. Resource Limiting

If cgroups are available, the container creates a cgroup and limits memory to 100MB.

## Testing the Container

### Test 1: Verify PID Namespace

```bash
# Inside container, you should see the process as PID 1
sudo ./container ./rootfs /bin/sh
# Then in the container:
$ echo $$  # Should show PID 1 or a low number
$ ps aux   # Should show only processes in the container
```

### Test 2: Verify Filesystem Isolation

```bash
# The container should only see files in the rootfs
sudo ./container ./rootfs /bin/ls /
```

### Test 3: Verify Network Isolation

```bash
# Inside the container, network interfaces should be isolated
sudo ./container ./rootfs /bin/sh
# Then in the container:
$ ip link  # or ifconfig (if available)
```

## Limitations

This is a minimal implementation for educational purposes. It does not include:

- Network configuration (containers start with isolated but unconfigured network)
- Volume mounting
- Image management
- Layer filesystem (union mounts)
- Container registry support
- Advanced security features (AppArmor, SELinux)
- User namespace remapping
- Many other Docker features

## Security Considerations

- This program requires root privileges
- Container escape vulnerabilities may exist
- Not suitable for production use
- Intended for educational purposes only

## Troubleshooting

### "Operation not permitted" errors

Make sure you're running as root:
```bash
sudo ./container ./rootfs /bin/sh
```

### "Failed to mount /proc" warning

This is usually safe to ignore. Some minimal rootfs setups may not have a `/proc` directory.

### "Failed to pivot_root" error

Make sure:
1. The rootfs path is a valid directory
2. The rootfs is on a different mount point (may need to bind mount it first)
3. You're running as root

## Learning Resources

- [Linux Namespaces](https://man7.org/linux/man-pages/man7/namespaces.7.html)
- [Control Groups (cgroups)](https://man7.org/linux/man-pages/man7/cgroups.7.html)
- [pivot_root](https://man7.org/linux/man-pages/man2/pivot_root.2.html)
- [clone system call](https://man7.org/linux/man-pages/man2/clone.2.html)

## License

Educational project for Comp 310 Final Project.

## Authors

Created as a course project to understand containerization fundamentals.
