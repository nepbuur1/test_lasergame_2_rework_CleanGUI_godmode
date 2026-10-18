// by Marius Versteegen, 2026

// Doet hetzelfde als main/main.cpp van dit project, maar in de vorm van setup()/loop(),
// zodat de main.cpp in de projectroot hem kan kiezen.

#include "app/MachineControl.hpp"
#include "app/testHelpers/InputSimulator.hpp"
#include "app/Counter.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Keuzes voor CYD: de RGB-led.
constexpr uint8_t kPinLedR = 4;
constexpr uint8_t kPinLedG = 16;
constexpr uint8_t kPinLedB = 17;

void setup()
{
	// static: de objecten moeten blijven bestaan nadat setup() terugkeert.
	// (In main/main.cpp zijn het lokale variabelen van app_main, maar die keert nooit terug.)
	static Counter counter;

	vTaskDelay(pdMS_TO_TICKS(100));

	// Create state machine task (internally creates EventGroup and initializes GPIO)
	static MachineControl machineControl(kPinLedR, kPinLedG, kPinLedB, counter);

	// Create the input simulation task and link it to the state machine
	static InputSimulator inputSimulator("InputSimulator", machineControl);
}

void loop()
{
	vTaskDelay(pdMS_TO_TICKS(1000));	// Nothing to do in loop - all code runs in the tasks above.
}
