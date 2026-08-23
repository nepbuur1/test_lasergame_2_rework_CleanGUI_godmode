// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>

#include "pcg_DisplayControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_cg
{
	class GameOverControl : public IButtonListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		DisplayControl& displayControl;
		IButton* pGameOverPageStartButton;
		Flag flagStartPressed;

	public:
		GameOverControl(Task* pTask, DisplayControl& displayControl) :
			pTask(pTask),
			displayControl(displayControl),
			pGameOverPageStartButton(nullptr),
			flagStartPressed(pTask)
		{
			pGameOverPageStartButton = displayControl.addGameOverPageStartButtonListener(this);
		}

    void buttonPressed(IButton* pButton)
    {
		logger.logText(pButton->getButtonName());

		if(pButton == pGameOverPageStartButton)
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

		void doGameOver()
		{
			displayControl.showGameOverPage();
			pTask->dumpStackHighWaterMarkIfIncreased(); // This function call takes about 0.25ms! It should be called while debugging only.

			// Een druk van voor deze pagina telt niet mee: de game-over-knop zit
			// op dezelfde plek als de startknop, dus de druk waarmee het spel
			// begon zou de game-over-pagina meteen weer wegklikken.
			flagStartPressed.clear();

			// Only one state, so no need for a switch statement.
			// STATE_WAIT_FOR_START
			pTask->wait(flagStartPressed);

			displayControl.hideGameOverPage();
		}
	}; // end class GameOverControl

};// end namespace pong_cleangui_cg
