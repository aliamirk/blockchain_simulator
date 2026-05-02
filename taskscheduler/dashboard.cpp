#include "dashboard.h"
#include "monitor.h"
#include <ncurses.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <cmath>

// Color pair IDs
#define CP_HEADER    1
#define CP_GOOD      2
#define CP_WARN      3
#define CP_ERROR     4
#define CP_TITLE     5
#define CP_DIM       6
#define CP_HIGHLIGHT 7
#define CP_BORDER    8

Dashboard::Dashboard(Engine& engine) : engine_(engine) {}
Dashboard::~Dashboard() { stop(); }

void Dashboard::start() {
    running_.store(true);
    pthread_create(&thread_, nullptr, threadEntry, this);
}

void Dashboard::stop() {
    if (!running_.exchange(false)) return;
    pthread_join(thread_, nullptr);
}

void* Dashboard::threadEntry(void* arg) {
    static_cast<Dashboard*>(arg)->loop();
    return nullptr;
}

void Dashboard::loop() {
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(CP_HEADER,    COLOR_BLACK,  COLOR_CYAN);
        init_pair(CP_GOOD,      COLOR_GREEN,  -1);
        init_pair(CP_WARN,      COLOR_YELLOW, -1);
        init_pair(CP_ERROR,     COLOR_RED,    -1);
        init_pair(CP_TITLE,     COLOR_CYAN,   -1);
        init_pair(CP_DIM,       COLOR_WHITE,  -1);
        init_pair(CP_HIGHLIGHT, COLOR_BLACK,  COLOR_GREEN);
        init_pair(CP_BORDER,    COLOR_CYAN,   -1);
    }

    while (running_.load()) {
        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            running_.store(false);
            break;
        }

        render();

        struct timespec ts = {0, 250000000L}; // 250ms refresh
        nanosleep(&ts, nullptr);
    }

    endwin();
}

void Dashboard::drawBox(int y, int x, int h, int w, const char* title) {
    attron(COLOR_PAIR(CP_BORDER));

    // Corners
    mvaddch(y,       x,       ACS_ULCORNER);
    mvaddch(y,       x+w-1,   ACS_URCORNER);
    mvaddch(y+h-1,   x,       ACS_LLCORNER);
    mvaddch(y+h-1,   x+w-1,   ACS_LRCORNER);

    // Top/bottom borders
    for (int i = 1; i < w-1; ++i) {
        mvaddch(y,     x+i, ACS_HLINE);
        mvaddch(y+h-1, x+i, ACS_HLINE);
    }
    // Side borders
    for (int i = 1; i < h-1; ++i) {
        mvaddch(y+i, x,     ACS_VLINE);
        mvaddch(y+i, x+w-1, ACS_VLINE);
    }

    attroff(COLOR_PAIR(CP_BORDER));

    // Title
    if (title && strlen(title) > 0) {
        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvprintw(y, x+2, " %s ", title);
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    }
}

void Dashboard::drawBar(int y, int x, int width, double fraction, int colorPair) {
    fraction = std::max(0.0, std::min(1.0, fraction));
    int filled = (int)(fraction * width);
    attron(COLOR_PAIR(colorPair) | A_REVERSE);
    for (int i = 0; i < filled; ++i) mvaddch(y, x+i, ' ');
    attroff(COLOR_PAIR(colorPair) | A_REVERSE);
    attron(COLOR_PAIR(CP_DIM));
    for (int i = filled; i < width; ++i) mvaddch(y, x+i, ACS_CKBOARD);
    attroff(COLOR_PAIR(CP_DIM));
}

void Dashboard::render() {
    clear();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    auto& mon   = Monitor::instance();
    auto& stats = mon.stats();

    // ── Header bar ──────────────────────────────────────────────────────────
    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    for (int i = 0; i < cols; ++i) mvaddch(0, i, ' ');
    mvprintw(0, 2, "  TaskScheduler  |  Kubernetes/ECS Simulator  |  Press Q to quit  ");
    attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

    // ── Stats panel (top-left) ───────────────────────────────────────────────
    int statW = 36, statH = 14;
    drawBox(1, 0, statH, statW, "System Stats");

    int row = 2;
    auto prtStat = [&](const char* label, long long val, int cp) {
        mvprintw(row, 2, "%-20s", label);
        attron(COLOR_PAIR(cp) | A_BOLD);
        mvprintw(row, 22, "%10lld", val);
        attroff(COLOR_PAIR(cp) | A_BOLD);
        ++row;
    };

    prtStat("Submitted:",   stats.tasksSubmitted.load(),  CP_DIM);
    prtStat("Completed:",   stats.tasksCompleted.load(),  CP_GOOD);
    prtStat("Timed Out:",   stats.tasksTimedOut.load(),   CP_WARN);
    prtStat("Cancelled:",   stats.tasksCancelled.load(),  CP_ERROR);

    mvprintw(row, 2, "%-20s", "Queue Size:");
    attron(COLOR_PAIR(CP_WARN) | A_BOLD);
    mvprintw(row, 22, "%10d", stats.currentQueueSize.load());
    attroff(COLOR_PAIR(CP_WARN) | A_BOLD);
    ++row;

    // Workers bar
    int active  = stats.activeWorkers.load();
    int total   = stats.totalWorkers.load();
    if (total == 0) total = 1;
    mvprintw(row, 2, "Workers Active:");
    attron(COLOR_PAIR(CP_GOOD) | A_BOLD);
    mvprintw(row, 18, "%d/%d", active, total);
    attroff(COLOR_PAIR(CP_GOOD) | A_BOLD);
    ++row;
    mvprintw(row, 2, "  ");
    drawBar(row, 2, statW-4, (double)active/total, CP_GOOD);
    ++row;

    // Throughput
    double tps    = mon.tasksPerSecond();
    double avgLat = mon.avgLatencyMs();
    ++row;
    mvprintw(row, 2, "Throughput:");
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvprintw(row, 14, "%8.2f tasks/s", tps);
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    ++row;
    mvprintw(row, 2, "Avg Latency:");
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvprintw(row, 14, "%8.1f ms", avgLat);
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

    // ── Queue panel (top-right of stats) ────────────────────────────────────
    int queueX = statW + 1;
    int queueW = cols - queueX;
    int queueH = statH;
    if (queueW > 10) {
        drawBox(1, queueX, queueH, queueW, "Queue (Top 10 by Priority)");

        auto snapshot = engine_.queue().snapshot();
        // Sort by priority ascending for display
        std::sort(snapshot.begin(), snapshot.end(),
            [](auto& a, auto& b){ return a->priority < b->priority; });

        int displayN = std::min((int)snapshot.size(), queueH - 2);
        // Header
        attron(A_UNDERLINE | COLOR_PAIR(CP_DIM));
        mvprintw(2, queueX+2, "%-4s %-20s %5s %8s",
                 "ID", "Name", "Pri", "Wait(ms)");
        attroff(A_UNDERLINE | COLOR_PAIR(CP_DIM));

        auto now = std::chrono::steady_clock::now();
        for (int i = 0; i < displayN && i < 9; ++i) {
            auto& t = snapshot[i];
            auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - t->submitTime).count();
            int cp = (t->priority <= 2) ? CP_ERROR :
                     (t->priority <= 5) ? CP_WARN  : CP_DIM;
            attron(COLOR_PAIR(cp));
            std::string name = t->name;
            if ((int)name.size() > 19) name = name.substr(0, 19);
            mvprintw(3+i, queueX+2, "%-4d %-20s %5d %8lld",
                     t->id, name.c_str(), t->priority, (long long)waitMs);
            attroff(COLOR_PAIR(cp));
        }
        if ((int)snapshot.size() > 9) {
            mvprintw(12, queueX+2, "  ... and %d more",
                     (int)snapshot.size() - 9);
        }
    }

    // ── Log panel (bottom) ──────────────────────────────────────────────────
    int logY    = statH + 1;
    int logH    = rows - logY;
    int logW    = cols;
    if (logH >= 5) {
        drawBox(logY, 0, logH, logW, "Event Log");

        auto logs = mon.recentLogs(logH - 3);
        // Only show as many as fit
        int maxLines = logH - 2;
        int startIdx = (int)logs.size() > maxLines ?
                       (int)logs.size() - maxLines : 0;

        static auto startTime = std::chrono::steady_clock::now();

        for (int i = startIdx; i < (int)logs.size(); ++i) {
            auto& e = logs[i];
            int   r = logY + 1 + (i - startIdx);
            if (r >= logY + logH - 1) break;

            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                e.ts - startTime).count();

            // Timestamp
            attron(COLOR_PAIR(CP_DIM));
            mvprintw(r, 1, "[%6lld.%03lld]", ms/1000, ms%1000);
            attroff(COLOR_PAIR(CP_DIM));

            // Level tag + message
            int cp = (e.level == LogLevel::WARN)  ? CP_WARN  :
                     (e.level == LogLevel::ERROR)  ? CP_ERROR : CP_DIM;

            // Color-code by event type
            if (e.message.find("COMPLETED") != std::string::npos)      cp = CP_GOOD;
            else if (e.message.find("TIMEOUT") != std::string::npos)   cp = CP_WARN;
            else if (e.message.find("STARTED") != std::string::npos)   cp = CP_HIGHLIGHT;
            else if (e.message.find("SUBMITTED") != std::string::npos) cp = CP_TITLE;

            attron(COLOR_PAIR(cp));
            std::string msg = e.message;
            int maxMsgW = logW - 14;
            if ((int)msg.size() > maxMsgW) msg = msg.substr(0, maxMsgW);
            mvprintw(r, 13, "%s", msg.c_str());
            attroff(COLOR_PAIR(cp));
        }
    }

    refresh();
}
