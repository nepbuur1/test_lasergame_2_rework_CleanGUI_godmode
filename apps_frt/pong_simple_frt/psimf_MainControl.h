// by Marius Versteegen, 2023

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <crt_CleanGUI.h>

//#include <crt_TFT_eSPI_DisplayAdapter.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include "psimf_DisplayControl.h"
#include "psimf_StartPageControl.h"
#include "psimf_GameControl.h"
#include "psimf_GameOverPageControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_simple_frt
{
	// Zie psimf_BallControl.h: logger komt uit namespace crt, niet uit deze.

	class MainControl
	{

		// --- FreeRTOS (was CleanRTOS) ---
		// MainControl wacht zelf nergens op: dat doen de control-objecten die
		// hij aanroept, elk met een eigen eventGroup. Hij heeft alleen een taak.
		TaskHandle_t taskHandle;

		enum State
		{
			STATE_START_PAGE,
			STATE_GAME_RUNNING,
			STATE_GAME_OVER
		};

	private:
	    State state;
		// Control objects that have been merged to the current Task
		// based on control cohesion.
		GameControl gameControl;
		GameOverControl gameOverControl;
		StartPageControl startPageControl;

	public:
		MainControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, DisplayControl& displayControl, BallControl& ballControl, PaddleControl& paddleControl) :
			taskHandle(nullptr),
			state(STATE_START_PAGE),
			gameControl(displayControl, ballControl, paddleControl),
			gameOverControl(displayControl),
			startPageControl(displayControl)
		{
			xTaskCreatePinnedToCore(staticMain, taskName, taskSizeBytes, this, taskPriority, &taskHandle, taskCoreNumber);
		}

	private:


		// --- FreeRTOS-hulpfuncties ---
		static void staticMain(void* pvParameters)
		{
			static_cast<MainControl*>(pvParameters)->main();
		}



		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

			while (true)
			{
				vTaskDelay(1);

				switch(state)
				{
					case STATE_START_PAGE:
						startPageControl.doStartPage();
						state = STATE_GAME_RUNNING;
						break;

					case STATE_GAME_RUNNING:
						gameControl.doGame();
						state = STATE_GAME_OVER;
						break;

					case STATE_GAME_OVER:
						gameOverControl.doGameOver();
						state = STATE_GAME_RUNNING;
						break;

					default:
						assert(false);
				}

				vTaskDelay(1);
			}
		}
	}; // end class MainControl

};// end namespace pong_simple_frt
