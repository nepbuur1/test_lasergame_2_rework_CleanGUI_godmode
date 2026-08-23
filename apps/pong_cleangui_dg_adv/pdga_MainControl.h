// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>

#include "pdga_HallOfFame.h"
#include "pdga_StartPageControl.h"
#include "pdga_GameControl.h"
#include "pdga_GameOverControl.h"

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
		// Elke pagina heeft zijn eigen DisplayControl, dus die komen hier alle vier
		// binnen. Elk control-object krijgt precies die mee waar het iets van moet
		// weten - GameControl kent de startpagina niet en andersom. De naampagina
		// gaat door naar GameOverControl, dat hem meteen doorgeeft aan zijn
		// NameInputControl en er zelf niets van hoeft te weten.
		MainControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber,
					StartPageDisplayControl& startPageDisplayControl, GameDisplayControl& gameDisplayControl,
					NameInputPageDisplayControl& nameInputPageDisplayControl,
					HallOfFameDisplayControl& hallOfFameDisplayControl, PaddleControl& paddleControl,
					HallOfFame& hallOfFame) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
			state(STATE_START_PAGE),
			gameControl(this, gameDisplayControl, paddleControl),
			gameOverControl(this, nameInputPageDisplayControl, hallOfFameDisplayControl, hallOfFame),
			startPageControl(this, startPageDisplayControl)
		{
			// Dat de naampagina meldt wanneer de naam is ingetypt (UC03 stap
			// 1b) regelt NameInputControl zelf: die wacht erop, dus die vraagt het
			// aan. Hier stond daar een regel voor, en dat was een derde plek die van
			// die melding moest weten.

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
						// De score van het zojuist gespeelde spel gaat mee: UC03
						// beslist ermee of er om een naam wordt gevraagd.
						gameOverControl.doGameOver(gameControl.getScore());
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
