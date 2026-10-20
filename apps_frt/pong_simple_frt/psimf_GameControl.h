// by Marius Versteegen, 2023

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>

#include "psimf_DisplayControl.h"
#include "psimf_BallControl.h"
#include "psimf_PaddleControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_simple_frt
{
	class GameControl : public IBallControlListener
	{

		// --- FreeRTOS (was CleanRTOS) ---
		static constexpr EventBits_t eventBitBallMissedByPaddle = (1 << 0);
		EventGroupHandle_t eventGroup;
		EventBits_t firedBits;		// wat de laatste wachtoproep op meer tegelijk opleverde; zie hasFired()

	private:
		DisplayControl& displayControl;
		BallControl& ballControl;
		PaddleControl& paddleControl;

	public:
		GameControl(DisplayControl& displayControl, BallControl& ballControl, PaddleControl& paddleControl) :
			eventGroup(xEventGroupCreate()),
			firedBits(0),
			displayControl(displayControl),
			ballControl(ballControl),
			paddleControl(paddleControl)
		{
			ballControl.setBallControlListener(this);
		}

		void ballMissedByPaddle()
		{
			xEventGroupSetBits(eventGroup, eventBitBallMissedByPaddle);
		}

		/*override keyword not supported*/
		void doGame()
		{
			displayControl.showGamePage();
			xEventGroupClearBits(eventGroup, eventBitBallMissedByPaddle);	// en een gemiste bal van het vorige spel mag dit spel niet meteen beeindigen.


			// De peddel eerst: die moet er staan voordat de bal eraan komt.
			paddleControl.enable();
			ballControl.startBall();

			// One and only state for now:
			// STATE_GAME_RUNNING:
			ESP_LOGI("pong_simple_frt", "%s", "STATE_GAME_RUNNING");
			firedBits = xEventGroupWaitBits(eventGroup, eventBitBallMissedByPaddle, pdTRUE, pdFALSE, portMAX_DELAY);

			// De peddelknoppen horen bij het spel, niet bij de game-over-pagina.
			paddleControl.disable();
			displayControl.hideGamePage();
		}
	private:
		// --- FreeRTOS-hulpfuncties ---
		// Zoals CleanRTOS' hasFired(): kijkt of het bit bij de laatste wachtoproep
		// gevuurd had, en wist alleen dat bit.
		bool hasFired(EventBits_t eventBit)
		{
			bool bFired = ((firedBits & eventBit) != 0);
			if (bFired)
			{
				xEventGroupClearBits(eventGroup, eventBit);
			}
			return bFired;
		}


	}; // end class GameControl

};// end namespace pong_simple_frt
