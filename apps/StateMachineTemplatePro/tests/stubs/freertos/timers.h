#pragma once
#include "FreeRTOS.h"
struct FakeTimer;
using TimerHandle_t = FakeTimer *;
using TimerCallbackFunction_t = void (*)(TimerHandle_t);
TimerHandle_t xTimerCreate(const char *, TickType_t, BaseType_t, void *, TimerCallbackFunction_t);
BaseType_t xTimerStart(TimerHandle_t, TickType_t);
BaseType_t xTimerStop(TimerHandle_t, TickType_t);
void *pvTimerGetTimerID(TimerHandle_t);
