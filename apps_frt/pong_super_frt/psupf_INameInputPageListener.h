// by Marius Versteegen, 2026

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>

using namespace crt;
namespace pong_super_frt
{
	class INameInputPageListener
	{
	public:
		virtual void nameInputPageHidden() = 0;
	};
};// end namespace pong_super_frt
