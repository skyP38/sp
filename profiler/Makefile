CC = gcc
CLANG = clang
BPFTOOL = bpftool

CFLAGS = -g -O2 -Wall -I./include
ARCH := $(shell uname -m | sed 's/x86_64/x86/')
BPF_CFLAGS = -g -O2 -Wall -target bpf -D__TARGET_ARCH_$(ARCH) -I./include 

# Генерация vmlinux.h
VMLINUX_H := include/vmlinux.h

# Директории
INCLUDE_DIR = include
SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build

# Исходные файлы
PROFILER_SRC = $(SRC_DIR)/profiler.c
PROFILER_BPF_SRC = $(SRC_DIR)/profiler.bpf.c
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

# Цели
PROFILER = $(BUILD_DIR)/profiler
TEST_FALSE_SHARING = $(BUILD_DIR)/test_false_sharing
TEST_LOCK_CONTENTION = $(BUILD_DIR)/test_lock_contention
ALL_TARGETS = $(PROFILER) $(TEST_FALSE_SHARING) $(TEST_LOCK_CONTENTION) 

all: $(ALL_TARGETS)


vmlinux.h:
	$(BPFTOOL) btf dump file $(VMLINUX) format c > $@

# Генерация скелетона
%.skel.h: %.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

%.bpf.o: %.bpf.c vmlinux.h
	$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) -I./include \
		-c $< -o $@
	$(LLVM_STRIP) -g $@

# Профайлер
$(PROFILER): $(BUILD_DIR)/profiler.skel.h $(PROFILER_SRC) $(HEADERS)
	@echo "Building profiler..."
	$(CC) $(CFLAGS) -o $@ $(PROFILER_SRC) \
		-I$(BUILD_DIR) \
		-lbpf -lelf -lz

# Генерация скелетона eBPF
$(BUILD_DIR)/profiler.skel.h: $(PROFILER_BPF_SRC) $(HEADERS)
	@echo "Generating BPF skeleton..."
	$(CLANG) $(BPF_CFLAGS) -c $(PROFILER_BPF_SRC) -o $(BUILD_DIR)/profiler.bpf.o
	$(BPFTOOL) gen skeleton $(BUILD_DIR)/profiler.bpf.o > $@
	@echo "Skeleton generated: $@"

# Тесты
$(TEST_FALSE_SHARING): $(TEST_DIR)/test_false_sharing.c $(HEADERS)
	@echo "Building false sharing test..."
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_false_sharing.c -lpthread

$(TEST_LOCK_CONTENTION): $(TEST_DIR)/test_lock_contention.c $(HEADERS)
	@echo "Building lock contention test..."
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_lock_contention.c -lpthread

# Утилиты для запуска
run-profiler: $(PROFILER)
	@echo "Running profiler..."
	sudo $(PROFILER) --detect-false-sharing

run-false-sharing: $(TEST_FALSE_SHARING)
	@echo "Running false sharing test..."
	$(TEST_FALSE_SHARING)

run-lock-contention: $(TEST_LOCK_CONTENTION)
	@echo "Running lock contention test..."
	$(TEST_LOCK_CONTENTION)

# Проверка зависимостей
check-deps:
	@echo "Checking dependencies..."
	@which $(CLANG) > /dev/null || (echo "ERROR: clang not found" && false)
	@which $(BPFTOOL) > /dev/null || (echo "ERROR: bpftool not found" && false)
	@which $(CC) > /dev/null || (echo "ERROR: gcc not found" && false)
	@echo "All dependencies found"

demo: $(PROFILER) $(TEST_FALSE_SHARING)
	@echo "=== eBPF Profiler Demo ==="
	@echo ""
	@echo "1. Open first terminal and run:"
	@echo "   sudo $(PROFILER) --detect-false-sharing"
	@echo ""
	@echo "2. Open second terminal and run:"
	@echo "   $(TEST_FALSE_SHARING)"
	@echo ""
	@echo "Or use commands:"
	@echo "  make run-profiler        - run profiler"
	@echo "  make run-false-sharing   - run false sharing test"
	@echo "  make check-deps          - check dependencies"

# Очистка
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)/*

# Список целей
list:
	@echo "Available targets:"
	@echo "  all               - Build everything"
	@echo "  check-deps        - Check build dependencies"
	@echo "  run-profiler      - Run profiler (sudo)"
	@echo "  run-false-sharing - Run false sharing test"
	@echo "  run-lock-contention - Run lock contention test"
	@echo "  demo              - Show demo instructions"
	@echo "  clean             - Remove build directory"
	@echo "  list              - This help"

.PHONY: all clean list check-deps \
        run-profiler run-false-sharing run-lock-contention demo vmlinux