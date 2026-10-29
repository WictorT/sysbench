# sysbench_prime

Standalone C version of the `sysbench cpu` prime number test (`cpu_execute_event()` in sysbench's `src/tests/cpu/sb_cpu.c`). Each event counts the primes below `max_prime` by trial division up to the square root.

## Build

```
gcc -O2 -pthread -o sysbench_prime sysbench_prime.c -lm
```

## Run

```
./sysbench_prime [max_prime] [seconds] [threads]
```

Defaults: `max_prime` 10000, `seconds` 10, `threads` 1.

```
# 1 thread, 10 seconds
./sysbench_prime 10000 10

# 12 threads
./sysbench_prime 10000 10 12

# all CPU threads
./sysbench_prime 10000 10 $(nproc)
```

## Compare with sysbench

```
sysbench cpu --cpu-max-prime=10000 --threads=1 --time=10 run
sysbench cpu --cpu-max-prime=10000 --threads=$(nproc) --time=10 run
```

## Watch

Run a 1-second test over and over. `watch` waits for each run to finish, then waits the interval, so the screen updates about every 2 seconds. Press `Ctrl+C` to stop.

```
watch -n 1 ./sysbench_prime 10000 1

# highlight changes between runs
watch -d -n 1 ./sysbench_prime 10000 1

# only the events/sec line, all CPU threads
watch -n 1 "./sysbench_prime 10000 1 \$(nproc) | grep 'per second'"

# same with the real sysbench
watch -n 1 "sysbench cpu --cpu-max-prime=10000 --threads=\$(nproc) --time=1 run | grep 'events per second'"
```

The `\$` makes `nproc` run on every refresh rather than once when `watch` starts.

Use the same `seconds` value whenever you compare results: short runs tend to score higher because the CPU hasn't heated up and slowed down yet.
