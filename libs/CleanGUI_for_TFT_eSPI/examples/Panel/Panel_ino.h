// by Marius Versteegen, 2023

#include <crt_CleanRTOS.h>        // This file includes crt_Config.h  You'll need to change defines there for a release build.

// All Tasks should be created in this main file.
#include "crt_TestPanel.h"
namespace crt
{
	MainInits mainInits;            // Initialize CleanRTOS.
}

// 10000 bytes stack, en dat is hier geen ruime marge maar een noodzaak: dit
// voorbeeld maakt zijn display-adapter als *lokale* variabele in main(), dus
// die staat op deze stack. De PAL8-adapter is 2744 bytes waar de RGB565-variant
// er 984 telt - de drie paletkopieen erin zijn samen al 1280 bytes. Met de
// oorspronkelijke 4000 liep de stack over, met een LoadProhibited in een malloc
// diep in TFT_eSPI::init() als gevolg.
crt::TestPanel testPanel("TestPanel", 2 /*priority*/, 10000 /*stackBytes*/, ARDUINO_RUNNING_CORE);

void setup()
{
	ESP_LOGI("checkpoint", "start of main");
}

void loop()
{
	vTaskDelay(1);// Nothing to do in loop - all example code runs in the 4 threads above.
}
