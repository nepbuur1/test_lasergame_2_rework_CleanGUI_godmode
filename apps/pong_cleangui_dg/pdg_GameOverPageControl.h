// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>

#include "pdg_GameOverDisplayControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_dg
{
	class GameOverControl : public IButtonListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		GameOverDisplayControl& gameOverDisplayControl;
		IButton* pGameOverPageStartButton;
		Flag flagStartPressed;

	public:
		GameOverControl(Task* pTask, GameOverDisplayControl& gameOverDisplayControl) :
			pTask(pTask),
			gameOverDisplayControl(gameOverDisplayControl),
			pGameOverPageStartButton(nullptr),
			flagStartPressed(pTask)
		{
			pGameOverPageStartButton = gameOverDisplayControl.addGameOverPageStartButtonListener(this);
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

		// Zie GameControl::doGame(): tonen aan het begin, verbergen aan het eind.
		// Dat MainControl deze functies strikt na elkaar aanroept is wat
		// garandeert dat er nooit twee PageRoots tegelijk in beeld staan.
		void doGameOver()
		{
			gameOverDisplayControl.showGameOverPage();
			pTask->dumpStackHighWaterMarkIfIncreased(); // This function call takes about 0.25ms! It should be called while debugging only.

			// Een druk van voor deze pagina telt niet mee: de game-over-knop zit
			// op dezelfde plek als de startknop, dus de druk waarmee het spel
			// begon zou de game-over-pagina meteen weer wegklikken.
			flagStartPressed.clear();

			// Only one state, so no need for a switch statement.
			// STATE_WAIT_FOR_START
			pTask->wait(flagStartPressed);

			gameOverDisplayControl.hideGameOverPage();
		}
	}; // end class GameOverControl

};// end namespace pong_cleangui_dg
