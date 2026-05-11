# TaskScheduler — Multithreaded Task Scheduling System

A production-quality C++ simulation of a container orchestrator (Kubernetes/ECS-style)
built entirely on raw POSIX primitives: `pthreads`, `pthread_mutex_t`,
`pthread_cond_t`, and `sem_t`.

---

## Architecture

```
Clients (N pthreads)
     │  submit(name, fn, priority, timeoutMs)
     ▼
┌─────────────────────────────────────────┐
│           TaskQueue (min-heap)           │
│  pthread_mutex_t + pthread_cond_t        │
│  • Producers push, workers block on pop  │
│  • Aging thread boosts stale priorities  │
└──────────────────┬──────────────────────┘
                   │  pop()
        ┌──────────┴──────────┐
        │    WorkerPool        │
        │  N pthread workers   │
        │  sem_t concurrencySem│  ← caps parallel execution
        └──────────┬──────────┘
                   │  execute fn()
        ┌──────────┴──────────┐
        │      Watchdog        │
        │  checks timeouts     │
        │  pthread_cancel()    │
        └─────────────────────┘
        
Monitor (singleton) — lock-free stats + ring-buffer logs
Dashboard (pthread) — ncurses TUI, refreshes every 250 ms
```

---

## Components

| File | Responsibility |
|------|---------------|
| `task.h` | `Task` struct: id, priority, fn, timeout, atomic status |
| `queue.h/cpp` | Thread-safe min-heap priority queue |
| `worker.h/cpp` | Fixed-size worker pool with semaphore concurrency cap |
| `watchdog.h/cpp` | Background thread that cancels timed-out tasks |
| `monitor.h/cpp` | Singleton logger + stats (throughput, latency) |
| `engine.h/cpp` | Orchestrator: wires all components, exposes `submit()` |
| `dashboard.h/cpp` | ncurses TUI: stats panel, live queue, colour log stream |
| `main.cpp` | Entry point: client threads, arg parsing, signal handling |

---

## Concurrency Design

### Priority Queue
- **`pthread_mutex_t`** serialises all heap mutations.
- **`pthread_cond_t notEmpty_`** — workers call `pthread_cond_wait` instead of
  spinning; producers call `pthread_cond_signal` after each push.
- Heap invariant is maintained by `heapifyUp` / `heapifyDown`.

### Concurrency Semaphore
A **`sem_t concurrencySem_`** initialised to `maxConcurrent` ensures at most
that many tasks execute simultaneously even with more worker threads available.
This mirrors Kubernetes resource limits.

### Scheduling / Aging
A dedicated aging thread runs every `agingIntervalMs` ms.  
Any task that has been **QUEUED** longer than `ageThresholdMs` ms has its
priority decreased by `agingBoost` (lower number = higher urgency), preventing
starvation of low-priority tasks.

### Timeout / Watchdog
The watchdog thread wakes every 100 ms and inspects all **RUNNING** tasks.  
On expiry it:
1. CAS the status from `RUNNING` → `TIMEOUT`.
2. Calls `pthread_cancel(task->workerThread)`.

Workers use a RAII `TaskGuard` that releases the semaphore and cleans up the
running-tasks map even when `pthread_cancel` unwinds the stack.

---

## Build

### Ubuntu 22.04 / 24.04
```bash
sudo apt install build-essential libncurses-dev
make
```

### macOS (Monterey +)
```bash
brew install ncurses
make
```

### Targets
```bash
make            # optimised release build
make debug      # -g -O0 with DEBUG macro
make clean      # remove objects + binary
make run        # build + run with default params
make stress     # build + run with heavy load
```

---

## Usage

```
./taskscheduler [OPTIONS]

  --workers    N    Number of worker threads       (default: 6)
  --concurrent N    Max tasks running at once       (default: 4)
  --clients    N    Number of client threads        (default: 3)
  --tasks      N    Tasks submitted per client      (default: 50)
  --interval   N    Ms between client submissions   (default: 200)
```

**Examples**
```bash
# Default run
./taskscheduler

# Stress test: 8 workers, 5 clients, 100 tasks each, fast submission
./taskscheduler --workers 8 --concurrent 6 --clients 5 --tasks 100 --interval 100

# Timeout-heavy: short timeouts, slow workers
./taskscheduler --workers 4 --concurrent 2 --tasks 30 --interval 50
```

Press **Q** in the dashboard to quit cleanly.

---

## Dashboard

```
┌─ System Stats ──────────────────┐ ┌─ Queue (Top 10 by Priority) ─────────────┐
│ Submitted:            150        │ │ ID   Name                  Pri  Wait(ms) │
│ Completed:            112        │ │  3   DataSync-C1-2           1      840  │
│ Timed Out:             12        │ │  7   PaymentProc-C2-0        2      612  │
│ Cancelled:              0        │ │ 14   MLInfer-C3-4            3      290  │
│ Queue Size:            26        │ │ ...                                       │
│ Workers Active:       4/6        │ └──────────────────────────────────────────┘
│ [████████░░░░]                   │
│ Throughput:     8.31 tasks/s     │
│ Avg Latency:   643.2 ms          │
└──────────────────────────────────┘
┌─ Event Log ──────────────────────────────────────────────────────────────────┐
│ [     0.041] TASK_SUBMITTED | Task #1 "DataSync-C1-0" | Pri=3 | Timeout=1200ms│
│ [     0.083] TASK_STARTED   | Task #1 "DataSync-C1-0" | Pri=3 | WaitMs=42     │
│ [     0.541] TASK_COMPLETED | Task #1 "DataSync-C1-0" | Pri=3 | RunMs=458     │
│ [     1.203] TASK_TIMEOUT   | Task #5 "MLInfer-C2-1"  | Elapsed=1200ms        │
└──────────────────────────────────────────────────────────────────────────────┘
```

Colour key:
- **Cyan** — task submitted
- **Green/Highlight** — task started / completed
- **Yellow** — timeout / warn
- **Red** — error

---

## Key Design Decisions

1. **No busy-waiting anywhere.** Workers sleep on a condition variable; the
   watchdog uses `nanosleep`.
2. **Lock granularity.** The queue mutex is held only for heap operations
   (microseconds). The running-tasks mutex is separate so workers and the
   watchdog don't contend with the queue.
3. **`pthread_cancel` safety.** Worker execution is wrapped in a try/catch that
   explicitly re-throws `__forced_unwind` (the GCC ABI type used by
   `pthread_cancel`) so the C++ runtime stack-unwinds properly.
4. **Atomic status CAS.** All status transitions (`QUEUED→RUNNING`,
   `RUNNING→DONE`, `RUNNING→TIMEOUT`) use `compare_exchange_strong` to
   guarantee exactly-once semantics with no locks.
