#pragma once
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct FakeQueue { unsigned capacity, itemSize; std::deque<std::vector<unsigned char>> messages; };
struct FakeEventGroup { EventBits_t bits = 0; };
struct FakeTimer {
    std::string name;
    TickType_t period, due;
    void *id;
    TimerCallbackFunction_t callback;
    bool active = false;
    unsigned starts = 0, stops = 0;
};

namespace fake {
struct Stop {};
struct Task { std::string name; TaskFunction_t entry; void *arg; };
struct Wait {
    EventGroupHandle_t group;
    EventBits_t mask;
    bool clear, all;
    TickType_t timeout;
};
struct Signal { TickType_t time; EventBits_t bits; };
struct SentItem { TickType_t time; QueueHandle_t queue; unsigned char value; };
struct Receive { QueueHandle_t queue; TickType_t timeout; };
struct Runtime {
    TickType_t now = 0;
    std::vector<Task> tasks;
    std::vector<std::unique_ptr<FakeQueue>> queues;
    std::vector<std::unique_ptr<FakeEventGroup>> groups;
    std::vector<std::unique_ptr<FakeTimer>> timers;
    std::vector<std::string> logs;
    std::vector<Signal> signals;
    std::vector<SentItem> sent, overwritten;
    std::vector<Wait> waits, blocked;
    std::vector<Receive> receives;
    std::vector<TickType_t> peekTimeouts;
    std::map<int, unsigned> gpio;
    std::string state;
    std::function<void(const Wait &)> onWait;
    std::function<void(TickType_t)> onDelay;
    std::function<void(const std::string &)> onLog;
    std::function<void(QueueHandle_t)> afterReset;
    std::function<void(QueueHandle_t)> onBlockingReceive;
    std::function<void(EventGroupHandle_t, EventBits_t)> beforeClear;
    std::function<void(EventGroupHandle_t, EventBits_t)> afterClear;
};
Runtime &runtime();
void reset();
void runTask(const std::string &name);
void advance(TickType_t elapsed);
FakeTimer &timer(const std::string &name);
void require(bool condition, const std::string &message);
}
