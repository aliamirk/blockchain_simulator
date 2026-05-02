#include "monitor.h"
#include <cstdio>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>

Monitor* Monitor::instance_ = nullptr;
pthread_once_t Monitor::once_ = PTHREAD_ONCE_INIT;

void Monitor::init() {
    instance_ = new Monitor();
}

Monitor& Monitor::instance() {
    pthread_once(&once_, init);
    return *instance_;
}

Monitor::Monitor() {
    pthread_mutex_init(&logMutex_, nullptr);
    pthread_mutex_init(&tpMutex_, nullptr);
}

Monitor::~Monitor() {
    pthread_mutex_destroy(&logMutex_);
    pthread_mutex_destroy(&tpMutex_);
}


void Monitor::log(LogLevel level, const std::string& msg) {
    LogEntry e;
    e.ts      = std::chrono::steady_clock::now();
    e.level   = level;
    e.message = msg;

    pthread_mutex_lock(&logMutex_);
    logs_.push_back(e);
    if (logs_.size() > 2000) logs_.pop_front();
    pthread_mutex_unlock(&logMutex_);
}

void Monitor::logTaskEvent(const std::string& event, const Task& task) {
    std::ostringstream oss;
    oss << event << " | Task #" << task.id
        << " \"" << task.name << "\""
        << " | Pri=" << task.priority
        << " | Status=" << statusToString(task.status.load());

    if (task.status == TaskStatus::RUNNING || task.status == TaskStatus::DONE ||
        task.status == TaskStatus::TIMEOUT) {
        auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            task.startTime - task.submitTime).count();
        oss << " | WaitMs=" << waitMs;
    }
    if (task.status == TaskStatus::DONE) {
        auto runMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            task.endTime - task.startTime).count();
        oss << " | RunMs=" << runMs;
        stats_.tasksCompleted.fetch_add(1);
        auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            task.endTime - task.submitTime).count();
        stats_.totalLatencyMs.fetch_add(totalMs);
        recordCompletion();
    }
    if (task.status == TaskStatus::TIMEOUT) {
        stats_.tasksTimedOut.fetch_add(1);
        recordCompletion();
    }

    log(LogLevel::INFO, oss.str());
}

void Monitor::recordCompletion() {
    auto now = std::chrono::steady_clock::now();
    pthread_mutex_lock(&tpMutex_);
    tpSamples_.push_back({now, stats_.tasksCompleted.load() + stats_.tasksTimedOut.load()});
    // Keep only last 10 seconds of samples
    while (!tpSamples_.empty()) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - tpSamples_.front().time).count();
        if (age > 10000) tpSamples_.pop_front();
        else break;
    }
    pthread_mutex_unlock(&tpMutex_);
}

double Monitor::tasksPerSecond() const {
    pthread_mutex_lock(&tpMutex_);
    if (tpSamples_.size() < 2) {
        pthread_mutex_unlock(&tpMutex_);
        return 0.0;
    }
    auto& first = tpSamples_.front();
    auto& last  = tpSamples_.back();
    double ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        last.time - first.time).count();
    double count = (double)(last.completedCount - first.completedCount);
    pthread_mutex_unlock(&tpMutex_);
    if (ms < 1.0) return 0.0;
    return count / (ms / 1000.0);
}

double Monitor::avgLatencyMs() const {
    long long completed = stats_.tasksCompleted.load();
    if (completed == 0) return 0.0;
    return (double)stats_.totalLatencyMs.load() / (double)completed;
}

std::vector<LogEntry> Monitor::recentLogs(int n) const {
    pthread_mutex_lock(&logMutex_);
    std::vector<LogEntry> result;
    int start = (int)logs_.size() > n ? (int)logs_.size() - n : 0;
    for (int i = start; i < (int)logs_.size(); ++i)
        result.push_back(logs_[i]);
    pthread_mutex_unlock(&logMutex_);
    return result;
}
