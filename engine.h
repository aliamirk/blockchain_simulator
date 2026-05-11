#pragma once
#include "queue.h"
#include "worker.h"
#include "watchdog.h"
#include "monitor.h"
#include <atomic>
#include <pthread.h>

struct EngineConfig {
    int numWorkers      = 4;
    int maxConcurrent   = 4;    // semaphore cap
    int ageThresholdMs  = 3000; // ms before boosting priority
    int agingBoost      = 1;    // priority reduction per aging cycle
    int agingIntervalMs = 1000; // how often to run aging
};

class Engine {
public:
    explicit Engine(EngineConfig cfg = {});
    ~Engine();

    void start();
    void stop();

    // Submit a task; returns assigned task id
    int submit(const std::string& name, std::function<void()> fn,
               int priority = 5, int timeoutMs = 5000);

    TaskQueue&  queue()   { return queue_;   }
    WorkerPool& workers() { return *workers_; }
    Monitor&    monitor() { return Monitor::instance(); }

    int totalSubmitted() const { return nextId_.load() - 1; }

private:
    EngineConfig      cfg_;
    TaskQueue         queue_;
    WorkerPool*       workers_{nullptr};
    Watchdog*         watchdog_{nullptr};

    std::atomic<int>  nextId_{1};
    std::atomic<bool> running_{false};

    // Aging thread
    pthread_t         agingThread_;
    static void*      agingEntry(void* arg);
    void              agingLoop();
};
