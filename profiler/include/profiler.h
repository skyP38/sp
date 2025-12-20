#ifndef PROFILER_H
#define PROFILER_H

#include <stdint.h>

// Структуры данных, общие для eBPF и пользовательского пространства
struct event {
    uint32_t pid;           // ID процесса
    uint32_t tid;           // ID потока (thread)
    int32_t syscall_id;     // ID системного вызова
    uint64_t duration_ns;   // Длительность выполнения (наносекунды)
    uint64_t lock_addr;     // Адрес блокировки (для анализа contention)
    uint32_t wait_time;     // Время ожидания блокировки
};

struct syscall_stats {
    uint64_t count;          // Количество вызовов
    uint64_t total_duration; // Общее время выполнения
    uint64_t max_duration;   // Максимальное время выполнения
    uint64_t min_duration;   // Минимальное время выполнения
};

struct cache_line_access {
    uint64_t cache_line_addr; // Адрес кэш-линии 
    uint32_t access_count;    // Количество обращений
    uint32_t unique_cpus;     // Битовое поле уникальных CPU 
    uint64_t last_access;     // Время последнего доступа
};

struct lock_contention {
    uint64_t lock_addr;        // Адрес блокировки
    uint32_t contention_count; // Количество contention-событий
    uint64_t total_wait_time;  // Общее время ожидания
    uint32_t max_waiters;      // Максимальное количество ожидающих
    uint32_t current_waiters;  // Текущее количество ожидающих
};

// Константы
#define MAX_FILTER_PIDS 100       // Максимальное количество PID для фильтрации
#define CACHE_LINE_SIZE 64        // Размер кэш-линии (стандарт для x86)
#define RINGBUF_SIZE (256 * 1024) // Размер ring buffer (256 КБ)

#endif // PROFILER_H