#pragma once
#include "task.h"
#include "queue.h"
#include <vector>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <atomic>
#include <map>

class WorkerPool {
public:
    WorkerPool(int numWorkers, int maxConcurrent, TaskQueue& queue);
    ~WorkerPool();

    void start();
    void stop();

    int activeCount() const { return activeCount_.load(); }
    int workerCount() const { return numWorkers_; }

    // For watchdog: snapshot of currently executing tasks
    std::map<int, std::shared_ptr<Task>> runningTasks() const;

    // Exposed for TaskGuard (defined in worker.cpp)
    mutable pthread_mutex_t              runningMutex_;
    std::map<int, std::shared_ptr<Task>> runningMap_;
    std::atomic<int>                     activeCount_{0};
    sem_t                                concurrencySem_;

private:
    int        numWorkers_;
    TaskQueue& queue_;

    std::vector<pthread_t> threads_;
    std::atomic<bool>      running_{false};

    static void* workerEntry(void* arg);
    void workerLoop();

    struct WorkerArg { WorkerPool* pool; };
};
