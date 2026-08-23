// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>

//#include <crt_TFT_eSPI_DisplayAdapter.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include "pcg_DisplayControl.h"
#include "pcg_StartPageControl.h"
#include "pcg_GameControl.h"
#include "pcg_GameOverPageControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_cg
{
	// Zie pcg_BallControl.h: logger komt uit namespace crt, niet uit deze.

	class MainControl : public Task
	{
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
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
			state(STATE_START_PAGE),
			gameControl(this, displayControl, ballControl, paddleControl),
			gameOverControl(this, displayControl),
			startPageControl(this, displayControl)
		{
			start();
		}

	private:

		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

			while (true)
			{
				vTaskDelay(1);
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

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

};// end namespace pong_cleangui_cg
