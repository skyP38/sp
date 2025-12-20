# eBPF Profiler: Анализатор производительности, lock contention + false sharing

## Описание проекта

**eBPF Profiler** — это инструмент профилирования и анализа производительности, построенный на технологии eBPF (extended Berkeley Packet Filter). Он позволяет в реальном времени отслеживать:

- **Время выполнения системных вызовов**
- **Конкуренцию за блокировки (lock contention)**
- **Ложное разделение кэша (false sharing)**
- **Паттерны доступа к памяти**

Проект особенно полезен для разработчиков высоконагруженных приложений, где проблемы производительности трудно обнаружить традиционными методами.

## Особенности

### Основные возможности
- **Низкоуровневое профилирование** с минимальным оверхедом
- **Обнаружение false sharing** через анализ паттернов доступа к кэш-линиям
- **Анализ lock contention** с детальной статистикой ожидания
- **Фильтрация процессов** для фокусировки на целевых приложениях
- **Интеграция с perf** для сбора дополнительных метрик кэша

### Собираемые метрики
| Метрика | Описание | Точность |
|---------|----------|----------|
| Длительность системных вызовов | Время выполнения syscalls | Наносекунды |
| Время ожидания блокировок | Lock contention latency | Наносекунды |
| Доступы к кэш-линиям | Количество и распределение по CPU | Адрес кэш-линии |
| Максимальное количество ожидающих | Пиковая нагрузка на блокировку | Количество потоков |

## Архитектура

### Компоненты системы

```
┌─────────────────────────────────────────────────┐
│          Пользовательское пространство          │
├─────────────────────────────────────────────────┤
│  profiler.c      │  Анализ данных   │  Вывод    │
│  (управление)    │  и статистика    │  в консоль│
└──────────────────┴──────────────────┴───────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────┐
│              Ядро Linux (eBPF)                  │
├─────────────────────────────────────────────────┤
│  profiler.bpf.c  │  Карты eBPF     │  Ring      │
│  (трассировка)   │  (данные)       │  Buffer    │
└──────────────────┴─────────────────┴────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────┐
│          Отслеживаемые приложения               │
│  Системные вызовы │  Блокировки   │  Доступ к   │
│                   │               │  памяти     │
└─────────────────────────────────────────────────┘
```

## Структура проекта

```
profiler/
├── include/                   # Заголовочные файлы
│   ├── profiler.h             # Основные структуры данных
│   ├── ebpf_profiler.h        # Типы для eBPF программы
│   ├── common.h               # Утилиты и макросы
├── src/                       # Исходный код
│   ├── profiler.bpf.c         # eBPF программа (ядро)
│   └── profiler.c             # Пользовательская часть
├── tests/                     # Тестовые программы
│   ├── test_false_sharing.c   # Демо false sharing
│   └── test_lock_contention.c # Демо lock contention
├── build/                     # Директория сборки
│   ├── profiler               # Исполняемый файл
│   └── profiler.skel.h        # Автогенерируемый скелетон
├── vmlinux.h                  # Заголовки ядра (генерируется)
├── CMakeLists.txt             # Файл сборки CMake
└── README.md                  # Документация
```

### Типы используемых eBPF карт
- **Ring Buffer** (`events`) — передача событий в пользовательское пространство
- **Hash Map** (`start_times`) — время начала операций
- **Per-CPU Hash** (`stats`) — статистика системных вызовов
- **LRU Hash** (`cache_line_stats`) — статистика доступа к кэш-линиям
- **Hash Map** (`lock_stats`) — статистика блокировок

## Использование

### Базовый запуск
```bash
# Запуск профилировщика (требуются права root)
sudo ./profiler
```

### Параметры командной строки
```bash
# Режим суммарной статистики
sudo ./build/profiler --summary

# Установка минимальной длительности для отображения
sudo ./build/profiler --min-duration 5000  # 5 микросекунд

# Обнаружение false sharing
sudo ./build/profiler --detect-false-sharing

# Обнаружение false sharing с запуском тестовой программы
sudo ./build/profiler --detect-false-sharing ./tests/test_false_sharing

# Исключение конкретного PID
sudo ./build/profiler --exclude-pid 1234

# Исключение всех системных процессов (PID < 1000)
sudo ./build/profiler --exclude-system

# Показать статистику по блокировкам
sudo ./build/profiler --show-locks

# Комбинированные параметры
sudo ./build/profiler --summary --show-locks --min-duration 10000
```

### Запуск тестовых программ
```bash
# Тест false sharing
cd tests
gcc -o test_false_sharing test_false_sharing.c -pthread -O2
./test_false_sharing

# Тест lock contention
gcc -o test_lock_contention test_lock_contention.c -pthread -O2
./test_lock_contention

# Запуск с профилировщиком
sudo ../build/profiler --detect-false-sharing ./test_false_sharing
```

## Примеры

### Пример 1: Обнаружение false sharing
```bash
$ sudo ./profiler --detect-false-sharing ./tests/test_false_sharing

=== eBPF Profiler Starting ===
[INFO] Profiler loaded successfully!
[INFO] False sharing detection: ENABLED
[INFO] Min duration: 1000 ns

=== False Sharing Test Results ===
With false sharing: 3.45 seconds
Without false sharing: 1.12 seconds
Slowdown: 3.1x

[WARN] Suspected false sharing: 0x7ffd4a3b2f00
[INFO] Accesses: 150234, CPUs: 2
[WARN] Found 1 potential false sharing cases
```

### Пример 2: Анализ lock contention
```bash
$ sudo ./profiler --show-locks ./tests/test_lock_contention

=== Lock Contention Statistics ===
Lock Address          Contention Count   Avg Wait (us)   Max Waiters
-------------------------------------------------------------------------
0x7ffd4a3b2f80        1245                45.2            4
0x7ffd4a3b2fc0        892                 32.1            3
  ^ High contention lock detected!

Total locks monitored: 2
High contention locks: 1
```

### Пример 3: Профилирование системных вызовов
```bash
$ sudo ./profiler --min-duration 100000  # Только вызовы > 100 мкс

[INFO] SYSCALL: pid=5678, syscall=1 (write), duration=125000ns
[INFO] SYSCALL: pid=5678, syscall=0 (read), duration=234000ns
[INFO] LOCK: pid=5678, lock=0x7ffd4a3b2f80, wait=45000us
```