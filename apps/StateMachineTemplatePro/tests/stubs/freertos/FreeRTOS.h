#pragma once
#include <cassert>
#include <cstdint>
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
constexpr BaseType_t pdFALSE = 0;
constexpr BaseType_t pdTRUE = 1;
constexpr BaseType_t pdFAIL = 0;
constexpr BaseType_t pdPASS = 1;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
constexpr TickType_t portTICK_PERIOD_MS = 1;
#define pdMS_TO_TICKS(ms) static_cast<TickType_t>(ms)
#define configASSERT(value) assert(value)
