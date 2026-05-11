#include "worker.h"
#include "monitor.h"
#include <sstream>
#include <cstring>

WorkerPool::WorkerPool(int numWorkers, int maxConcurrent, TaskQueue& queue)
    : numWorkers_(numWorkers), queue_(queue)
{
    sem_init(&concurrencySem_, 0, maxConcurrent);
    pthread_mutex_init(&runningMutex_, nullptr);
    Monitor::instance().stats().totalWorkers.store(numWorkers);
}

WorkerPool::~WorkerPool() {
    stop();
    sem_destroy(&concurrencySem_);
    pthread_mutex_destroy(&runningMutex_);
}

void WorkerPool::start() {
    running_.store(true);
    threads_.resize(numWorkers_);
    for (int i = 0; i < numWorkers_; ++i) {
        auto* arg = new WorkerArg{this};
        pthread_create(&threads_[i], nullptr, workerEntry, arg);
    }
    std::ostringstream oss;
    oss << "WorkerPool started with " << numWorkers_ << " workers";
    Monitor::instance().log(LogLevel::INFO, oss.str());
}

void WorkerPool::stop() {
    if (!running_.exchange(false)) return;
    queue_.shutdown();
    for (auto& t : threads_) pthread_join(t, nullptr);
    threads_.clear();
    Monitor::instance().log(LogLevel::INFO, "WorkerPool stopped");
}

void* WorkerPool::workerEntry(void* arg) {
    // Allow pthread_cancel to fire at cancellation points (nanosleep, etc.)
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,  nullptr);
    pthread_setcanceltype (PTHREAD_CANCEL_DEFERRED, nullptr);
    auto* warg = static_cast<WorkerArg*>(arg);
    warg->pool->workerLoop();
    delete warg;
    return nullptr;
}

// RAII guard: always releases semaphore + removes task from running map
struct TaskGuard {
    WorkerPool* pool;
    std::shared_ptr<Task> task;
    bool released = false;

    void release() {
        if (released) return;
        released = true;
        pthread_mutex_lock(&pool->runningMutex_);
        pool->runningMap_.erase(task->id);
        pthread_mutex_unlock(&pool->runningMutex_);
        pool->activeCount_.fetch_sub(1);
        Monitor::instance().stats().activeWorkers.store(pool->activeCount_.load());
        sem_post(&pool->concurrencySem_);
    }
    ~TaskGuard() { release(); }
};

void WorkerPool::workerLoop() {
    while (running_.load()) {
        auto task = queue_.pop();
        if (!task) break;  // shutdown signal

        // Skip already-cancelled tasks
        TaskStatus expected = TaskStatus::QUEUED;
        if (!task->status.compare_exchange_strong(expected, TaskStatus::RUNNING))
            continue;

        // Acquire concurrency semaphore
        sem_wait(&concurrencySem_);
        activeCount_.fetch_add(1);
        Monitor::instance().stats().activeWorkers.store(activeCount_.load());

        task->startTime    = std::chrono::steady_clock::now();
        task->workerThread = pthread_self();

        // Register in running map
        pthread_mutex_lock(&runningMutex_);
        runningMap_[task->id] = task;
        pthread_mutex_unlock(&runningMutex_);

        Monitor::instance().logTaskEvent("TASK_STARTED", *task);

        // RAII guard ensures cleanup even if pthread_cancel fires
        TaskGuard guard{this, task};

        bool cancelled = false;
        try {
            task->fn();
        } catch (...) {
            // pthread_cancel delivers __forced_unwind; must rethrow it
            cancelled = true;
            guard.release();   // clean up before rethrowing
            throw;
        }

        // Mark done only if not already timed out
        if (!cancelled) {
            TaskStatus runState = TaskStatus::RUNNING;
            if (task->status.compare_exchange_strong(runState, TaskStatus::DONE)) {
                task->endTime = std::chrono::steady_clock::now();
                Monitor::instance().logTaskEvent("TASK_COMPLETED", *task);
            }
        }
        // guard destructor handles release
    }
}

std::map<int, std::shared_ptr<Task>> WorkerPool::runningTasks() const {
    pthread_mutex_lock(&runningMutex_);
    auto copy = runningMap_;
    pthread_mutex_unlock(&runningMutex_);
    return copy;
}
