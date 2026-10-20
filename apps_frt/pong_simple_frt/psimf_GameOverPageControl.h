// by Marius Versteegen, 2023

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>

#include "psimf_DisplayControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_simple_frt
{
	class GameOverControl : public IButtonListener
	{

		// --- FreeRTOS (was CleanRTOS) ---
		static constexpr EventBits_t eventBitStartPressed = (1 << 0);
		EventGroupHandle_t eventGroup;
		EventBits_t firedBits;		// wat de laatste wachtoproep op meer tegelijk opleverde; zie hasFired()

	private:
		DisplayControl& displayControl;
		IButton* pGameOverPageStartButton;

	public:
		GameOverControl(DisplayControl& displayControl) :
			eventGroup(xEventGroupCreate()),
			firedBits(0),
			displayControl(displayControl),
			pGameOverPageStartButton(nullptr)
		{
			pGameOverPageStartButton = displayControl.addGameOverPageStartButtonListener(this);
		}

    void buttonPressed(IButton* pButton)
    {
		ESP_LOGI("pong_simple_frt", "%s", pButton->getButtonName());

		if(pButton == pGameOverPageStartButton)
		{
        	xEventGroupSetBits(eventGroup, eventBitStartPressed);
		}
		else
		{
			;// not interested.
		}
    }

	// must override pure virtual function
	void buttonReleased(IButton* pButton)
	{
		// do nothing
	};

		void doGameOver()
		{
			displayControl.showGameOverPage();

			// Een druk van voor deze pagina telt niet mee: de game-over-knop zit
			// op dezelfde plek als de startknop, dus de druk waarmee het spel
			// begon zou de game-over-pagina meteen weer wegklikken.
			xEventGroupClearBits(eventGroup, eventBitStartPressed);

			// Only one state, so no need for a switch statement.
			// STATE_WAIT_FOR_START
			firedBits = xEventGroupWaitBits(eventGroup, eventBitStartPressed, pdTRUE, pdFALSE, portMAX_DELAY);

			displayControl.hideGameOverPage();
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


	}; // end class GameOverControl

};// end namespace pong_simple_frt
