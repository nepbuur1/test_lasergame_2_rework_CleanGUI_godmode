#pragma once
#include "FreeRTOS.h"
using EventBits_t = uint32_t;
struct FakeEventGroup;
using EventGroupHandle_t = FakeEventGroup *;
EventGroupHandle_t xEventGroupCreate();
EventBits_t xEventGroupSetBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupClearBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t, EventBits_t, BaseType_t, BaseType_t, TickType_t);
