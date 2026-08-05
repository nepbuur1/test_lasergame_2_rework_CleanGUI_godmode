// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

#include "pcg_DisplayControl.h"

using namespace crt;
namespace pong_cleangui_cg
{
	class StartPageControl : public IButtonListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		IDisplayControl& displayControl;
		IButton* pStartPageStartButton;
		Flag flagStartPressed;

	public:
		StartPageControl(Task* pTask, IDisplayControl& displayControl) :
			pTask(pTask),
			displayControl(displayControl),
			pStartPageStartButton(nullptr),
			flagStartPressed(pTask)
		{
			pStartPageStartButton = displayControl.addStartPageStartButtonListener(this);
		}

    void buttonPressed(IButton* pButton)
    {
		logger.logText(pButton->getButtonName());

		if(pButton == pStartPageStartButton)
		{
        	flagStartPressed.set();
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
			pTask->dumpStackHighWaterMarkIfIncreased(); // This function call takes about 0.25ms! It should be called while debugging only.

			// Een druk van voor deze pagina - bijvoorbeeld die op de
			// game-over-knop, die op precies dezelfde plek zit - mag de startknop
			// niet meteen weer "indrukken".
			flagStartPressed.clear();

			// Only one state, so no need for a switch statement.
			// STATE_WAIT_FOR_START
			pTask->wait(flagStartPressed);

			displayControl.hideStartPage();
		}
	}; // end class StartPageControl

};// end namespace pong_cleangui_cg
