#pragma once
#include "task.h"
#include <vector>
#include <memory>
#include <pthread.h>

class TaskQueue {
public:
    TaskQueue();
    ~TaskQueue();

    // Push a task; signals waiting workers
    void push(std::shared_ptr<Task> task);

    // Pop highest-priority task; blocks if empty (unless shutdown)
    std::shared_ptr<Task> pop();

    // Non-blocking try pop
    std::shared_ptr<Task> tryPop();

    // Shutdown: wake all waiting workers
    void shutdown();

    int size() const;
    bool empty() const;

    // Age tasks: boost priority of tasks waiting too long
    void ageTasks(int ageThresholdMs, int boostAmount);

    // Get snapshot of queue for monitoring (does not remove tasks)
    std::vector<std::shared_ptr<Task>> snapshot() const;

private:
    mutable pthread_mutex_t mutex_;
    pthread_cond_t  notEmpty_;
    bool            stopped_;

    // Min-heap storage (priority 1 = highest = smallest number)
    std::vector<std::shared_ptr<Task>> heap_;

    void heapifyUp(int idx);
    void heapifyDown(int idx);
    bool heapLess(int a, int b) const; // a should come before b?
};
