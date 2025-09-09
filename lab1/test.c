#include <stdio.h>
#include <stdint.h>

#define MAX_PATH 4096

int64_t sys_getcwd(char *buf, size_t size) {
    int64_t ret;
    asm volatile (
        "movq $79, %%rax\n"
        "movq %1, %%rdi\n"
        "movq %2, %%rsi\n"
        "syscall\n"
        "movq %%rax, %0\n"
        :"=r" (ret)
        :"r" (buf), "r" (size)
        :"rax", "rdi", "rsi", "rcx", "r11", "memory"
    );
    return ret;
}

int64_t main() {
    char buffer[MAX_PATH];
    int64_t result = sys_getcwd(buffer, MAX_PATH);
    if (result < 0) {
        printf ("Error Code: %ld\n", result);
    }
    printf ("%s\n", buffer);
}