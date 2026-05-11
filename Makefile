CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pthread

# ── Platform detection ────────────────────────────────────────────────────
UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
  # macOS (Homebrew ncurses preferred; falls back to system)
  NCURSES_PREFIX := $(shell brew --prefix ncurses 2>/dev/null)
  ifneq ($(NCURSES_PREFIX),)
    INCLUDES := -I$(NCURSES_PREFIX)/include
    LDFLAGS  := -L$(NCURSES_PREFIX)/lib -lncurses -lpthread
  else
    INCLUDES :=
    LDFLAGS  := -lncurses -lpthread
  endif
else
  # Linux (Ubuntu 22.04 / similar)
  INCLUDES :=
  LDFLAGS  := -lncurses -ltinfo -lpthread
endif

TARGET  := taskscheduler
SRCS    := main.cpp engine.cpp worker.cpp queue.cpp watchdog.cpp monitor.cpp dashboard.cpp
OBJS    := $(SRCS:.cpp=.o)
HEADERS := task.h queue.h worker.h watchdog.h monitor.h engine.h dashboard.h

# ── Build rules ───────────────────────────────────────────────────────────
.PHONY: all clean run debug

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

debug: CXXFLAGS += -g -DDEBUG -O0
debug: all

clean:
	rm -f $(OBJS) $(TARGET)

# Run with defaults: 6 workers, 4 concurrent, 3 clients, 50 tasks/client
run: all
	./$(TARGET) --workers 6 --concurrent 4 --clients 3 --tasks 50 --interval 200

# Stress run
stress: all
	./$(TARGET) --workers 8 --concurrent 6 --clients 5 --tasks 100 --interval 100
