# C++17 Thread Pool

A C++17 thread pool implemented from scratch, featuring generic asynchronous task submission, move-only task support, bounded queues, per-worker task queues, mutex-based work stealing, graceful shutdown, stress testing, and benchmarking.

The project was built as a learning-oriented systems programming project with emphasis on concurrency correctness, synchronization design, task scheduling, and performance investigation.

---

## Features

- Generic `submit(F, Args...)` interface
- `std::future` based asynchronous result retrieval
- Exception propagation through `std::packaged_task`
- Custom move-only callable wrapper
- Per-worker task queues
- Mutex-based work stealing
- Bounded queue with producer backpressure
- Multiple concurrent producers
- Graceful shutdown
- Exact-once stress testing
- ThreadSanitizer validation
- Independent benchmark target
- CMake library / executable separation

---

## Project Structure

```text
thread_pool/
├── include/
│   ├── move_only_function.h
│   ├── thread_pool.h
│   └── worker_queue.h
│
├── src/
│   ├── main.cpp
│   ├── thread_pool.cpp
│   └── worker_queue.cpp
│
├── tests/
│   └── stress_test.cpp
│
├── benchmarks/
│   └── benchmark.cpp
│
├── CMakeLists.txt
├── .gitignore
└── README.md
```

The core implementation is built as a reusable library target:

```text
                    thread_pool_demo
                           ↑
                           |
thread_pool_stress ← thread_pool_lib → thread_pool_benchmark
```

---

## Basic Usage

```cpp
#include "thread_pool.h"

#include <iostream>

int add(int a, int b)
{
    return a + b;
}

int main()
{
    ThreadPool pool(4, 16);

    auto future = pool.submit(add, 10, 20);

    std::cout << future.get() << '\n';

    return 0;
}
```

`submit()` returns a `std::future`, allowing the caller to wait for the task result independently of the worker threads.

---

## Task Submission Pipeline

The submission path is approximately:

```text
callable + arguments
        |
        v
    std::bind
        |
        v
std::packaged_task<ReturnType()>
        |
        +------> std::future<ReturnType>
        |
        v
 MoveOnlyFunction
        |
        v
   WorkerQueue
```

`std::packaged_task` connects task execution with the corresponding `std::future`.

If the user task throws an exception, the exception is stored in the shared state and is rethrown when the caller invokes:

```cpp
future.get();
```

The worker thread itself therefore remains alive and can continue executing later tasks.

---

## Why `MoveOnlyFunction`?

In C++17, `std::function` requires its stored callable to be copy constructible.

However, objects such as:

```cpp
std::packaged_task
```

are move-only.

To store move-only tasks directly, this project implements a small type-erased callable wrapper:

```text
MoveOnlyFunction
       |
       v
unique_ptr<CallableBase>
       |
       v
CallableHolder<F>
       |
       v
actual callable
```

Its copy constructor and copy assignment operator are disabled:

```cpp
MoveOnlyFunction(const MoveOnlyFunction&) = delete;
MoveOnlyFunction& operator=(const MoveOnlyFunction&) = delete;
```

while move operations are supported.

This allows `std::packaged_task` and other move-only callables to be transferred through the scheduler without introducing `shared_ptr` solely to satisfy copyability requirements.

---

## Worker Queue Design

Each worker owns a local queue implemented with:

```cpp
std::deque<MoveOnlyFunction>
```

and an independent mutex.

The owner worker removes tasks from the back:

```text
owner → pop_back()
```

while other workers steal from the front:

```text
thief → pop_front()
```

Conceptually:

```text
front                               back
older tasks ---------------- newer tasks
    ↑                                ↑
  thief                            owner
```

Each `WorkerQueue` has its own mutex, so operations on different worker queues do not require one global queue lock.

---

## Scheduling

External submissions are distributed across worker queues using round-robin selection.

Each worker follows approximately this scheduling policy:

```text
try local queue
      |
      | success
      v
   execute
      |
      | failure
      v
scan other queues
      |
      v
try to steal
```

The current implementation uses mutex-protected queues rather than a lock-free work-stealing deque.

The goal of this version is to keep synchronization behavior understandable and auditable before introducing more complex lock-free techniques.

---

## Bounded Queue and Backpressure

The thread pool limits the number of queued tasks with:

```text
max_size
```

A global logical counter:

```text
queued_tasks
```

tracks tasks that are waiting in queues.

It does **not** include tasks that have already been dequeued and are currently executing.

When the queue reaches capacity, producers wait on:

```cpp
not_full
```

using a predicate equivalent to:

```cpp
queued_tasks < max_size || stop
```

Workers wait for available work using:

```cpp
not_empty
```

with:

```cpp
queued_tasks > 0 || stop
```

When a worker successfully removes a task from a queue:

```text
queued_tasks--
```

is performed immediately.

The worker then notifies:

```cpp
not_full.notify_one();
```

before executing the task.

This is intentional: queue capacity becomes available when a task is **dequeued**, not when its execution finishes.

---

## Graceful Shutdown

The destructor performs graceful shutdown rather than abandoning accepted tasks.

The shutdown sequence is:

```text
set stop = true
       |
       v
notify blocked workers
and blocked producers
       |
       v
workers continue processing
already accepted tasks
       |
       v
queued_tasks reaches 0
       |
       v
workers exit
       |
       v
join all worker threads
```

A worker exits only when:

```cpp
stop && queued_tasks == 0
```

Therefore tasks successfully accepted before shutdown are drained before the `ThreadPool` destructor returns.

---

## Synchronization Design

Global scheduler state such as:

```text
stop
queued_tasks
next_queue
```

is protected by:

```cpp
state_mutex
```

Each local task deque is independently protected by its own `WorkerQueue` mutex.

A key design decision is to avoid holding a queue mutex and `state_mutex` simultaneously on the worker path.

This helps avoid opposite nested lock orders such as:

```text
Thread A:
state_mutex
    ↓
queue_mutex

Thread B:
queue_mutex
    ↓
state_mutex
```

which could form an ABBA deadlock.

User tasks are also always executed **outside** the thread pool's internal locks.

Otherwise, a long-running user task could prevent other workers, producers, or shutdown operations from progressing.

---

## Condition Variables

The thread pool uses two condition variables:

```text
not_empty
not_full
```

### `not_empty`

Workers wait until:

```cpp
queued_tasks > 0 || stop
```

### `not_full`

Producers wait until:

```cpp
queued_tasks < max_size || stop
```

The `stop` condition is included in both predicates so that all waiting threads can leave their waits during shutdown.

Normal queue state transitions generally use:

```cpp
notify_one()
```

while shutdown uses:

```cpp
notify_all()
```

because all blocked workers and producers must re-check the shutdown state.

---

## Testing

The stress test covers several important concurrency properties:

- multiple concurrent producers
- bounded queue backpressure
- exact-once task execution
- future return values
- exception propagation
- worker survival after a task exception
- graceful shutdown
- draining all accepted work before destruction

A representative stress configuration is:

```text
workers             = 4
producers           = 4
tasks per producer  = 5000
total tasks         = 20000
maximum queued work = 128
```

Every logical task owns an atomic execution counter.

After the thread pool is destroyed, the test verifies:

```text
missing tasks    = 0
duplicated tasks = 0
```

Repeated stress runs pass on the current implementation.

---

## ThreadSanitizer

The project was also tested with ThreadSanitizer.

A TSan build can be generated with:

```bash
cmake -S . -B build-tsan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"

cmake --build build-tsan
```

On the tested WSL environment, ThreadSanitizer required ASLR to be disabled for the process:

```bash
setarch "$(uname -m)" -R \
  ./build-tsan/thread_pool_stress
```

Under the currently exercised stress-test paths, ThreadSanitizer did not report a data race.

This should be interpreted as validation of the tested execution paths rather than a proof that every possible concurrent execution is race-free.

---

## Benchmark

The benchmark uses a synthetic CPU workload:

```cpp
std::uint64_t x = i + 1;

for (int j = 0; j < loops_per_task; ++j)
{
    x = x * 1664525ULL + 1013904223ULL;
}
```

The total amount of computation is kept approximately constant while task granularity is changed.

The benchmark timer covers:

```text
task submission
+
thread-pool scheduling
+
task execution
+
completion waiting
```

### Corrected Results

During the final code review, a constructor bug was discovered that created each worker thread twice.

After fixing the constructor and rerunning the benchmark, the corrected three-run averages were approximately:

| Workload | 4 workers | 8 workers |
|---|---:|---:|
| 100 tasks × 1,000,000 loops | 0.0545 s | 0.0311 s |
| 1,000 tasks × 100,000 loops | 0.0544 s | 0.0311 s |
| 10,000 tasks × 10,000 loops | 0.0656 s | 0.2525 s |

The checksum remains identical between worker counts for the same workload.

---

## Performance Observation

For coarse-grained tasks:

```text
100 × 1,000,000
1000 × 100,000
```

the 8-worker configuration shows clear parallel speedup over 4 workers.

For the fine-grained workload:

```text
10000 × 10,000
```

the behavior reverses sharply:

```text
4 workers ≈ 0.066 s
8 workers ≈ 0.252 s
```

This demonstrates that adding worker threads does not automatically improve performance.

When individual tasks become sufficiently small, the relative cost of:

- queue synchronization
- queue scanning
- stealing attempts
- condition-variable interaction
- thread scheduling
- producer / consumer synchronization

can become significant relative to useful computation.

The corrected benchmark confirms the fine-grained scaling regression.

Earlier profiling experiments were performed during development, but their exact numeric results are not treated as evidence for the final implementation because they predated the constructor bug fix.

---

## Known Limitation: Logical Queue Count vs Physical Queues

`queued_tasks` is protected by `state_mutex`, while the actual local deques are protected by independent queue mutexes.

A worker removes a task from a physical queue first, and then later updates:

```cpp
queued_tasks--;
```

under `state_mutex`.

This creates a short window such as:

```text
queued_tasks == 1

worker A:
removes the final physical task
        |
        | has not decremented queued_tasks yet
        v

worker B:
observes queued_tasks > 0
        |
        v
scans queues
        |
        v
finds no task
```

Worker B can temporarily repeat its queue scan until the logical counter catches up.

This is currently treated as a performance limitation rather than a correctness failure.

Making physical deque modification and the global logical count perfectly atomic would require tighter coordination between queue locks and `state_mutex`, increasing synchronization complexity and potentially creating new lock-order risks.

For this version, the simpler locking protocol is intentionally retained.

---

## Current Limitations and Possible Improvements

The current implementation intentionally avoids several more advanced techniques.

Possible future improvements include:

- replacing `std::bind` with an `std::invoke` / tuple-based one-shot task binder
- cleaner support for move-only bound arguments
- reducing fine-grained scheduling overhead
- smarter stealing victim selection
- batching external task injection
- adaptive spinning before condition-variable blocking
- lock-free or specialized work-stealing deques
- NUMA-aware scheduling
- per-worker affinity
- more rigorous native-Linux profiling using hardware performance counters

These are intentionally left outside the current scope so that the project remains small enough to reason about and review thoroughly.

---

## Build

Requirements:

- C++17 compatible compiler
- CMake 3.20 or later
- pthread-compatible threading support

Configure and build:

```bash
cmake -S . -B build
cmake --build build
```

---

## Run Demo

```bash
./build/thread_pool_demo
```

---

## Run Stress Test

```bash
./build/thread_pool_stress
```

---

## Run Benchmark

```bash
./build/thread_pool_benchmark
```

---

## Design Takeaways

This project was primarily used to study several systems-programming concepts in practice:

- RAII and object lifetime
- move semantics
- move-only types
- type erasure
- perfect forwarding
- `std::future`
- `std::packaged_task`
- mutexes and lock ownership
- condition variables
- producer / consumer synchronization
- backpressure
- graceful shutdown
- work stealing
- deadlock prevention
- data-race testing
- performance benchmarking
- profiling-driven investigation

A major lesson from the project is that concurrent-system performance cannot be inferred only from thread count.

Correctness, synchronization structure, workload granularity, scheduling behavior, and measurement methodology all interact.

The final implementation therefore prioritizes a clear and auditable concurrency model while keeping known performance limitations documented rather than hidden behind additional scheduler complexity.