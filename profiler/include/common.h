#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Утилиты для вывода
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

#define LOG_INFO(fmt, ...) \
    printf(COLOR_CYAN "[INFO] " COLOR_RESET fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    printf(COLOR_YELLOW "[WARN] " COLOR_RESET fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, COLOR_RED "[ERROR] " COLOR_RESET fmt, ##__VA_ARGS__)

#define LOG_SUCCESS(fmt, ...) \
    printf(COLOR_GREEN "[SUCCESS] " COLOR_RESET fmt, ##__VA_ARGS__)

// Проверка ошибок
#define CHECK(cond, msg, ...) \
    do { \
        if (!(cond)) { \
            LOG_ERROR(msg, ##__VA_ARGS__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define CHECK_ERR(err, msg, ...) \
    do { \
        if (err) { \
            LOG_ERROR(msg ": %s\n", ##__VA_ARGS__, strerror(-err)); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#endif // COMMON_H