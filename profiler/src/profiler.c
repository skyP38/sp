#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/resource.h>
#include <string.h>
#include <dirent.h>
#include <sys/wait.h>

#include <linux/types.h>

#include <bpf/libbpf.h>
#include <bpf/bpf.h>

#include "../include/profiler.h"
#include "../include/common.h"
#include "../build/profiler.skel.h"

static int output_mode = 0;                    // Режим вывода: 0 - подробный, 1 - суммарный
static int min_duration_ns = 1000;             // Минимальная длительность события для отображения (нс)
static bool detect_false_sharing = false;      // Флаг обнаружения false sharing
static bool print_lock_stats = false;          // Флаг вывода статистики по блокировкам

static volatile bool exiting = false;          // Флаг завершения программы (volatile для работы в сигнале)

static __u32 exclude_pid = 0;                  // PID процесса для исключения из трассировки
static bool exclude_all_system = false;        // Флаг исключения всех системных процессов (PID < 1000)

static void sig_handler(int sig) {
    exiting = true;
}

// Обработчик событий, получаемых из ring buffer eBPF
static int handle_event(void *ctx, void *data, size_t data_sz) {
    const struct event *e = data;

    // Проверка корректности данных
    if (data_sz < sizeof(*e)) return 0;

    // Фильтрация по минимальной длительности
    if (e->duration_ns < min_duration_ns) return 0;

    if (output_mode == 0 && !detect_false_sharing) {
        if (e->lock_addr != 0) {
            // Событие блокировки
            LOG_INFO("LOCK: pid=%d, lock=0x%lx, wait=%uus\n",
                     e->pid, (unsigned long)e->lock_addr, e->wait_time/1000);
        } else {
            // Событие системного вызова
            LOG_INFO("SYSCALL: pid=%d, syscall=%d, duration=%luns\n",
                     e->pid, e->syscall_id, (unsigned long)e->duration_ns);
        }
    }
    return 0;
}

// Анализирует паттерны доступа к памяти для обнаружения false sharing
void analyze_access_patterns(struct profiler_bpf *skel) {
    if (!detect_false_sharing) return;

    int cache_map_fd = bpf_map__fd(skel->maps.cache_line_stats);
    __u64 cache_line_addr;
    struct cache_line_access stats;

    int suspicious = 0;          // Счетчик подозрительных случаев
    __u64 next_key = 0;          // Для итерации по карте

    // Итерация по всем элементам карты cache_line_stats
    while (bpf_map_get_next_key(cache_map_fd, &next_key, &cache_line_addr) == 0) {
        if (bpf_map_lookup_elem(cache_map_fd, &cache_line_addr, &stats) == 0) {
            // Подсчет количества уникальных CPU, обращавшихся к кэш-линии
            int cpu_count = 0;
            for (int i = 0; i < 32; i++) {
                if (stats.unique_cpus & (1 << i)) cpu_count++;
            }

            if (stats.access_count > 100 && cpu_count > 1) {
                LOG_WARN("Suspected false sharing: 0x%lx\n", (unsigned long)cache_line_addr);
                LOG_INFO("Accesses: %u, CPUs: %d\n", stats.access_count, cpu_count);
                suspicious++;
            }
        }
        next_key = cache_line_addr;
    }

    if (suspicious > 0) {
        LOG_WARN("\nFound %d potential false sharing cases\n", suspicious);
    } else {
        LOG_SUCCESS("No obvious false sharing detected\n");
    }
}

// Выводит статистику по contention блокировок
void print_lock_contention_stats(struct profiler_bpf *skel) {
    int lock_map_fd = bpf_map__fd(skel->maps.lock_stats);
    if (lock_map_fd < 0) {
        LOG_ERROR("Failed to get lock stats map fd\n");
        return;
    }

    __u64 lock_addr;
    struct lock_contention lc;
    
    __u64 next_key = 0;
    int total_locks = 0;             // Общее количество отслеживаемых блокировок
    int high_contention_locks = 0;   // Количество блокировок с высоким contention

    LOG_INFO("\n=== Lock Contention Statistics ===\n");
    LOG_INFO("Lock Address          Contention Count   Avg Wait (us)   Max Waiters\n");
    LOG_INFO("-------------------------------------------------------------------------\n");

    // Итерация по всем блокировкам в карте
    while (bpf_map_get_next_key(lock_map_fd, &next_key, &lock_addr) == 0) {
        if (bpf_map_lookup_elem(lock_map_fd, &lock_addr, &lc) == 0) {
            total_locks++;
            
            // Рассчитываем среднее время ожидания в микросекундах
            __u64 avg_wait_us = (lc.total_wait_time / lc.contention_count) / 1000;
            
            printf("0x%-18lx  %-16u  %-14lu  %-12u\n",
                   (unsigned long)lock_addr,
                   lc.contention_count,
                   (unsigned long)avg_wait_us,
                   lc.max_waiters);

            // Обнаружение высокого contention
            if (lc.contention_count > 100 || avg_wait_us > 1000) {
                high_contention_locks++;
                LOG_WARN("  ^ High contention lock detected!\n");
            }
            
            next_key = lock_addr;
        }
    }

    if (total_locks == 0) {
        LOG_INFO("No lock contention events recorded\n");
    } else {
        LOG_INFO("\nTotal locks monitored: %d\n", total_locks);
        LOG_INFO("High contention locks: %d\n", high_contention_locks);
    }
}

// Функция для запуска perf с программой
static void run_perf_for_program(const char *program) {
    char perf_cmd[1024];

    // Запускаем perf stat для сбора общей статистики кэша
    snprintf(perf_cmd, sizeof(perf_cmd),
             "sudo perf stat -e "
             "cache-misses,cache-references,"
             "L1-dcache-load-misses,L1-dcache-loads,"
             "LLC-load-misses,LLC-loads,cycles,instructions "
             "%s 2>&1", program);
    system(perf_cmd);

    // Запускаем perf record для детального анализа
    snprintf(perf_cmd, sizeof(perf_cmd),
             "sudo perf record -e cache-misses -g -- %s 2>/dev/null && "
             "sudo perf report --stdio --sort=dso,symbol 2>&1 | head -40", program);
    system(perf_cmd);

    // Очищаем временные файлы perf
    system("sudo rm -f perf.data perf.data.old 2>/dev/null");
}

// Парсинг аргументов командной строки
void parse_args(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--summary") == 0) {
            output_mode = 1;
        } else if (strcmp(argv[i], "--min-duration") == 0 && i+1 < argc) {
            min_duration_ns = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--detect-false-sharing") == 0) {
            detect_false_sharing = true;
            if (i+1 < argc && argv[i+1][0] != '-') {
                // Запускаем программу через perf
                run_perf_for_program(argv[i+1]);
                i++; // Пропускаем имя программы
            }
        } else if (strcmp(argv[i], "--exclude-pid") == 0 && i+1 < argc) {
            exclude_pid = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--exclude-system") == 0) {
            exclude_all_system = true;
        } else if (strcmp(argv[i], "--show-locks") == 0) {
            print_lock_stats = true;
        }   else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --summary              Show only statistics\n");
            printf("  --min-duration N       Minimum duration in nanoseconds\n");
            printf("  --detect-false-sharing Enable false sharing detection\n");
            printf("  --exclude-pid PID      Exclude specific PID from tracing\n");
            printf("  --exclude-system       Exclude all system processes (PID < 1000)\n");
            printf("  --show-locks           Show lock contention statistics\n");
            printf("  --help                 Show this help\n");
            exit(0);
        }
    }
}


int main(int argc, char **argv) {
    // Парсинг аргументов командной строки
    parse_args(argc, argv);

    struct profiler_bpf *skel;      // BPF скелетон
    struct ring_buffer *rb = NULL;  // Ring buffer для событий
    int err;                        // Код ошибки

    LOG_INFO("=== eBPF Profiler Starting ===\n");;

    // Увеличение лимита памяти для eBPF
    struct rlimit rlim = {512UL << 20, 512UL << 20};
    setrlimit(RLIMIT_MEMLOCK, &rlim);

    // Открытие скелетона
    skel = profiler_bpf__open();
    CHECK(skel, "Failed to open BPF skeleton\n");

    // Загрузка и верификация BPF программы в ядро
    err = profiler_bpf__load(skel);
    CHECK_ERR(err, "Failed to load BPF skeleton");

    // Исключение процессов из отслеживания
    __u32 pid = getpid();
    __u32 ppid = getppid();
    __u32 value = 1; // Любое ненулевое значение для карты фильтрации
    int filter_map_fd = bpf_map__fd(skel->maps.filter_pids);
    
    // Добавление текущий PID в фильтр
    if (filter_map_fd >= 0) {
        // 1. Всегда исключаем себя и родительский
        bpf_map_update_elem(filter_map_fd, &pid, &value, BPF_ANY);
        
        if (ppid > 1 && ppid != pid) {
            bpf_map_update_elem(filter_map_fd, &ppid, &value, BPF_ANY);
        }

        // 2. Исключаем указанный пользователем PID
        if (exclude_pid > 0) {
            bpf_map_update_elem(filter_map_fd, &exclude_pid, &value, BPF_ANY);
        }

        // 3. Исключаем системные процессы 
        if (exclude_all_system) {
            // Исключаем все PID < 1000
            DIR *dir = opendir("/proc");
            if (dir) {
                struct dirent *entry;
                int system_count = 0;
                while ((entry = readdir(dir)) != NULL) {
                    if (entry->d_type == DT_DIR) {
                        __u32 proc_pid = atoi(entry->d_name);
                        if (proc_pid > 0 && proc_pid < 1000 && proc_pid != pid) {
                            // Проверяем, не исключили ли уже
                            __u32 *check = NULL;
                            if (bpf_map_lookup_elem(filter_map_fd, &proc_pid, &check) != 0) {
                                bpf_map_update_elem(filter_map_fd, &proc_pid, &value, BPF_ANY);
                                system_count++;
                            }
                        }
                    }
                }
                closedir(dir);
            }
        }
    } 

    // Создание ring buffer
    rb = ring_buffer__new(bpf_map__fd(skel->maps.events), handle_event, NULL, NULL);
    CHECK(rb, "Failed to create ring buffer\n");

    // Присоединение bpf программы к точкам трассировки
    err = profiler_bpf__attach(skel);
    CHECK_ERR(err, "Failed to attach BPF program");

    LOG_INFO("Profiler loaded successfully!\n");
    LOG_INFO("False sharing detection: %s\n", detect_false_sharing ? "ENABLED" : "DISABLED");
    LOG_INFO("Min duration: %d ns\n", min_duration_ns);
    LOG_INFO("Press Ctrl+C to stop\n\n");

    // Настройка обработки сигналов
    signal(SIGINT, sig_handler); 

    while (!exiting && !detect_false_sharing) {
        err = ring_buffer__poll(rb, 10000);
        if (err < 0 && err != -EINTR) {
            LOG_ERROR("Error polling ring buffer: %s\n", strerror(-err));
            break;
        }
    }

    LOG_INFO("\n=== Profiler Stopping ===\n");

    // // Вывод статистики
    if (print_lock_stats || output_mode == 1) {
        print_lock_contention_stats(skel);
    }
    return err;
}