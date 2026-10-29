/*
 * Standalone version of sysbench's CPU test (src/tests/cpu/sb_cpu.c).
 * Each "event" counts primes in [3, max_prime) by trial division up to sqrt(c).
 * Every thread runs events independently until the time limit, like sysbench --threads=N.
 *
 * Build: gcc -O2 -pthread -o sysbench_prime sysbench_prime.c -lm
 * Usage: ./sysbench_prime [max_prime] [seconds] [threads]
 */
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static unsigned long long max_prime = 10000;
static double seconds = 10.0;
static double start;

/* Same loop as sysbench's cpu_execute_event() */
static unsigned long long cpu_execute_event(void)
{
  unsigned long long c, l, n = 0;
  double t;

  for (c = 3; c < max_prime; c++)
  {
    t = sqrt((double)c);
    for (l = 2; l <= t; l++)
      if (c % l == 0)
        break;
    if (l > t)
      n++;
  }

  return n;
}

static double now(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

typedef struct
{
  unsigned long long events;
  unsigned long long primes;
} worker_t;

static void *worker(void *arg)
{
  worker_t *w = arg;

  do
  {
    w->primes = cpu_execute_event();
    w->events++;
  } while (now() - start < seconds);

  return NULL;
}

int main(int argc, char **argv)
{
  int threads = 1;

  if (argc > 1)
    max_prime = strtoull(argv[1], NULL, 10);
  if (argc > 2)
    seconds = atof(argv[2]);
  if (argc > 3)
    threads = atoi(argv[3]);
  if (threads < 1)
    threads = 1;

  pthread_t *tids = calloc(threads, sizeof(*tids));
  worker_t *workers = calloc(threads, sizeof(*workers));

  start = now();
  for (int i = 0; i < threads; i++)
    pthread_create(&tids[i], NULL, worker, &workers[i]);

  unsigned long long events = 0;
  for (int i = 0; i < threads; i++)
  {
    pthread_join(tids[i], NULL);
    events += workers[i].events;
  }
  double elapsed = now() - start;

  printf("Prime numbers limit: %llu\n", max_prime);
  printf("Threads: %d\n", threads);
  printf("Primes found per event: %llu\n", workers[0].primes);
  printf("Total events: %llu\n", events);
  printf("Total time: %.4fs\n", elapsed);
  printf("events per second: %.2f\n", events / elapsed);

  free(tids);
  free(workers);
  return 0;
}
