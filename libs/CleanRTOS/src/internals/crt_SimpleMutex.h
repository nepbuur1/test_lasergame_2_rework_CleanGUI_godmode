#pragma once
#include "internals/crt_FreeRTOS.h"

namespace crt
{
	// A SimpleMutex protects a resource from concurrent access by multiple threads.
	// It is a bit simpler to use and faster than the normal Mutex.
	// However, opposed to the normal Mutex, it does not detect deadlock situations.
	// Thus the SimpleMutex should only be used in situations that cannot potentially
	// yield deadlock situations, like inside a Pool.
	
	class SimpleMutex
	{
	public:
		SemaphoreHandle_t freeRtosMutex;
		BaseType_t rc;
		
	public:
		// MutexSections with lower mutexID can wrap MutexSections with higher mutexID.
		// but not the other way around (to prevent deadlocks).
		SimpleMutex() :
			freeRtosMutex(xSemaphoreCreateMutex())
			//mutexID(mutexID), freeRtosMutex(xSemaphoreCreateBinary())
		{
			//xSemaphoreGive(freeRtosMutex);	// In case of a binary semaphore: that one should be given before it can be taken.
		}
		
		// Blokkerend wachten, en niet pollen met een timeout van 0 plus taskYIELD().
		// Dat laatste gaf priority inversion zonder uitweg: taskYIELD() geeft de
		// processor alleen aan taken met dezelfde of een hogere prioriteit. Wacht
		// een hoge-prioriteitstaak zo op een mutex die een lagere taak op dezelfde
		// core vasthoudt, dan komt die lagere taak nooit meer aan de beurt om hem
		// vrij te geven. De hoge taak blijft dan eeuwig draaien, en de idle-taak ook
		// niet: de task watchdog vuurt. Gezien in pong_cleangui_dg_adv
		// (2026-10-20): GameDisplayControl (prio 3) in Pool<int>::read, terwijl
		// PaddleControl (prio 1) midden in de write zat.
		// Met een blokkerende take krijgt de houder via priority inheritance
		// tijdelijk de prioriteit van de wachter, maakt hij zijn korte stukje af en
		// geeft hij de mutex vrij. Wachten voedt de watchdog vanzelf.
		void lock()
		{
			rc = xSemaphoreTake(freeRtosMutex, portMAX_DELAY);
			assert(rc == pdPASS);
		}
		
		void unlock()
		{
			rc = xSemaphoreGive(freeRtosMutex);
			assert(rc == pdPASS);
		}
	};
};
