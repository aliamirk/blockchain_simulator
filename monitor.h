#pragma once
#include "task.h"
#include <string>
#include <vector>
#include <deque>
#include <pthread.h>
#include <atomic>

enum class LogLevel { INFO, WARN, ERROR };

struct LogEntry {
    std::chrono::steady_clock::time_point ts;
    LogLevel level;
    std::string message;
};

struct Stats {
    std::atomic<long long> tasksSubmitted{0};
    std::atomic<long long> tasksCompleted{0};
    std::atomic<long long> tasksTimedOut{0};
    std::atomic<long long> tasksCancelled{0};
    std::atomic<long long> totalLatencyMs{0};  // sum of all completed task latencies
    std::atomic<int>       currentQueueSize{0};
    std::atomic<int>       activeWorkers{0};
    std::atomic<int>       totalWorkers{0};
};

class Monitor {
public:
    static Monitor& instance();

    void log(LogLevel level, const std::string& msg);
    void logTaskEvent(const std::string& event, const Task& task);

    Stats& stats() { return stats_; }

    // Returns last N log entries
    std::vector<LogEntry> recentLogs(int n = 50) const;

    // Throughput: tasks/sec over last window
    double tasksPerSecond() const;
    double avgLatencyMs() const;

private:
    Monitor();
    ~Monitor();
    Monitor(const Monitor&) = delete;

    mutable pthread_mutex_t logMutex_;
    std::deque<LogEntry>    logs_;
    Stats                   stats_;

    // Ring buffer for throughput calculation
    struct ThroughputSample {
        std::chrono::steady_clock::time_point time;
        long long completedCount;
    };
    mutable pthread_mutex_t tpMutex_;
    std::deque<ThroughputSample> tpSamples_;
    void recordCompletion();

    static Monitor* instance_;
    static pthread_once_t once_;
    static void init();
};
