// by Marius Versteegen, 2026

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>

#include "psimf_DisplayControl.h"

using namespace crt;
namespace pong_simple_frt
{
	class StartPageControl : public IButtonListener
	{

		// --- FreeRTOS (was CleanRTOS) ---
		static constexpr EventBits_t eventBitStartPressed = (1 << 0);
		EventGroupHandle_t eventGroup;
		EventBits_t firedBits;		// wat de laatste wachtoproep op meer tegelijk opleverde; zie hasFired()

	private:
		DisplayControl& displayControl;
		IButton* pStartPageStartButton;

	public:
		StartPageControl(DisplayControl& displayControl) :
			eventGroup(xEventGroupCreate()),
			firedBits(0),
			displayControl(displayControl),
			pStartPageStartButton(nullptr)
		{
			pStartPageStartButton = displayControl.addStartPageStartButtonListener(this);
		}

    void buttonPressed(IButton* pButton)
    {
		ESP_LOGI("pong_simple_frt", "%s", pButton->getButtonName());

		if(pButton == pStartPageStartButton)
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

		/*override keyword not supported*/
		void doStartPage()
		{
			displayControl.showStartPage();

			// Een druk van voor deze pagina - bijvoorbeeld die op de
			// game-over-knop, die op precies dezelfde plek zit - mag de startknop
			// niet meteen weer "indrukken".
			xEventGroupClearBits(eventGroup, eventBitStartPressed);

			// Only one state, so no need for a switch statement.
			// STATE_WAIT_FOR_START
			firedBits = xEventGroupWaitBits(eventGroup, eventBitStartPressed, pdTRUE, pdFALSE, portMAX_DELAY);

			displayControl.hideStartPage();
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


	}; // end class StartPageControl

};// end namespace pong_simple_frt
