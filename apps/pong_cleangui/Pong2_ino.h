// by Marius Versteegen, 2023

#include <crt_CleanRTOS.h>        // This file includes crt_Config.h  You'll need to change defines there for a release build.
#include <crt_Mutex.h>            // crt_Mutex.h must be included separately.

#include <crt_CleanGUI.h>
#include <crt_AsyncDisplay.h>
#include <crt_TouchscreenButton.h>
#include <crt_TFT_eSPI_DisplayAdapter.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

// All Tasks should be created in this main file.
#include <crt_Logger.h>
#include <crt_Handler.h>
#include <crt_Button.h>
#include "crt_PaddleControl.h"
#include "crt_SceneDisplayControl.h"
#include "crt_GameControl.h"

namespace crt
{
    MainInits mainInits;            // Initialize CleanRTOS.

	// Keuzes voor buffergroottes - ruim gekozen voor deze applicatie.
	const int MaxNofTouchListeners = 20;	// Max aantal touch listeners dat het display kan hebben.
	const int CommandBufferSize = 100;		// Max aantal commando's dat het display kan bufferen.
	const int MaxNofClientTasks = 10;		// Max aantal taken dat commando's naar het display kan sturen.	

	TFT_eSPI_FreeFonts<20/*MaxNofFreeFonts*/> freeFonts;		// not used when LOAD_GFXFF is not defined.
	TFT_eSPI_DisplayAdapter<MaxNofTouchListeners> tft_eSPI_DisplayAdapter(freeFonts, "/TouchCalData1"/*filenameCalibration*/, false/*bRepeatCalibration*/);
	AsyncDisplay<CommandBufferSize, MaxNofClientTasks> asyncDisplay("AsyncDisplay", 2 /*priority*/, 20000 + CommandBufferSize*sizeof(DisplayCommand)+
				            MaxNofClientTasks*sizeof(PrintContext) /*stackBytes*/, ARDUINO_RUNNING_CORE, 
				            tft_eSPI_DisplayAdapter, 1/*rotation*/, 2/*fontTouchCalibration*/);

	SceneDisplayControl<20,10> sceneDisplayControl("SceneDisplayControl", 1 /*priority*/, 20000 /*stack size*/, ARDUINO_RUNNING_CORE, 1000 /*frameDelayMs*/, asyncDisplay, freeFonts); 

	BallControl ballControl("BallControl", 1 /*priority*/, 5000 /*stack size*/, ARDUINO_RUNNING_CORE, sceneDisplayControl);
	PaddleControl paddleControl("PaddleControl", 1 /*priority*/, 5000 /*stack size*/, ARDUINO_RUNNING_CORE, asyncDisplay, freeFonts, sceneDisplayControl, ballControl); 	
	GameControl gameControl("GameControl", 1 /*priority*/, 5000 /*stack size*/, ARDUINO_RUNNING_CORE, asyncDisplay, freeFonts, sceneDisplayControl, ballControl, paddleControl);
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