// include/ebpf_profiler.h
#ifndef EBPF_PROFILER_H
#define EBPF_PROFILER_H

// Кастомные типы для eBPF (аналоги linux/types.h)
typedef unsigned int __u32;
typedef unsigned long long __u64;
typedef int __s32;
typedef _Bool bool;

#define false 0
#define true 1

// Константы
#define MAX_FILTER_PIDS 100
#define CACHE_LINE_SIZE 64

// Структуры данных для eBPF (совпадают с profiler.h)
struct event {
    __u32 pid;
    __u32 tid;
    __s32 syscall_id;
    __u64 duration_ns;
    __u64 lock_addr;
    __u32 wait_time;
};

struct syscall_stats {
    __u64 count;
    __u64 total_duration;
    __u64 max_duration;
    __u64 min_duration;
};

struct cache_line_access {
    __u64 cache_line_addr;
    __u32 access_count;
    __u32 unique_cpus;
    __u64 last_access;
};

struct lock_contention {
    __u64 lock_addr;
    __u32 contention_count;
    __u64 total_wait_time;
    __u32 max_waiters;
    __u32 current_waiters;
};
#endif // EBPF_PROFILER_H