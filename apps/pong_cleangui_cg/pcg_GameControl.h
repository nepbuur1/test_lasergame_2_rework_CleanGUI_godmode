// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>

#include "pcg_DisplayControl.h"
#include "pcg_BallControl.h"
#include "pcg_PaddleControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_cg
{
	class GameControl : public IButtonListener, public IBallControlListener
	{
	private:
		IDisplayControl& displayControl;
		BallControl& ballControl;
		PaddleControl& paddleControl;
		State state;
		Flag flagBallMissedByPaddle;

	public:
		GameControl(Task* pTask, IDisplayControl& displayControl, BallControl& ballControl, PaddleControl& paddleControl) :
			displayControl(displayControl),
			ballControl(ballControl),
			paddleControl(paddleControl),
			state(STATE_GAME_RUNNING),
			flagBallMissedByPaddle(pTask),
		{
			displayControl.add
			ballControl.setBallControlListener(this);
		}

		void ballMissedByPaddle()
		{
			flagBallMissedByPaddle.set();
		}

	private:

		/*override keyword not supported*/
		void doGame
		{
			displayControl.selectGamePage();			
			flagBallMissedByPaddle.clear();	// en een gemiste bal van het vorige spel mag dit spel niet meteen beeindigen.

			dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

			// One and only state for now:	
			// STATE_GAME_RUNNING:
			logger.logText("STATE_GAME_RUNNING");
			wait(flagBallMissedByPaddle);
		}
	}; // end class PaddleControl

};// end namespace crt
