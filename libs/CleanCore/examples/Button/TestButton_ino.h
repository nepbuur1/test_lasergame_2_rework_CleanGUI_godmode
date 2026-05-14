// by Marius Versteegen, 2023

#include <crt_CleanRTOS.h>        // This file includes crt_Config.h  You'll need to change defines there for a release build.

// All Tasks should be created in this main file.
#include <crt_Logger.h>
#include <crt_Handler.h>
#include "crt_Button.h"
#include "crt_TestButtons.h"
namespace crt
{
    MainInits mainInits;            // Initialize CleanRTOS.
	Handler<10 /*MAXLISTENERCOUNT*/> buttonHandler("ButtonHandler", 2 /*priority*/, ARDUINO_RUNNING_CORE, 100 /*periodMs*/, 3000 /*batchTimeUs*/); // Don't forget to call its start() member during setup().
	Button<1> buttonA ("A" /*name*/, 33 /*pin*/, true/*positive logic*/, buttonHandler);
	// More buttons can be added here..
	TestButtons testButtons(buttonA);
}

void setup()
{
	// initialize serial communication at 115200 bits per second:
	// Serial.begin(115200);    // Only needed when using Serial.print();

	vTaskDelay(10);// allow tasks to initialize.
	ESP_LOGI("checkpoint", "start of main");vTaskDelay(1);
}

void loop()
{
	vTaskDelay(1);// Nothing to do in loop - all example code runs in the 4 threads above.
}
