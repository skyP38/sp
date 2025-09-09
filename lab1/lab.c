#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h> 
#include <unistd.h> 


// #define USE_ASM

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_EXIT 60

int64_t sys_open(const char *filename, int flags, int mode);
int64_t sys_read(int fd, void *buf, size_t count);
int64_t sys_write(int fd, const void *buf, size_t count);
int64_t sys_close(int fd);
void sys_exit(int status);

#ifndef USE_ASM
int64_t sys_open(const char *filename, int flags, int mode) {
    printf("Opening file: %s with flags: %d\n", filename, flags);
    return syscall(SYS_OPEN, filename, flags, mode);
}
int64_t sys_read(int fd, void *buf, size_t count) {
    printf("Reading from fd: %d, count: %zu\n", fd, count);
    return syscall(SYS_READ, fd, buf, count);
}
int64_t sys_write(int fd, const void *buf, size_t count) {
    printf("Writing to fd: %d, count: %zu\n", fd, count);
    return syscall(SYS_WRITE, fd, buf, count);
}
int64_t sys_close(int fd) {
    printf("Closing fd: %d\n", fd);
    return syscall(SYS_CLOSE, fd);
}
void sys_exit(int status) {
    syscall(SYS_EXIT, status);
}
#endif


#ifdef USE_ASM
int64_t sys_open(const char *filename, int flags, int mode) {
    printf("ASM: Opening %s with flags: %d, mode: %d\n", filename, flags, mode);
    int64_t ret;
    asm volatile (
        "movq %1, %%rdi\n"   // filename -> rdi
        "movl %2, %%esi\n"   // flags -> esi 
        "movl %3, %%edx\n"   // mode -> edx 
        "movl $2, %%eax\n"   // SYS_OPEN -> eax
        "syscall\n"
        "movq %%rax, %0"     // result -> ret
        : "=r" (ret)
        : "r" (filename), "r" (flags), "r" (mode)
        : "%rax", "%rdi", "%rsi", "%rdx", "memory"
    );
    return ret;
}

int64_t sys_read(int fd, void *buf, size_t count) {
    printf("ASM: Reading from fd: %d, count: %zu\n", fd, count);
    int64_t ret;
    asm volatile (
        "movl %1, %%edi\n"   // fd -> edi
        "movq %2, %%rsi\n"   // buf -> rsi
        "movq %3, %%rdx\n"   // count -> rdx
        "movl $0, %%eax\n"   // SYS_READ -> eax
        "syscall\n"
        "movq %%rax, %0"     // result -> ret
        : "=r" (ret)
        : "r" (fd), "r" (buf), "r" (count)
        : "%rax", "%rdi", "%rsi", "%rdx", "memory"
    );
    return ret;
}

int64_t sys_write(int fd, const void *buf, size_t count) {
    printf("ASM: Writing to fd: %d, count: %zu\n", fd, count);
    int64_t ret;
    asm volatile (
        "movl %1, %%edi\n"   // fd -> edi
        "movq %2, %%rsi\n"   // buf -> rsi
        "movq %3, %%rdx\n"   // count -> rdx
        "movl $1, %%eax\n"   // SYS_WRITE -> eax
        "syscall\n"
        "movq %%rax, %0"     // result -> ret
        : "=r" (ret)
        : "r" (fd), "r" (buf), "r" (count)
        : "%rax", "%rdi", "%rsi", "%rdx", "memory"
    );
    return ret;
}

int64_t sys_close(int fd) {
    printf("ASM: Close fd: %d\n", fd);
    int64_t ret;
    asm volatile (
        "movl %1, %%edi\n"   // fd -> edi
        "movl $3, %%eax\n"   // SYS_CLOSE -> eax
        "syscall\n"
        "movq %%rax, %0"     // result -> ret
        : "=r" (ret)
        : "r" (fd)
        : "%rax", "%rdi", "memory"
    );
    return ret;
}

void sys_exit(int status) {
    asm volatile (
        "movl %0, %%edi\n"   // status -> edi
        "movl $60, %%eax\n"  // SYS_EXIT -> eax
        "syscall\n"
        :
        : "r" (status)
        : "%rax", "%rdi", "memory"
    );
}
#endif

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s if=<input file> of=<output file> [bs=<block size>] [count=<blocks>]\n", prog_name);
    fprintf(stderr, "Example: %s if=file.bin of=copy.bin bs=512 count=10\n", prog_name);
    sys_exit(1);
}

int main(int argc, char *argv[]) {
    char *input_file = NULL;
    char *output_file = NULL;
    size_t block_size = 10; // default = 512
    size_t count = 0; // copy full
    int flags;
    int mode = 0666; // file permissions


    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "if=", 3) == 0) {
            input_file = argv[i] + 3;
        } else if (strncmp(argv[i], "of=", 3) == 0) {
            output_file = argv[i] + 3;
        } else if (strncmp(argv[i], "bs=", 3) == 0) {
            block_size = atol(argv[i] + 3);
            if (block_size <= 0) {
                fprintf(stderr, "Error: Block size must be positive.\n");
                sys_exit(1);
            }
        } else if (strncmp(argv[i], "count=", 6) == 0) {
            count = atol(argv[i] + 6);
        } else {
            fprintf(stderr, "Error: Unknown argument '%s'\n", argv[i]);
            print_usage(argv[0]);
        }
    }

    if (input_file == NULL || output_file == NULL) {
        fprintf(stderr, "Error: Input and output file must be specified.\n");
        print_usage(argv[0]);
    }

    printf("Input file: %s\n", input_file);
    printf("Output file: %s\n", output_file);

    unsigned char *buffer = (unsigned char *)malloc(block_size);
    if (buffer == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for buffer.\n");
        sys_exit(1);
    }

    // open input file
    flags = O_RDONLY; 
    int fd_in = sys_open(input_file, flags, mode);
    if (fd_in < 0) {
        perror("Error opening input file");
        free(buffer);
        sys_exit(1);
    }

    // open/create output file
    flags = O_WRONLY | O_CREAT | O_TRUNC;
    int fd_out = sys_open(output_file, flags, mode);
    if (fd_out < 0) {
        perror("Error opening output file");
        sys_close(fd_in);
        free(buffer);
        sys_exit(1);
    }

    size_t total_bytes_copied = 0;
    size_t blocks_copied = 0;
    int64_t bytes_read;

    while (1) {
        if (count > 0 && blocks_copied >= count) {
            break;
        }

        bytes_read = sys_read(fd_in, buffer, block_size);

        if (bytes_read < 0) {
            perror("Error reading from input file");
            break; 
        }

        if (bytes_read == 0) {
            break;
        }

        int64_t bytes_written = sys_write(fd_out, buffer, bytes_read);
        if (bytes_written < 0) {
            perror("Error writing to output file");
            break;
        }

        total_bytes_copied += bytes_written;
        blocks_copied++;
    }

    fprintf(stderr, "%zu+0 records in\n", blocks_copied);
    fprintf(stderr, "%zu+0 records out\n", blocks_copied);
    fprintf(stderr, "%zu bytes copied\n", total_bytes_copied);

    sys_close(fd_in);
    sys_close(fd_out);
    free(buffer);

    return 0;
}