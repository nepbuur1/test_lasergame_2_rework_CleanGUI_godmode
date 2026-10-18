#include "FakeRtos.hpp"
#include "driver/gpio.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace fake {
static Runtime rt;
Runtime &runtime() { return rt; }
void reset() { rt = Runtime{}; }
void require(bool value, const std::string &message) {
    if (!value) throw std::runtime_error(message);
}
void runTask(const std::string &name) {
    const auto it = std::find_if(rt.tasks.begin(), rt.tasks.end(), [&](const Task &t) { return t.name == name; });
    require(it != rt.tasks.end(), "Task not registered: " + name);
    const auto task = *it;
    try { task.entry(task.arg); }
    catch (const Stop &) { return; }
    throw std::runtime_error("Task unexpectedly returned: " + name);
}
FakeTimer &timer(const std::string &name) {
    for (auto &t : rt.timers) if (t->name == name) return *t;
    throw std::runtime_error("Timer not registered: " + name);
}
void advance(TickType_t elapsed) {
    rt.now += elapsed;
    for (auto &t : rt.timers) {
        if (t->active && rt.now >= t->due) {
            t->active = false;
            t->callback(t.get());
        }
    }
}
}

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t entry, const char *name, uint32_t, void *arg,
                                 UBaseType_t, TaskHandle_t *handle, BaseType_t) {
    fake::runtime().tasks.push_back({name, entry, arg});
    *handle = arg;
    return pdPASS;
}
void vTaskDelay(TickType_t delay) {
    if (fake::runtime().onDelay) fake::runtime().onDelay(delay);
    fake::advance(delay);
}
TickType_t xTaskGetTickCount() { return fake::runtime().now; }

QueueHandle_t xQueueCreate(UBaseType_t count, UBaseType_t size) {
    auto queue = std::make_unique<FakeQueue>();
    queue->capacity = count; queue->itemSize = size;
    auto handle = queue.get();
    fake::runtime().queues.push_back(std::move(queue));
    return handle;
}
static std::vector<unsigned char> bytes(QueueHandle_t queue, const void *value) {
    const auto p = static_cast<const unsigned char *>(value);
    return {p, p + queue->itemSize};
}
BaseType_t xQueueOverwrite(QueueHandle_t queue, const void *value) {
    fake::require(queue->capacity == 1, "Overwrite needs a length-one queue");
    queue->messages.clear(); queue->messages.push_back(bytes(queue, value));
    fake::runtime().overwritten.push_back({fake::runtime().now, queue, queue->messages.back().front()});
    return pdPASS;
}
BaseType_t xQueueSend(QueueHandle_t queue, const void *value, TickType_t) {
    fake::require(queue->messages.size() < queue->capacity, "Unexpected full queue in deterministic test");
    queue->messages.push_back(bytes(queue, value));
    fake::runtime().sent.push_back({fake::runtime().now, queue, queue->messages.back().front()});
    return pdPASS;
}
BaseType_t xQueuePeek(QueueHandle_t queue, void *value, TickType_t timeout) {
    fake::runtime().peekTimeouts.push_back(timeout);
    if (queue->messages.empty()) return pdFAIL;
    std::memcpy(value, queue->messages.front().data(), queue->itemSize);
    return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t queue, void *value, TickType_t timeout) {
    auto &rt = fake::runtime();
    rt.receives.push_back({queue, timeout});
    if (queue->messages.empty() && timeout != 0) {
        // A blocking receive on an empty queue: let the test act as the producer.
        if (rt.onBlockingReceive) rt.onBlockingReceive(queue);
        fake::require(!queue->messages.empty(), "Blocking receive never satisfied in " + rt.state);
    }
    if (queue->messages.empty()) return pdFAIL;
    std::memcpy(value, queue->messages.front().data(), queue->itemSize);
    queue->messages.pop_front();
    return pdPASS;
}
BaseType_t xQueueReset(QueueHandle_t queue) {
    queue->messages.clear();
    if (fake::runtime().afterReset) fake::runtime().afterReset(queue);
    return pdPASS;
}
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue) { return static_cast<UBaseType_t>(queue->messages.size()); }

EventGroupHandle_t xEventGroupCreate() {
    auto group = std::make_unique<FakeEventGroup>();
    auto handle = group.get(); fake::runtime().groups.push_back(std::move(group)); return handle;
}
EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits) {
    fake::runtime().signals.push_back({fake::runtime().now, bits});
    return group->bits |= bits;
}
EventBits_t xEventGroupClearBits(EventGroupHandle_t group, EventBits_t bits) {
    if (fake::runtime().beforeClear) fake::runtime().beforeClear(group, bits);
    const auto before = group->bits; group->bits &= ~bits;
    if (fake::runtime().afterClear) fake::runtime().afterClear(group, bits);
    return before;
}
EventBits_t xEventGroupWaitBits(EventGroupHandle_t group, EventBits_t mask, BaseType_t clear,
                              BaseType_t all, TickType_t timeout) {
    const fake::Wait wait{group, mask, clear == pdTRUE, all == pdTRUE, timeout};
    fake::runtime().waits.push_back(wait);
    for (int guard = 0; guard < 1000; ++guard) {
        if (fake::runtime().onWait) fake::runtime().onWait(wait);
        const auto bits = group->bits;
        if (all ? ((bits & mask) == mask) : ((bits & mask) != 0)) {
            if (clear) group->bits &= ~mask;
            return bits;
        }
        fake::runtime().blocked.push_back(wait);
        if (timeout == 0) return bits;
    }
    throw std::runtime_error("Wait never satisfied in " + fake::runtime().state);
}

TimerHandle_t xTimerCreate(const char *name, TickType_t period, BaseType_t reload, void *id, TimerCallbackFunction_t cb) {
    fake::require(reload == pdFALSE, "Only one-shot timers expected");
    auto timer = std::make_unique<FakeTimer>();
    timer->name = name; timer->period = period; timer->due = 0; timer->id = id; timer->callback = cb;
    auto handle = timer.get(); fake::runtime().timers.push_back(std::move(timer)); return handle;
}
BaseType_t xTimerStart(TimerHandle_t timer, TickType_t) {
    timer->active = true; timer->due = fake::runtime().now + timer->period; ++timer->starts; return pdPASS;
}
BaseType_t xTimerStop(TimerHandle_t timer, TickType_t) { timer->active = false; ++timer->stops; return pdPASS; }
void *pvTimerGetTimerID(TimerHandle_t timer) { return timer->id; }
int gpio_set_level(gpio_num_t pin, uint32_t value) { fake::runtime().gpio[pin] = value; return 0; }
void fakeLog(const char *tag, const char *format, ...) {
    char buffer[512]; va_list args; va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
    auto &rt = fake::runtime();
    const std::string message(buffer);
    if (std::string(tag) == "MachineControl" && message.rfind("State is State", 0) == 0)
        rt.state = message.substr(9);
    rt.logs.push_back(message);
    if (rt.onLog) rt.onLog(message);
}
