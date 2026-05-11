#include "watchdog.h"
#include "monitor.h"
#include <sstream>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

Watchdog::Watchdog(WorkerPool& pool, int checkIntervalMs)
    : pool_(pool), checkIntervalMs_(checkIntervalMs) {}

Watchdog::~Watchdog() { stop(); }

void Watchdog::start() {
    running_.store(true);
    pthread_create(&thread_, nullptr, threadEntry, this);
    Monitor::instance().log(LogLevel::INFO, "Watchdog started");
}

void Watchdog::stop() {
    if (!running_.exchange(false)) return;
    pthread_join(thread_, nullptr);
    Monitor::instance().log(LogLevel::INFO, "Watchdog stopped");
}

void* Watchdog::threadEntry(void* arg) {
    static_cast<Watchdog*>(arg)->loop();
    return nullptr;
}

void Watchdog::loop() {
    while (running_.load()) {
        // Sleep in small chunks for responsiveness
        struct timespec ts = {0, (long)checkIntervalMs_ * 1000000L};
        nanosleep(&ts, nullptr);

        auto now     = std::chrono::steady_clock::now();
        auto running = pool_.runningTasks();

        for (auto& [id, task] : running) {
            if (task->status.load() != TaskStatus::RUNNING) continue;

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - task->startTime).count();

            if (elapsed >= task->timeoutMs) {
                // Attempt to mark as TIMEOUT
                TaskStatus expected = TaskStatus::RUNNING;
                if (task->status.compare_exchange_strong(expected, TaskStatus::TIMEOUT)) {
                    task->cancelFlag.store(true);
                    task->endTime = std::chrono::steady_clock::now();

                    std::ostringstream oss;
                    oss << "TASK_TIMEOUT | Task #" << task->id
                        << " \"" << task->name << "\""
                        << " | Elapsed=" << elapsed << "ms"
                        << " | Limit=" << task->timeoutMs << "ms";
                    Monitor::instance().log(LogLevel::WARN, oss.str());
                    Monitor::instance().stats().tasksTimedOut.fetch_add(1);

                    // Signal the thread to cancel via pthread_cancel
                    if (task->workerThread != 0) {
                        pthread_cancel(task->workerThread);
                    }
                }
            }
        }
    }
}
