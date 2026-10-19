// by Marius Versteegen, 2026

#include <crt_CleanRTOS.h>        // This file includes crt_Config.h  You'll need to change defines there for a release build.

// All Tasks should be created in this main file.
#include <crt_ADC_buttons.h>
#include "crt_WorstCaseBorderDistanceTester.h"
namespace crt
{
	MainInits mainInits;            // Initialize CleanRTOS.

	// Hooguit 3 knoppen tegelijk herkennen, net als de default van ADC_buttons. De beslisgrenzen
	// liggen dan midden tussen de niveaus van die 41 combinaties (plus "geen knop"), en de test meet
	// de marges zoals een applicatie ze krijgt. Zet hem op 6 om de ladder zelf te meten (alle 64).
	const int maxSimultaneousButtons = 3;

	// De ladder en RL komen uit crt_ADC_buttons_E12_ladder.h.
	ADC_buttons<1 /*MAXLISTENERCOUNT*/> adcButtons("ADC_buttons", 3 /*priority*/, 4096 /*stackBytes*/, ARDUINO_RUNNING_CORE,
	                                                   35 /*pinAdc*/, 3300 /*vrefMillivolts*/,
	                                                   10 /*pollPeriodMs*/, 3 /*nofStablePolls*/, maxSimultaneousButtons);
	WorstCaseBorderDistanceTester<1> worstCaseBorderDistanceTester("BorderDistance", 2 /*priority*/, 6144 /*stackBytes*/, ARDUINO_RUNNING_CORE,
	                                                               adcButtons, 10 /*pollPeriodMs*/);
}

void setup()
{
	vTaskDelay(10);// allow tasks to initialize.
	ESP_LOGI("checkpoint", "start of main");vTaskDelay(1);
}

void loop()
{
	vTaskDelay(1);// Nothing to do in loop - all test code runs in the tasks of adcButtons and worstCaseBorderDistanceTester.
}
