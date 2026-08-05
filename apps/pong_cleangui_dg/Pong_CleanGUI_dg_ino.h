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
#include "pdg_StartPageDisplayControl.h"
#include "pdg_GameDisplayControl.h"
#include "pdg_GameOverDisplayControl.h"
#include "pdg_PaddleControl.h"
#include "pdg_BallControl.h"
#include "pdg_MainControl.h"

using namespace crt;
namespace pong_cleangui_dg
{
    MainInits mainInits;            // Initialize CleanRTOS.

	// Keuzes voor buffergroottes - ruim gekozen voor deze applicatie.
	const int MaxNofTouchListeners = 20;	// Max aantal touch listeners dat het display kan hebben.
	const int CommandBufferSize = 100;		// Max aantal commando's dat het display kan bufferen.
	const int MaxNofClientTasks = 10;		// Max aantal taken dat commando's naar het display kan sturen.

	TFT_eSPI_FreeFonts<20/*MaxNofFreeFonts*/> freeFonts;		// not used when LOAD_GFXFF is not defined.

	// Typedefs, puur voor de leesbaarheid hieronder.
	typedef TFT_eSPI_DisplayAdapter_RGB565<MaxNofTouchListeners> DisplayAdapterType;
	typedef AsyncDisplay<CommandBufferSize, MaxNofClientTasks> AsyncDisplayType;

	DisplayAdapterType tft_eSPI_DisplayAdapter(freeFonts, "/TouchCalData1"/*filenameCalibration*/, false/*bRepeatCalibration*/);
	AsyncDisplayType asyncDisplay("AsyncDisplay", 2 /*priority*/, 20000 + CommandBufferSize*sizeof(DisplayCommand)+
				            MaxNofClientTasks*sizeof(PrintContext) /*stackBytes*/, ARDUINO_RUNNING_CORE,
				            tft_eSPI_DisplayAdapter, 1/*rotation*/, 2/*fontTouchCalibration*/);

	// Elke pagina zijn eigen DisplayControl, elk met een eigen PageRoot en een
	// eigen taak. Ze mogen naast elkaar bestaan omdat er steeds maar een van de
	// drie in beeld staat - MainControl roept de pagina's strikt na elkaar aan.
	// Zie de opmerking boven crt_PageRoot.h.
	//
	// Let op de volgorde: elk object hieronder krijgt de objecten erboven mee.
	// gameDisplayControl kent de schermmaat al in zijn constructor, dus de twee
	// controls daaronder mogen daar meteen getWidth()/getHeight() van vragen.
	StartPageDisplayControl startPageDisplayControl("StartPageDisplayControl", 1 /*priority*/, 10000 /*stack size*/, ARDUINO_RUNNING_CORE, asyncDisplay, freeFonts);
	GameDisplayControl<20,10> gameDisplayControl("GameDisplayControl", 1 /*priority*/, 20000 /*stack size*/, ARDUINO_RUNNING_CORE, 1000 /*frameDelayMs*/, asyncDisplay, freeFonts);
	GameOverDisplayControl gameOverDisplayControl("GameOverDisplayControl", 1 /*priority*/, 10000 /*stack size*/, ARDUINO_RUNNING_CORE, asyncDisplay, freeFonts);

	PaddleControl paddleControl("PaddleControl", 1 /*priority*/, 5000 /*stack size*/, ARDUINO_RUNNING_CORE, gameDisplayControl);
	BallControl ballControl("BallControl", 1 /*priority*/, 5000 /*stack size*/, ARDUINO_RUNNING_CORE, gameDisplayControl, paddleControl);
	MainControl mainControl("MainControl", 1 /*priority*/, 5000 /*stack size*/, ARDUINO_RUNNING_CORE,
	                        startPageDisplayControl, gameDisplayControl, gameOverDisplayControl, ballControl, paddleControl);
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
