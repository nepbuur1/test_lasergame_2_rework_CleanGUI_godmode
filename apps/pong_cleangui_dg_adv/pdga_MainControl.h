// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>

//#include <crt_TFT_eSPI_DisplayAdapter.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include "pdga_StartPageControl.h"
#include "pdga_GameControl.h"
#include "pdga_GameOverPageControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// Zie pdga_BallControl.h: logger komt uit namespace crt, niet uit deze.

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
		// Elke pagina heeft zijn eigen DisplayControl, dus die komen hier alle drie
		// binnen. Elk control-object krijgt precies die ene mee waar het iets van
		// moet weten - GameControl kent de startpagina niet en andersom.
		MainControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber,
					IStartPageDisplayControl& startPageDisplayControl, IGameDisplayControl& gameDisplayControl,
					IGameOverDisplayControl& gameOverDisplayControl, BallDepot& ballDepot, CrateDepot& crateDepot,
					GameObjectsControl& gameObjectsControl, PaddleControl& paddleControl) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
			state(STATE_START_PAGE),
			gameControl(this, gameDisplayControl, ballDepot, crateDepot, gameObjectsControl, paddleControl),
			gameOverControl(this, gameOverDisplayControl),
			startPageControl(this, startPageDisplayControl)
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

};// end namespace pong_cleangui_dg_adv
