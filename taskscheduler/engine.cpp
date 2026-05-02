#include "engine.h"
#include <sstream>
#include <unistd.h>

Engine::Engine(EngineConfig cfg)
    : cfg_(cfg), queue_()
{
    workers_ = new WorkerPool(cfg_.numWorkers, cfg_.maxConcurrent, queue_);
    watchdog_ = new Watchdog(*workers_, 100);
}

Engine::~Engine() {
    stop();
    delete watchdog_;
    delete workers_;
}

void Engine::start() {
    running_.store(true);
    workers_->start();
    watchdog_->start();

    // Start aging thread
    pthread_create(&agingThread_, nullptr, agingEntry, this);

    Monitor::instance().log(LogLevel::INFO,
        "Engine started | Workers=" + std::to_string(cfg_.numWorkers) +
        " MaxConcurrent=" + std::to_string(cfg_.maxConcurrent));
}

void Engine::stop() {
    if (!running_.exchange(false)) return;
    pthread_join(agingThread_, nullptr);
    watchdog_->stop();
    workers_->stop();
    Monitor::instance().log(LogLevel::INFO, "Engine stopped");
}

int Engine::submit(const std::string& name, std::function<void()> fn,
                   int priority, int timeoutMs)
{
    int id = nextId_.fetch_add(1);
    auto task = std::make_shared<Task>(id, priority, name, std::move(fn), timeoutMs);
    Monitor::instance().stats().tasksSubmitted.fetch_add(1);
    Monitor::instance().stats().currentQueueSize.store(queue_.size() + 1);

    std::ostringstream oss;
    oss << "TASK_SUBMITTED | Task #" << id << " \"" << name
        << "\" | Pri=" << priority << " | Timeout=" << timeoutMs << "ms";
    Monitor::instance().log(LogLevel::INFO, oss.str());

    queue_.push(task);
    Monitor::instance().stats().currentQueueSize.store(queue_.size());
    return id;
}

void* Engine::agingEntry(void* arg) {
    static_cast<Engine*>(arg)->agingLoop();
    return nullptr;
}

void Engine::agingLoop() {
    while (running_.load()) {
        struct timespec ts = {0, (long)cfg_.agingIntervalMs * 1000000L};
        nanosleep(&ts, nullptr);
        if (!running_.load()) break;
        queue_.ageTasks(cfg_.ageThresholdMs, cfg_.agingBoost);
        Monitor::instance().stats().currentQueueSize.store(queue_.size());
    }
}
