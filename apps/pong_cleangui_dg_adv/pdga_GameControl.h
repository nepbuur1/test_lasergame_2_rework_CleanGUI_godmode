// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>

#include <stdlib.h>
#include <esp_timer.h>

#include "pdga_GameDisplayControl.h"
#include "pdga_BallControl.h"
#include "pdga_BallsControl.h"
#include "pdga_PaddleControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class GameControl : public IBallControlListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		IGameDisplayControl& gameDisplayControl;
		BallDepot& ballDepot;
		PaddleControl& paddleControl;
		Flag flagBallLeftTheScreen;

	public:
		GameControl(Task* pTask, IGameDisplayControl& gameDisplayControl, BallDepot& ballDepot, PaddleControl& paddleControl) :
			pTask(pTask),
			gameDisplayControl(gameDisplayControl),
			ballDepot(ballDepot),
			paddleControl(paddleControl),
			flagBallLeftTheScreen(pTask)
		{
			// Eenmalig zaaien, hier en niet in elke bal: srand() zet de reeks
			// terug, dus vijftig keer achter elkaar zaaien met vrijwel dezelfde
			// klokstand levert juist minder toeval op.
			srand((unsigned int)esp_timer_get_time());

			// Alle ballen bedraden, ook de negenenveertig die voorlopig niets doen.
			// Ze bestaan al - het depot bouwt ze in een keer - en een bal die pas
			// bij het spawnen zijn verwijzingen zou krijgen, zou die bedrading
			// telkens opnieuw moeten doen.
			for (int i = 0; i < MaxNofBalls; i++)
			{
				ballDepot.getItem(i).init(gameDisplayControl, paddleControl, this, &ballDepot, i);
			}
		}

		void ballLeftTheScreen()
		{
			flagBallLeftTheScreen.set();
		}

		// Elke do..()-functie vraagt zijn eigen pagina op en geeft hem aan het eind
		// weer vrij. Dat MainControl ze strikt na elkaar aanroept is wat garandeert
		// dat er nooit twee PageRoots tegelijk in beeld staan - de eis die boven
		// crt_PageRoot.h staat.
		/*override keyword not supported*/
		void doGame()
		{
			gameDisplayControl.showGamePage();
			flagBallLeftTheScreen.clear();	// en een gemiste bal van het vorige spel mag dit spel niet meteen beeindigen.

			pTask->dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

			// De peddel eerst: die moet er staan voordat de bal eraan komt.
			paddleControl.enable();

			// Een bal uit het depot halen en in het spel zetten. Voorlopig een;
			// UC07 zal er tien tegelijk willen. Een leeg depot is geen fout maar
			// een uitkomst - het depot meldt het zelf - en dan blijft het spel
			// hangen tot er weer een vrijkomt, wat met een enkele bal niet kan
			// gebeuren.
			int indexBall = ballDepot.obtain();
			if (indexBall != BallDepot::InvalidIndex)
			{
				ballDepot.getItem(indexBall).spawn();
			}

			// One and only state for now:
			// STATE_GAME_RUNNING:
			logger.logText("STATE_GAME_RUNNING");
			pTask->wait(flagBallLeftTheScreen);

			// De peddelknoppen horen bij het spel, niet bij de game-over-pagina.
			paddleControl.disable();
			gameDisplayControl.hideGamePage();
		}
	}; // end class GameControl

};// end namespace pong_cleangui_dg_adv
