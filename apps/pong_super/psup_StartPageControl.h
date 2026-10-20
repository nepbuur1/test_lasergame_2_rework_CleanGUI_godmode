// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_Logger.h>

#include "psup_StartPageDisplayControl.h"

using namespace crt;
namespace pong_super
{
	class StartPageControl : public IButtonListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		StartPageDisplayControl& startPageDisplayControl;
		IButton* pStartPageStartButton;
		Flag flagStartPressed;

	public:
		StartPageControl(Task* pTask, StartPageDisplayControl& startPageDisplayControl) :
			pTask(pTask),
			startPageDisplayControl(startPageDisplayControl),
			pStartPageStartButton(nullptr),
			flagStartPressed(pTask)
		{
			pStartPageStartButton = startPageDisplayControl.addStartPageStartButtonListener(this);
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

		// Zie GameControl::doGame(): tonen aan het begin, verbergen aan het eind.
		// Dat MainControl deze functies strikt na elkaar aanroept is wat
		// garandeert dat er nooit twee PageRoots tegelijk in beeld staan.
		/*override keyword not supported*/
		void doStartPage()
		{
			startPageDisplayControl.showStartPage();
			pTask->dumpStackHighWaterMarkIfIncreased(); // This function call takes about 0.25ms! It should be called while debugging only.

			// Een druk van voor deze pagina - bijvoorbeeld die op de
			// game-over-knop, die op precies dezelfde plek zit - mag de startknop
			// niet meteen weer "indrukken".
			flagStartPressed.clear();

			// Only one state, so no need for a switch statement.
			// STATE_WAIT_FOR_START
			pTask->wait(flagStartPressed);

			startPageDisplayControl.hideStartPage();
		}
	}; // end class StartPageControl

};// end namespace pong_super
