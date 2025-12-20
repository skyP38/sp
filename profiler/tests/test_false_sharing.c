#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>

#define CACHE_LINE_SIZE 64
#define ITERATIONS 100000000

// есть false sharing
struct __attribute__((packed)) bad_struct {
    volatile long counter1;
    volatile long counter2;  // в той же кэш линии
};

// нет false sharing
struct good_struct {
    volatile long counter1;
    char padding[CACHE_LINE_SIZE - sizeof(long)];
    volatile long counter2;
};

struct bad_struct bad;
struct good_struct good;
atomic_int ready = 0;

void *bad_worker(void *arg) {
    int id = *(int*)arg;
    atomic_fetch_add(&ready, 1);
    while (atomic_load(&ready) < 2) {}
    
    if (id == 0) {
        for (long i = 0; i < ITERATIONS; i++) bad.counter1++;
    } else {
        for (long i = 0; i < ITERATIONS; i++) bad.counter2++;
    }
    return NULL;
}

void *good_worker(void *arg) {
    int id = *(int*)arg;
    atomic_fetch_add(&ready, 1);
    while (atomic_load(&ready) < 2) {}
    
    if (id == 0) {
        for (long i = 0; i < ITERATIONS; i++) good.counter1++;
    } else {
        for (long i = 0; i < ITERATIONS; i++) good.counter2++;
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;
    struct timespec start, end;

    bad.counter1 = 0;
    bad.counter2 = 0;
    good.counter1 = 0;
    good.counter2 = 0;

    printf("=== eBPF Profiler False Sharing Demo ===\n");
    printf("Cache line size: %d bytes\n", CACHE_LINE_SIZE);
    printf("Bad struct size: %zu bytes (GUARANTEED false sharing)\n", sizeof(bad));
    printf("Good struct size: %zu bytes (no false sharing)\n", sizeof(good));
    printf("Iterations: %d\n", ITERATIONS);

    // Тест 1: С false sharing
    printf("\n1. Testing WITH false sharing:\n");
    bad.counter1 = bad.counter2 = 0;
    atomic_store(&ready, 0);

    clock_gettime(CLOCK_MONOTONIC, &start);
    int ids[] = {0, 1};
    pthread_create(&t1, NULL, bad_worker, &ids[0]);
    pthread_create(&t2, NULL, bad_worker, &ids[1]);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    clock_gettime(CLOCK_MONOTONIC, &end);

    double bad_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("   Time: %.3f seconds\n", bad_time);
    printf("   Counters: %ld, %ld\n", bad.counter1, bad.counter2);

    // Тест без false sharing
    printf("\n2. Testing WITHOUT false sharing:\n");
    good.counter1 = good.counter2 = 0;
    atomic_store(&ready, 0);

    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_create(&t1, NULL, good_worker, &ids[0]);
    pthread_create(&t2, NULL, good_worker, &ids[1]);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double good_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("   Time: %.3f seconds\n", good_time);
    printf("   Counters: %ld, %ld\n", good.counter1, good.counter2);


    printf("\n=== RESULTS ===\n");
    printf("False sharing: %.2f seconds\n", bad_time);
    printf("No false sharing: %.2f seconds\n", good_time);
    printf("Slowdown: %.1fx\n", bad_time / good_time);

    return 0;
}
