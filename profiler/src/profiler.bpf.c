#include "../vmlinux.h"
#include "../include/ebpf_profiler.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

// Ring buffer для передачи событий в пользовательское пространство
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} events SEC(".maps");

// Хранит время начала системных вызовов по TID
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, __u64);
    __type(value, __u64);
} start_times SEC(".maps");

// Статистика системных вызовов
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_HASH);
    __uint(max_entries, 512);
    __type(key, __u32);
    __type(value, struct syscall_stats);
} stats SEC(".maps");

// Статистика contention блокировок
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u64);
     __type(value, struct lock_contention);
} lock_stats SEC(".maps");

// Время начала ожидания блокировки по TID
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, __u64);   // tid
    __type(value, __u64); // timestamp
} lock_attempts SEC(".maps");

// Количество ожидающих по адресу блокировки
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u64);
    __type(value, __u32);
} lock_waiters SEC(".maps");

// Фильтр PID для исключения процессов из трассировки
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_FILTER_PIDS);
    __type(key, __u32);
    __type(value, __u32);
} filter_pids SEC(".maps");

// Статистика доступа к кэш-линиям (для обнаружения false sharing)
struct {
    __uint(type, BPF_MAP_TYPE_LRU_HASH);
    __uint(max_entries, 2048);
    __type(key, __u64);
    __type(value, struct cache_line_access);
} cache_line_stats SEC(".maps");

// Отслеживание пользовательских процессов
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32);
    __type(value, __u32);
} user_processes SEC(".maps");


// Вспомогательные функции

// Проверяет, нужно ли трассировать системный вызов
static __always_inline bool should_trace_syscall(void) {
    __u32 pid = bpf_get_current_pid_tgid() >> 32;
    if (pid == 0 || pid == 1) return false;

    // Проверяем, есть ли PID в фильтре исключений
    __u32 *value = bpf_map_lookup_elem(&filter_pids, &pid);
    return value == NULL;
}

// Проверяет, нужно ли трассировать доступ к памяти
static __always_inline bool should_trace_memory(void) {
    __u32 pid = bpf_get_current_pid_tgid() >> 32;

    // Трассируем память только для пользовательских процессов
    __u32 *is_user = bpf_map_lookup_elem(&user_processes, &pid);
    return is_user != NULL;
}


// Отслеживание блокировок
static __always_inline void track_lock_acquire(__u64 lock_addr) {
    if (!should_trace_syscall()) return;

    __u64 tid = bpf_get_current_pid_tgid();
    __u64 ts = bpf_ktime_get_ns();

    // Сохраняем время начала ожидания блокировки
    bpf_map_update_elem(&start_times, &tid, &ts, BPF_ANY);

    // Отслеживаем количество ожидающих
    __u32 *waiters = bpf_map_lookup_elem(&lock_waiters, &lock_addr);
    if (waiters) {
        __u32 new_count = *waiters + 1;
        bpf_map_update_elem(&lock_waiters, &lock_addr, &new_count, BPF_ANY);
    } else {
        __u32 initial_count = 1;
        bpf_map_update_elem(&lock_waiters, &lock_addr, &initial_count, BPF_ANY);
    }
}

// Обработка освобождения блокировки
static __always_inline void track_lock_release(__u64 lock_addr) {
    if (!should_trace_syscall()) return;

    __u64 tid = bpf_get_current_pid_tgid();
    __u64 *start_ts = bpf_map_lookup_elem(&start_times, &tid);
    if (!start_ts) return;

    __u64 wait_time = bpf_ktime_get_ns() - *start_ts;

    // Обновляем статистику contention
    struct lock_contention *lc = bpf_map_lookup_elem(&lock_stats, &lock_addr);
    if (!lc) {
        // Получаем текущее количество ожидающих перед уменьшением
        __u32 *current_waiters = bpf_map_lookup_elem(&lock_waiters, &lock_addr);
        __u32 max_waiters = 0;
        
        if (current_waiters) {
            // Текущие ожидающие + текущий поток (только что освободил)
            max_waiters = *current_waiters + 1;
        } else {
            max_waiters = 1; // Только текущий поток
        }
        struct lock_contention new_lc = {
            .lock_addr = lock_addr,
            .contention_count = 1,
            .total_wait_time = wait_time,
            .max_waiters = max_waiters,
            .current_waiters = 0
        };
        bpf_map_update_elem(&lock_stats, &lock_addr, &new_lc, BPF_ANY);
    } else {
        lc->contention_count++;
        lc->total_wait_time += wait_time;

        __u32 *current_waiters = bpf_map_lookup_elem(&lock_waiters, &lock_addr);
        if (current_waiters && *current_waiters > lc->max_waiters) {
            lc->max_waiters += *current_waiters;
        }
        bpf_map_update_elem(&lock_stats, &lock_addr, lc, BPF_ANY);
    }

    // Уменьшаем счетчик ожидающих
    __u32 *waiters = bpf_map_lookup_elem(&lock_waiters, &lock_addr);
    if (waiters && *waiters > 0) {
        __u32 new_count = *waiters - 1;
        bpf_map_update_elem(&lock_waiters, &lock_addr, &new_count, BPF_ANY);
    }

    // Создаем событие для пользовательского пространства
    struct event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (e) {
        e->pid = tid >> 32;
        e->tid = (__u32)tid;
        e->syscall_id = 999; // Специальный ID для событий блокировок
        e->duration_ns = wait_time;
        e->lock_addr = lock_addr;
        e->wait_time = wait_time;
        bpf_ringbuf_submit(e, 0);
    }
}


// Проверяет, является ли адрес пользовательским
static __always_inline bool is_userspace_addr(__u64 addr) {
    return addr <= 0x00007fffffffffffULL;
}


// Функции для системных вызовов

// Запись начала системного вызова
static __always_inline int record_start(int syscall_id) {
    if (!should_trace_syscall()) return 0;

    __u64 tid = bpf_get_current_pid_tgid();
    __u64 ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&start_times, &tid, &ts, BPF_ANY);
    return 0;
}

// Запись завершения системного вызова
static __always_inline int record_exit(int syscall_id) {
    if (!should_trace_syscall()) return 0;

    __u64 tid = bpf_get_current_pid_tgid();
    __u64 *start_ts = bpf_map_lookup_elem(&start_times, &tid);
    if (!start_ts) return 0;

    __u64 duration = bpf_ktime_get_ns() - *start_ts;

    // Обновление статистики
    struct syscall_stats *stat = bpf_map_lookup_elem(&stats, &syscall_id);
    if (!stat) {
        struct syscall_stats new_stat = {1, duration, duration, duration};
        bpf_map_update_elem(&stats, &syscall_id, &new_stat, BPF_ANY);
    } else {
        stat->count++;
        stat->total_duration += duration;
        if (duration > stat->max_duration) stat->max_duration = duration;
        if (duration < stat->min_duration) stat->min_duration = duration;
    }

    // Создание события для ring buffer
    struct event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e) {
        bpf_map_delete_elem(&start_times, &tid);
        return 0;
    }

    e->pid = tid >> 32;
    e->tid = (__u32)tid;
    e->syscall_id = syscall_id;
    e->duration_ns = duration;

    bpf_ringbuf_submit(e, 0);
    bpf_map_delete_elem(&start_times, &tid);
    return 0;
}

// Функция для отслеживания доступа к памяти
static __always_inline void track_memory_access(__u64 addr, bool is_write) {
    if (!should_trace_memory() || addr == 0) return;
    if (!is_userspace_addr(addr)) return;

    // Выравнивание адреса по границе кэш-линии
    __u64 cache_line_addr = addr & ~(CACHE_LINE_SIZE - 1);
    __u32 current_cpu = bpf_get_smp_processor_id();

    struct cache_line_access *stats = bpf_map_lookup_elem(&cache_line_stats, &cache_line_addr);

    if (!stats) {
        // Первое обращение к этой кэш-линии
        struct cache_line_access new_stats = {
            .cache_line_addr = cache_line_addr,
            .access_count = 1,
            .unique_cpus = (1 << (current_cpu % 16)),
            .last_access = bpf_ktime_get_ns()
        };
        bpf_map_update_elem(&cache_line_stats, &cache_line_addr, &new_stats, BPF_ANY);
    } else {
        // Обновление существующей статистики
        if (current_cpu < 16) {
            stats->unique_cpus |= (1 << current_cpu);
        }
        stats->access_count++;
        stats->last_access = bpf_ktime_get_ns();
        bpf_map_update_elem(&cache_line_stats, &cache_line_addr, stats, BPF_ANY);
    }
}


// Обработчик futex
SEC("kprobe/do_futex")
int kprobe_do_futex(struct pt_regs *ctx) {
    __u64 lock_addr = PT_REGS_PARM1(ctx);
    int cmd = PT_REGS_PARM3(ctx); // Команда futex

    __u32 pid = bpf_get_current_pid_tgid() >> 32;
    __u32 tid = bpf_get_current_pid_tgid();

    // FUTEX_WAIT - ожидание блокировки
    if (cmd == 0) {
        track_lock_acquire(lock_addr);

        // Отслеживаем адрес памяти, вызывающий contention
        struct event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
        if (e) {
            e->pid = pid;
            e->tid = tid;
            e->syscall_id = 202; // futex
            e->duration_ns = 0;
            e->lock_addr = lock_addr;
            e->wait_time = 0;
            bpf_ringbuf_submit(e, 0);
        }
    }
    // FUTEX_WAKE - пробуждение ожидающих
    else if (cmd == 1) {
        track_lock_release(lock_addr);
    }

    return 0;
}

// Uprobe для pthread_mutex_lock (пользовательское пространство)
SEC("uprobe")
int uprobe_pthread_mutex_lock(struct pt_regs *ctx) {
    __u64 mutex_ptr = PT_REGS_PARM1(ctx);
    if (mutex_ptr == 0) return 0;
    
    track_lock_acquire(mutex_ptr);
    
    // Отладочный вывод
    struct event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (e) {
        e->pid = bpf_get_current_pid_tgid() >> 32;
        e->tid = (__u32)bpf_get_current_pid_tgid();
        e->syscall_id = 300;  // Код для pthread_mutex_lock
        e->lock_addr = mutex_ptr;
        e->wait_time = 0;
        bpf_ringbuf_submit(e, 0);
    }
    
    return 0;
}

// Uretprobe для pthread_mutex_lock (возврат из функции)
SEC("uretprobe")
int uretprobe_pthread_mutex_lock(struct pt_regs *ctx) {
    __u64 mutex_ptr = PT_REGS_PARM1(ctx);
    if (mutex_ptr == 0) return 0;
    
    track_lock_release(mutex_ptr);
    return 0;
}

// Uprobe для pthread_mutex_unlock
SEC("uprobe")
int uprobe_pthread_mutex_unlock(struct pt_regs *ctx) {
    __u64 mutex_ptr = PT_REGS_PARM1(ctx);
    if (mutex_ptr == 0) return 0;
    
    // Отладочное событие
    struct event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (e) {
        e->pid = bpf_get_current_pid_tgid() >> 32;
        e->tid = (__u32)bpf_get_current_pid_tgid();
        e->syscall_id = 301;  // Код для pthread_mutex_unlock
        e->lock_addr = mutex_ptr;
        bpf_ringbuf_submit(e, 0);
    }
    
    return 0;
}

// Специфичные обработчики для libc
SEC("uprobe//usr/lib/libc.so.6:pthread_mutex_lock")
int uprobe_mutex_lock(struct pt_regs *ctx) {
    __u64 lock_addr = PT_REGS_PARM1(ctx);

    track_lock_acquire(lock_addr);
    return 0;
}

SEC("uretprobe//usr/lib/libc.so.6:pthread_mutex_lock")
int uretprobe_mutex_lock(struct pt_regs *ctx) {
    __u64 lock_addr = PT_REGS_PARM1(ctx);

    track_lock_release(lock_addr);
    return 0;
}


// Обработчики для пользовательских процессов

SEC("kprobe/__x64_sys_mmap")
int kprobe_sys_mmap_enter(struct pt_regs *ctx) {
    // При mmap добавляем процесс в список пользовательских
    __u32 pid = bpf_get_current_pid_tgid() >> 32;
    __u32 one = 1;
    bpf_map_update_elem(&user_processes, &pid, &one, BPF_ANY);

    return record_start(9);  // 9 - mmap
}

SEC("kretprobe/__x64_sys_mmap")
int kretprobe_sys_mmap_exit(struct pt_regs *ctx) {
    return record_exit(9);
}

// Обработчики для отслеживания памяти
SEC("kprobe")
int kprobe_memcpy(struct pt_regs *ctx) {
    if (!should_trace_memory()) return 0;

    __u64 dest = PT_REGS_PARM1(ctx);
    track_memory_access(dest, true);
    return 0;
}

SEC("kprobe")
int kprobe_memset(struct pt_regs *ctx) {
    if (!should_trace_memory()) return 0;

    __u64 dest = PT_REGS_PARM1(ctx);
    track_memory_access(dest, true);
    return 0;
}


// Остальные системные вызовы
SEC("kprobe/__x64_sys_write")
int kprobe_sys_write_enter(struct pt_regs *ctx) {
    return record_start(1);
}

SEC("kretprobe/__x64_sys_write")
int kretprobe_sys_write_exit(struct pt_regs *ctx) {
    return record_exit(1);
}

SEC("kprobe/__x64_sys_read")
int kprobe_sys_read_enter(struct pt_regs *ctx) {
    return record_start(0);
}

SEC("kretprobe/__x64_sys_read")
int kretprobe_sys_read_exit(struct pt_regs *ctx) {
    return record_exit(0);
}

// Лицензия eBPF программы
char _license[] SEC("license") = "GPL";
