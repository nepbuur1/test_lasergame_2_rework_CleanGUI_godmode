// by Marius Versteegen, 2026

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <frt_IADC_buttons.h>

#include "psupf_ButtonInfoDisplayControl.h"

using namespace crt;
namespace pong_super_frt
{
	// Toont eenmalig, vóór de eerste startpagina, of het externe toetsenbord (de
	// weerstandsladder met zes knoppen op GPIO35, zie libs/CYD_tools_crt) is aangesloten,
	// en wacht tot "Ga verder" is ingedrukt. Samengevoegd met de taak van MainControl,
	// net als StartPageControl.
	class ButtonInfoControl : public IButtonListener
	{

		// --- FreeRTOS (was CleanRTOS) ---
		static constexpr EventBits_t eventBitContinuePressed = (1 << 0);
		EventGroupHandle_t eventGroup;
		EventBits_t firedBits;		// wat de laatste wachtoproep op meer tegelijk opleverde; zie hasFired()

	private:
		ButtonInfoDisplayControl& buttonInfoDisplayControl;
		frt::IADC_buttons& adcButtons;
		IButton* pContinueButton;

	public:
		ButtonInfoControl(ButtonInfoDisplayControl& buttonInfoDisplayControl, frt::IADC_buttons& adcButtons) :
			eventGroup(xEventGroupCreate()),
			firedBits(0),
			buttonInfoDisplayControl(buttonInfoDisplayControl),
			adcButtons(adcButtons),
			pContinueButton(nullptr)
		{
			pContinueButton = buttonInfoDisplayControl.addButtonInfoContinueButtonListener(this);
		}

    void buttonPressed(IButton* pButton)
    {
		ESP_LOGI("pong_super_frt", "%s", pButton->getButtonName());

		if(pButton == pContinueButton)
		{
        	xEventGroupSetBits(eventGroup, eventBitContinuePressed);
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

		// Zie GameControl::doGame(): tonen aan het begin, verbergen aan het eind.
		// isConnected() is pas geldig als ADC_buttons zijn controle bij het opstarten
		// gedaan heeft (±100 ms na de start). MainControl wacht daarom 200 ms voordat
		// het deze functie aanroept.
		void doButtonInfoPage()
		{
			bool bConnected = adcButtons.isConnected();
			buttonInfoDisplayControl.showButtonInfoPage(bConnected);

			xEventGroupClearBits(eventGroup, eventBitContinuePressed);	// een druk van vóór deze pagina telt niet.

			// Only one state, so no need for a switch statement.
			// STATE_WAIT_FOR_CONTINUE
			firedBits = xEventGroupWaitBits(eventGroup, eventBitContinuePressed, pdTRUE, pdFALSE, portMAX_DELAY);

			buttonInfoDisplayControl.hideButtonInfoPage();
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


	}; // end class ButtonInfoControl

};// end namespace pong_super_frt
