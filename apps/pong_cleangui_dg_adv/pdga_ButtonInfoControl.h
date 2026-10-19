// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_Logger.h>
#include <crt_IADC_buttons.h>

#include "pdga_ButtonInfoPageDisplayControl.h"

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// Toont eenmalig, vóór de eerste startpagina, of het externe toetsenbord (de
	// weerstandsladder met zes knoppen op GPIO35, zie libs/CYD_tools_crt) is aangesloten,
	// en wacht tot "Ga verder" is ingedrukt. Samengevoegd met de taak van MainControl,
	// net als StartPageControl.
	class ButtonInfoControl : public IButtonListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		ButtonInfoPageDisplayControl& buttonInfoPageDisplayControl;
		IADC_buttons& adcButtons;
		IButton* pContinueButton;
		Flag flagContinuePressed;

	public:
		ButtonInfoControl(Task* pTask, ButtonInfoPageDisplayControl& buttonInfoPageDisplayControl, IADC_buttons& adcButtons) :
			pTask(pTask),
			buttonInfoPageDisplayControl(buttonInfoPageDisplayControl),
			adcButtons(adcButtons),
			pContinueButton(nullptr),
			flagContinuePressed(pTask)
		{
			pContinueButton = buttonInfoPageDisplayControl.addButtonInfoContinueButtonListener(this);
		}

    void buttonPressed(IButton* pButton)
    {
		logger.logText(pButton->getButtonName());

		if(pButton == pContinueButton)
		{
        	flagContinuePressed.set();
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
			buttonInfoPageDisplayControl.showButtonInfoPage(bConnected);
			pTask->dumpStackHighWaterMarkIfIncreased(); // This function call takes about 0.25ms! It should be called while debugging only.

			flagContinuePressed.clear();	// een druk van vóór deze pagina telt niet.

			// Only one state, so no need for a switch statement.
			// STATE_WAIT_FOR_CONTINUE
			pTask->wait(flagContinuePressed);

			buttonInfoPageDisplayControl.hideButtonInfoPage();
		}
	}; // end class ButtonInfoControl

};// end namespace pong_cleangui_dg_adv
