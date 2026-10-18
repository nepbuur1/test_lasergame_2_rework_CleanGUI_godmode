// by Marius Versteegen, 2026

#include <crt_CleanRTOS.h>        // This file includes crt_Config.h  You'll need to change defines there for a release build.

// All Tasks should be created in this main file.
#include <crt_ADC_buttons_E12.h>
#include "crt_AdcButtonTester.h"
namespace crt
{
	MainInits mainInits;            // Initialize CleanRTOS.

	// Zes knoppen op de weerstandsladder, Uuit op GPIO35 (connector P3 van de CYD), ladder op 3,3 V,
	// en 68k van Uuit naar massa (zie de docs: zonder die weerstand verzadigt de ADC).
	ADC_buttons_E12<1 /*MAXLISTENERCOUNT*/> adcButtons("ADC_buttons_E12", 3 /*priority*/, 4096 /*stackBytes*/, ARDUINO_RUNNING_CORE,
	                                                   35 /*pinAdc*/, 3300 /*vrefMillivolts*/,
	                                                   10 /*pollPeriodMs*/, 3 /*nofStablePolls*/, 3 /*maxSimultaneousButtons*/);
	AdcButtonTester adcButtonTester("AdcButtonTester", 2 /*priority*/, 4096 /*stackBytes*/, ARDUINO_RUNNING_CORE, adcButtons);
}

void setup()
{
	vTaskDelay(10);// allow tasks to initialize.
	ESP_LOGI("checkpoint", "start of main");vTaskDelay(1);
}

void loop()
{
	vTaskDelay(1);// Nothing to do in loop - all example code runs in the tasks of adcButtons and adcButtonTester.
}
