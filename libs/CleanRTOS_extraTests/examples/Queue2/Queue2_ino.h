// by Marius Versteegen, 2023

#include <crt_CleanRTOS.h>        // This file includes crt_Config.h  You'll need to change defines there for a release build.

// All Tasks should be created in this main file.
// That includes the Logger.

#include <crt_Logger.h>
#include <crt_Handler.h>

#include "crt_TestQueue2.h"
namespace crt
{
    MainInits mainInits;            // Initialize CleanRTOS

	StringDisplayTask stringDisplayTask("StringDisplayTask", 2 /*priority*/, 4000 /*stackBytes*/, ARDUINO_RUNNING_CORE); // Don't forget to call its start() memeber during setup().
	StringSendTask    stringSendTask   ("StringSendTask"   , 2 /*priority*/, 4000 /*stackBytes*/, ARDUINO_RUNNING_CORE, stringDisplayTask);
}

void setup()
{
	// initialize serial communication at 115200 bits per second:
	// Serial.begin(115200); // Only needed when using Serial.print();

	vTaskDelay(10);// allow logger to initialize.
	ESP_LOGI("checkpoint", "start of main");vTaskDelay(1);
	crt::logger.logText("starting");
}

void loop()
{
	vTaskDelay(1);// Nothing to do in loop - all example code runs in the 4 threads above.
}
