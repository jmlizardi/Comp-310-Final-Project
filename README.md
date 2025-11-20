# Comp-310-Final-Project

Final Project for Comp310: Creating a stripped down Docker (w/o docker)

## Overview

**Baby Docker** is a minimal container runtime implementation in C that demonstrates the core concepts behind container technology without using Docker itself. This educational project implements fundamental containerization features including namespace isolation, filesystem isolation, and basic resource limits.

## Quick Start

```bash
# Build the project
make

# Run a container (requires root)
sudo ./container ./rootfs /bin/sh
```

## Features

- ✅ PID namespace isolation
- ✅ Mount namespace isolation
- ✅ Network namespace isolation
- ✅ UTS namespace isolation (hostname)
- ✅ IPC namespace isolation
- ✅ Filesystem isolation using pivot_root
- ✅ Basic cgroup memory limits

## Documentation

For detailed usage instructions, see [USAGE.md](USAGE.md)

## Requirements

- Linux kernel with namespace support (3.8+)
- GCC compiler
- Root privileges
- A root filesystem for the container

## Project Structure

```
.
├── container.c    # Main container runtime implementation
├── Makefile       # Build configuration
├── README.md      # This file
└── USAGE.md       # Detailed usage guide
```

## What is Container Isolation?

This project uses Linux kernel features to create isolated environments:

1. **Namespaces** - Isolate processes, network, mounts, hostname, and IPC
2. **pivot_root** - Change the root filesystem visible to the container
3. **cgroups** - Limit resources like memory and CPU

Together, these create a lightweight, isolated environment similar to what Docker provides.

## Learning Objectives

This project helps understand:
- How containers work at the kernel level
- Linux namespace system calls
- Filesystem isolation techniques
- Process isolation and management
- Basic resource limiting

## License

Educational project for Comp 310.
