#include "queue.h"
#include <stdexcept>
#include <algorithm>

TaskQueue::TaskQueue() : stopped_(false) {
    pthread_mutex_init(&mutex_, nullptr);
    pthread_cond_init(&notEmpty_, nullptr);
}

TaskQueue::~TaskQueue() {
    pthread_mutex_destroy(&mutex_);
    pthread_cond_destroy(&notEmpty_);
}

bool TaskQueue::heapLess(int a, int b) const {
    // lower priority number = higher urgency = should be at top of heap
    return heap_[a]->priority < heap_[b]->priority;
}

void TaskQueue::heapifyUp(int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (heapLess(idx, parent)) {
            std::swap(heap_[idx], heap_[parent]);
            idx = parent;
        } else break;
    }
}

void TaskQueue::heapifyDown(int idx) {
    int n = (int)heap_.size();
    while (true) {
        int smallest = idx;
        int left  = 2 * idx + 1;
        int right = 2 * idx + 2;
        if (left  < n && heapLess(left,  smallest)) smallest = left;
        if (right < n && heapLess(right, smallest)) smallest = right;
        if (smallest == idx) break;
        std::swap(heap_[idx], heap_[smallest]);
        idx = smallest;
    }
}

void TaskQueue::push(std::shared_ptr<Task> task) {
    pthread_mutex_lock(&mutex_);
    heap_.push_back(std::move(task));
    heapifyUp((int)heap_.size() - 1);
    pthread_cond_signal(&notEmpty_);
    pthread_mutex_unlock(&mutex_);
}

std::shared_ptr<Task> TaskQueue::pop() {
    pthread_mutex_lock(&mutex_);
    while (heap_.empty() && !stopped_) {
        pthread_cond_wait(&notEmpty_, &mutex_);
    }
    if (heap_.empty()) {
        pthread_mutex_unlock(&mutex_);
        return nullptr;
    }
    auto task = heap_.front();
    std::swap(heap_.front(), heap_.back());
    heap_.pop_back();
    if (!heap_.empty()) heapifyDown(0);
    pthread_mutex_unlock(&mutex_);
    return task;
}

std::shared_ptr<Task> TaskQueue::tryPop() {
    pthread_mutex_lock(&mutex_);
    if (heap_.empty()) {
        pthread_mutex_unlock(&mutex_);
        return nullptr;
    }
    auto task = heap_.front();
    std::swap(heap_.front(), heap_.back());
    heap_.pop_back();
    if (!heap_.empty()) heapifyDown(0);
    pthread_mutex_unlock(&mutex_);
    return task;
}

void TaskQueue::shutdown() {
    pthread_mutex_lock(&mutex_);
    stopped_ = true;
    pthread_cond_broadcast(&notEmpty_);
    pthread_mutex_unlock(&mutex_);
}

int TaskQueue::size() const {
    pthread_mutex_lock(&mutex_);
    int s = (int)heap_.size();
    pthread_mutex_unlock(&mutex_);
    return s;
}

bool TaskQueue::empty() const {
    pthread_mutex_lock(&mutex_);
    bool e = heap_.empty();
    pthread_mutex_unlock(&mutex_);
    return e;
}

void TaskQueue::ageTasks(int ageThresholdMs, int boostAmount) {
    auto now = std::chrono::steady_clock::now();
    pthread_mutex_lock(&mutex_);
    bool changed = false;
    for (auto& t : heap_) {
        if (t->status.load() != TaskStatus::QUEUED) continue;
        auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - t->submitTime).count();
        if (waitMs > ageThresholdMs && t->priority > 1) {
            t->priority = std::max(1, t->priority - boostAmount);
            changed = true;
        }
    }
    if (changed) {
        // Rebuild heap
        int n = (int)heap_.size();
        for (int i = n / 2 - 1; i >= 0; --i) heapifyDown(i);
    }
    pthread_mutex_unlock(&mutex_);
}

std::vector<std::shared_ptr<Task>> TaskQueue::snapshot() const {
    pthread_mutex_lock(&mutex_);
    auto copy = heap_;
    pthread_mutex_unlock(&mutex_);
    return copy;
}
