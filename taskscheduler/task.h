#pragma once
#include <functional>
#include <string>
#include <chrono>
#include <atomic>

enum class TaskStatus {
    QUEUED,
    RUNNING,
    DONE,
    TIMEOUT,
    CANCELLED
};

inline const char* statusToString(TaskStatus s) {
    switch (s) {
        case TaskStatus::QUEUED:    return "QUEUED";
        case TaskStatus::RUNNING:   return "RUNNING";
        case TaskStatus::DONE:      return "DONE";
        case TaskStatus::TIMEOUT:   return "TIMEOUT";
        case TaskStatus::CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

struct Task {
    int id;
    int priority;           // 1 = highest priority
    int originalPriority;
    std::string name;
    std::function<void()> fn;
    int timeoutMs;          // max execution time in milliseconds

    std::atomic<TaskStatus> status{TaskStatus::QUEUED};
    std::chrono::steady_clock::time_point submitTime;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;

    pthread_t workerThread{0};  // thread executing this task (for cancellation)
    std::atomic<bool> cancelFlag{false};

    Task(int id, int priority, std::string name,
         std::function<void()> fn, int timeoutMs = 5000)
        : id(id), priority(priority), originalPriority(priority),
          name(std::move(name)), fn(std::move(fn)), timeoutMs(timeoutMs),
          submitTime(std::chrono::steady_clock::now()) {}

    // Comparator for min-heap (lower priority number = higher urgency)
    bool operator>(const Task& other) const {
        return priority > other.priority;
    }
};
