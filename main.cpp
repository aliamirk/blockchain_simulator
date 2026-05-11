#include "engine.h"
#include "dashboard.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <pthread.h>
#include <unistd.h>
#include <sstream>
#include <atomic>
#include <signal.h>

// ─── Global engine so signal handler can stop it ──────────────────────────
static Engine*    g_engine    = nullptr;
static Dashboard* g_dashboard = nullptr;
static std::atomic<bool> g_running{true};

static void handleSignal(int) {
    g_running.store(false);
    if (g_dashboard) g_dashboard->stop();
    if (g_engine)    g_engine->stop();
}

// ─── Client producer thread ───────────────────────────────────────────────
struct ClientArgs {
    Engine*           engine;
    int               clientId;
    int               tasksToSubmit;
    int               intervalMs;   // ms between submissions
    std::atomic<bool>* running;
};

static void* clientThread(void* arg) {
    auto* ca = static_cast<ClientArgs*>(arg);
    auto& eng = *ca->engine;

    const char* taskNames[] = {
        "DataSync", "ImageResize", "ReportGen", "CacheWarm",
        "BackupJob", "MetricAgg",  "UserAuth",  "PaymentProc",
        "MLInfer",   "LogFlush",   "DBCompact", "EmailSend"
    };
    const int nameCount = 12;

    for (int i = 0; i < ca->tasksToSubmit && ca->running->load(); ++i) {
        int priority  = (rand() % 9) + 1;       // 1-9
        int timeoutMs = 1000 + rand() % 4000;    // 1-5 seconds
        int duration  = 200  + rand() % 2000;    // 200ms-2.2s

        std::string name = std::string(taskNames[rand() % nameCount])
                         + "-C" + std::to_string(ca->clientId)
                         + "-" + std::to_string(i);

        bool isIO = (rand() % 2 == 0);

        eng.submit(name,
            [duration, isIO]() {
                // cancelFlag not easily passed via lambda here;
                // watchdog uses pthread_cancel instead
                if (isIO) {
                    struct timespec ts = {0, (long)(duration) * 1000000L};
                    nanosleep(&ts, nullptr);
                } else {
                    volatile double x = 1.0;
                    auto end = std::chrono::steady_clock::now() +
                               std::chrono::milliseconds(duration);
                    while (std::chrono::steady_clock::now() < end)
                        x = std::sin(x + 0.001) * 1.0001;
                    (void)x;
                }
            },
            priority, timeoutMs
        );

        struct timespec ts = {0, (long)ca->intervalMs * 1000000L};
        nanosleep(&ts, nullptr);
    }
    delete ca;
    return nullptr;
}

// ─── Main ─────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    srand(42);

    // Parse args
    int numWorkers    = 6;
    int maxConcurrent = 4;
    int numClients    = 3;
    int tasksPerClient= 50;
    int intervalMs    = 200;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--workers") == 0 && i+1 < argc)
            numWorkers = atoi(argv[++i]);
        else if (strcmp(argv[i], "--concurrent") == 0 && i+1 < argc)
            maxConcurrent = atoi(argv[++i]);
        else if (strcmp(argv[i], "--clients") == 0 && i+1 < argc)
            numClients = atoi(argv[++i]);
        else if (strcmp(argv[i], "--tasks") == 0 && i+1 < argc)
            tasksPerClient = atoi(argv[++i]);
        else if (strcmp(argv[i], "--interval") == 0 && i+1 < argc)
            intervalMs = atoi(argv[++i]);
    }

    signal(SIGINT,  handleSignal);
    signal(SIGTERM, handleSignal);

    // Boot engine
    EngineConfig cfg;
    cfg.numWorkers      = numWorkers;
    cfg.maxConcurrent   = maxConcurrent;
    cfg.ageThresholdMs  = 2000;
    cfg.agingBoost      = 1;
    cfg.agingIntervalMs = 500;

    Engine    engine(cfg);
    Dashboard dashboard(engine);

    g_engine    = &engine;
    g_dashboard = &dashboard;

    engine.start();
    dashboard.start();

    // Spawn client threads
    std::vector<pthread_t> clientThreads(numClients);
    for (int c = 0; c < numClients; ++c) {
        auto* args          = new ClientArgs();
        args->engine        = &engine;
        args->clientId      = c + 1;
        args->tasksToSubmit = tasksPerClient;
        args->intervalMs    = intervalMs + (c * 50); // stagger clients
        args->running       = &g_running;
        pthread_create(&clientThreads[c], nullptr, clientThread, args);
    }

    // Wait for clients to finish, then run for a bit more
    for (auto& t : clientThreads) pthread_join(t, nullptr);

    // Keep running until Q pressed or signal
    while (g_running.load()) {
        struct timespec ts = {0, 100000000L};
        nanosleep(&ts, nullptr);
    }

    dashboard.stop();
    engine.stop();
    return 0;
}
