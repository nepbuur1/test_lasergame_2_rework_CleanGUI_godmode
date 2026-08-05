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
		IDisplayControl& displayControl;
		IButton* pGameOverPageStartButton;
		Flag flagStartPressed;

	public:
		GameOverControl(Task* pTask, IDisplayControl& displayControl) :	
			pTask(pTask), 
			displayControl(displayControl),
			pStartPageStartButton(nullptr),
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

	private:
		void doGameOver()
		{
			displayControl.selectGameOverPage();
			pTask->dumpStackHighWaterMarkIfIncreased(); // This function call takes about 0.25ms! It should be called while debugging only.

			// Only one state, so no need for a switch statement.
			// STATE_WAIT_FOR_START
			pTask->wait(flagStartPressed);
		}
	}; // end class PaddleControl

};// end namespace crt
