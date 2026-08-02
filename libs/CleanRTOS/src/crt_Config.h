// by Marius Versteegen, 2023
//
// This file is included by CleanRTOS.h
// Normally, you don't need to change the settings below.

#pragma once
#define CRT_DEBUG_LOGGING

// Zet het venster in Queue::write expres wijd open (zie daar). Aanzetten om te
// controleren of een taak die met waitAny op een queue wacht het stale-doorbell
// probleem heeft; het maakt alles wel traag. Normaal uit.
//#define CRT_QUEUE_WIDEN_DOORBELL_RACE

// CRT_HIGH_WATERMARK_INCREASE_LOGGING laat elke Task per lus-iteratie
// dumpStackHighWaterMarkIfIncreased() aanroepen. Die kost ~0,25 ms, ook als er
// niets gelogd wordt: uxTaskGetStackHighWaterMark() scant telkens de hele stack
// af op het vulpatroon. In taken die veel iteraties draaien (zoals AsyncDisplay,
// die per iteratie precies een teken-commando verwerkt) tikt dat flink aan.
//
// Daarom: aan tijdens debuggen, uit in een release-build. sdkconfig.h definieert
// precies een van de CONFIG_COMPILER_OPTIMIZATION_*-macro's; PERF is -O2 en SIZE
// is -Os. Buiten ESP-IDF (bijv. Arduino IDE) bestaat sdkconfig.h niet en blijft
// het gedrag zoals het was: aan.
#if defined(__has_include)
  #if __has_include(<sdkconfig.h>)
    #include <sdkconfig.h>
  #endif
#endif

#if defined(CONFIG_COMPILER_OPTIMIZATION_PERF) || defined(CONFIG_COMPILER_OPTIMIZATION_SIZE)
  // release build: watermark-logging uit.
#else
  #define CRT_HIGH_WATERMARK_INCREASE_LOGGING
#endif

// #define ARDUINO_RUNNING_CORE 1  already defined in sdkconfig

namespace crt
{
	const uint32_t MAX_MUTEXNESTING = 20;

	// below, the mutexIDs directly involved in this test can be found.
	const uint32_t MutexID_Logger = (1 << 30);	// High ID, so can be nested very deeply.
};