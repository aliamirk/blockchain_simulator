#pragma once
#include "worker.h"
#include <pthread.h>
#include <atomic>

class Watchdog {
public:
    Watchdog(WorkerPool& pool, int checkIntervalMs = 200);
    ~Watchdog();

    void start();
    void stop();

private:
    WorkerPool&        pool_;
    int                checkIntervalMs_;
    pthread_t          thread_;
    std::atomic<bool>  running_{false};

    static void* threadEntry(void* arg);
    void loop();
};
