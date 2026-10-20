// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>

#include "psim_DisplayControl.h"
#include "psim_BallControl.h"
#include "psim_PaddleControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_simple
{
	class GameControl : public IBallControlListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		DisplayControl& displayControl;
		BallControl& ballControl;
		PaddleControl& paddleControl;
		Flag flagBallMissedByPaddle;

	public:
		GameControl(Task* pTask, DisplayControl& displayControl, BallControl& ballControl, PaddleControl& paddleControl) :
			pTask(pTask),
			displayControl(displayControl),
			ballControl(ballControl),
			paddleControl(paddleControl),
			flagBallMissedByPaddle(pTask)
		{
			ballControl.setBallControlListener(this);
		}

		void ballMissedByPaddle()
		{
			flagBallMissedByPaddle.set();
		}

		/*override keyword not supported*/
		void doGame()
		{
			displayControl.showGamePage();
			flagBallMissedByPaddle.clear();	// en een gemiste bal van het vorige spel mag dit spel niet meteen beeindigen.

			pTask->dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

			// De peddel eerst: die moet er staan voordat de bal eraan komt.
			paddleControl.enable();
			ballControl.startBall();

			// One and only state for now:
			// STATE_GAME_RUNNING:
			logger.logText("STATE_GAME_RUNNING");
			pTask->wait(flagBallMissedByPaddle);

			// De peddelknoppen horen bij het spel, niet bij de game-over-pagina.
			paddleControl.disable();
			displayControl.hideGamePage();
		}
	}; // end class GameControl

};// end namespace pong_simple
