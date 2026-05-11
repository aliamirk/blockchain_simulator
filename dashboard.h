#pragma once
#include "engine.h"
#include <pthread.h>
#include <atomic>

class Dashboard {
public:
    explicit Dashboard(Engine& engine);
    ~Dashboard();

    void start();
    void stop();

private:
    Engine&           engine_;
    pthread_t         thread_;
    std::atomic<bool> running_{false};

    static void* threadEntry(void* arg);
    void loop();
    void render();

    // Layout helpers
    void drawBox(int y, int x, int h, int w, const char* title);
    void drawBar(int y, int x, int width, double fraction, int colorPair);
};
