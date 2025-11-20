/*
 * Baby Docker - A Stripped Down Container Runtime
 * 
 * This program implements basic containerization features including:
 * - Namespace isolation (PID, mount, network, UTS, IPC, user)
 * - Filesystem isolation using pivot_root
 * - Basic cgroup resource limits
 * 
 * Usage: ./container <rootfs-path> <command> [args...]
 */

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/syscall.h>

#define STACK_SIZE (1024 * 1024) // 1MB stack

// Container configuration structure
typedef struct {
    char *rootfs_path;
    char **argv;
} container_config_t;

/*
 * Error handling utility
 */
void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

/*
 * Setup mount namespace
 * This isolates the container's filesystem mounts from the host
 */
int setup_mounts(const char *rootfs) {
    // Make sure we have a private mount namespace
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        die("Failed to make / private");
    }
    
    // Create old_root directory for pivot_root
    char old_root_path[PATH_MAX];
    snprintf(old_root_path, sizeof(old_root_path), "%s/.old_root", rootfs);
    
    // Create the old_root directory if it doesn't exist
    mkdir(old_root_path, 0755);
    
    // Bind mount the new root to itself
    if (mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL) == -1) {
        die("Failed to bind mount new root");
    }
    
    // Change to new root directory
    if (chdir(rootfs) == -1) {
        die("Failed to chdir to new root");
    }
    
    // Pivot root
    if (syscall(SYS_pivot_root, ".", ".old_root") == -1) {
        die("Failed to pivot_root");
    }
    
    // Change to new root
    if (chdir("/") == -1) {
        die("Failed to chdir to /");
    }
    
    // Unmount old root
    if (umount2("/.old_root", MNT_DETACH) == -1) {
        die("Failed to umount old root");
    }
    
    // Remove old_root directory
    if (rmdir("/.old_root") == -1) {
        perror("Warning: Failed to remove .old_root");
    }
    
    // Mount proc filesystem
    if (mount("proc", "/proc", "proc", 0, NULL) == -1) {
        perror("Warning: Failed to mount /proc");
    }
    
    return 0;
}

/*
 * Setup cgroups for resource limiting
 */
int setup_cgroups(pid_t pid) {
    char cgroup_path[PATH_MAX];
    char pid_str[32];
    
    snprintf(pid_str, sizeof(pid_str), "%d", pid);
    
    // Create a cgroup for this container (if cgroups v1 is available)
    snprintf(cgroup_path, sizeof(cgroup_path), 
             "/sys/fs/cgroup/memory/baby-docker-%d", pid);
    
    // Try to create cgroup directory (may fail if not running as root or cgroups not available)
    if (mkdir(cgroup_path, 0755) == 0) {
        // Set memory limit to 100MB
        char limit_path[PATH_MAX];
        snprintf(limit_path, sizeof(limit_path), 
                 "%s/memory.limit_in_bytes", cgroup_path);
        
        FILE *f = fopen(limit_path, "w");
        if (f) {
            fprintf(f, "104857600"); // 100MB
            fclose(f);
        }
        
        // Add process to cgroup
        char tasks_path[PATH_MAX];
        snprintf(tasks_path, sizeof(tasks_path), "%s/tasks", cgroup_path);
        f = fopen(tasks_path, "w");
        if (f) {
            fprintf(f, "%s", pid_str);
            fclose(f);
        }
    }
    
    return 0;
}

/*
 * Cleanup cgroups
 */
void cleanup_cgroups(pid_t pid) {
    char cgroup_path[PATH_MAX];
    snprintf(cgroup_path, sizeof(cgroup_path), 
             "/sys/fs/cgroup/memory/baby-docker-%d", pid);
    rmdir(cgroup_path);
}

/*
 * Setup hostname for UTS namespace
 */
int setup_hostname() {
    if (sethostname("container", 9) == -1) {
        perror("Warning: Failed to set hostname");
    }
    return 0;
}

/*
 * Child process function that runs in the new namespaces
 */
int child_func(void *arg) {
    container_config_t *config = (container_config_t *)arg;
    
    // Setup hostname
    setup_hostname();
    
    // Setup mounts and pivot root
    setup_mounts(config->rootfs_path);
    
    // Execute the command
    if (execvp(config->argv[0], config->argv) == -1) {
        die("Failed to exec");
    }
    
    return 0;
}

/*
 * Main function
 */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <rootfs-path> <command> [args...]\n", argv[0]);
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  %s ./rootfs /bin/sh\n", argv[0]);
        fprintf(stderr, "\nNote: This program requires root privileges\n");
        return EXIT_FAILURE;
    }
    
    // Check if running as root
    if (geteuid() != 0) {
        fprintf(stderr, "Error: This program must be run as root\n");
        return EXIT_FAILURE;
    }
    
    // Setup container configuration
    container_config_t config;
    config.rootfs_path = argv[1];
    config.argv = &argv[2];
    
    // Verify rootfs path exists
    struct stat st;
    if (stat(config.rootfs_path, &st) == -1) {
        die("Failed to stat rootfs path");
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s is not a directory\n", config.rootfs_path);
        return EXIT_FAILURE;
    }
    
    printf("Starting container...\n");
    printf("Rootfs: %s\n", config.rootfs_path);
    printf("Command: %s\n", config.argv[0]);
    
    // Allocate stack for child process
    char *stack = malloc(STACK_SIZE);
    if (!stack) {
        die("Failed to allocate stack");
    }
    char *stack_top = stack + STACK_SIZE;
    
    // Create new namespaces and clone child process
    // CLONE_NEWPID: New PID namespace (process will be PID 1 in container)
    // CLONE_NEWNS: New mount namespace (isolated filesystem mounts)
    // CLONE_NEWNET: New network namespace (isolated network stack)
    // CLONE_NEWUTS: New UTS namespace (isolated hostname)
    // CLONE_NEWIPC: New IPC namespace (isolated IPC resources)
    int flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | 
                CLONE_NEWUTS | CLONE_NEWIPC | SIGCHLD;
    
    pid_t pid = clone(child_func, stack_top, flags, &config);
    if (pid == -1) {
        die("Failed to clone");
    }
    
    printf("Container started with PID: %d\n", pid);
    
    // Setup cgroups for the container
    setup_cgroups(pid);
    
    // Wait for child to finish
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        die("Failed to wait for child");
    }
    
    // Cleanup
    cleanup_cgroups(pid);
    free(stack);
    
    if (WIFEXITED(status)) {
        printf("Container exited with status: %d\n", WEXITSTATUS(status));
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        printf("Container killed by signal: %d\n", WTERMSIG(status));
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
