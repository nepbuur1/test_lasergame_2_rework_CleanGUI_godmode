// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>

#include "pdga_GameOverDisplayControl.h"
#include "pdga_HallOfFame.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// UC03_GameOverHandling. De volgorde staat hier en niet in de DisplayControl:
	// die weet hoe de pagina eruitziet, deze weet wat er moet gebeuren.
	class GameOverControl : public IButtonListener, public IGameOverDisplayListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		IGameOverDisplayControl& gameOverDisplayControl;
		HallOfFame& hallOfFame;
		IButton* pGameOverPageStartButton;
		Flag flagStartPressed;
		Flag flagNameEntered;

	public:
		GameOverControl(Task* pTask, IGameOverDisplayControl& gameOverDisplayControl, HallOfFame& hallOfFame) :
			pTask(pTask),
			gameOverDisplayControl(gameOverDisplayControl),
			hallOfFame(hallOfFame),
			pGameOverPageStartButton(nullptr),
			flagStartPressed(pTask),
			flagNameEntered(pTask)
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

	/*override*/ void nameEntered()
	{
		flagNameEntered.set();
	}

		// Zie GameControl::doGame(): tonen aan het begin, verbergen aan het eind.
		// Dat MainControl deze functies strikt na elkaar aanroept is wat
		// garandeert dat er nooit twee PageRoots tegelijk in beeld staan.
		void doGameOver(int32_t score)
		{
			gameOverDisplayControl.showGameOverPage();
			pTask->dumpStackHighWaterMarkIfIncreased(); // This function call takes about 0.25ms! It should be called while debugging only.

			// Een druk van voor deze pagina telt niet mee: de knop zit op dezelfde
			// plek als waar tijdens het spel gespeeld werd.
			flagStartPressed.clear();

			// UC03 stap 1: alleen als de score een plek verdient.
			if (hallOfFame.qualifies(score))
			{
				flagNameEntered.clear();
				gameOverDisplayControl.showNameInput();
				pTask->wait(flagNameEntered);

				char strName[MaxNofNameChars + 1];
				gameOverDisplayControl.getEnteredName(strName, sizeof(strName));

				hallOfFame.insert(strName, score);
				hallOfFame.save();		// UC03 stap 1c.

				ESP_LOGI("pong", "top-5: '%s' met %d punten opgeslagen", strName, (int)score);
			}

			// UC03 stap 2 en 3: de lijst tonen en wachten op "Play Again".
			gameOverDisplayControl.showHallOfFame(hallOfFame);

			// Nog een keer wissen: tijdens het intypen kan de knop niet geraakt
			// zijn, maar de aanraking waarmee ENTER werd ingedrukt hoeft niet per
			// se de laatste te zijn geweest.
			flagStartPressed.clear();
			pTask->wait(flagStartPressed);

			gameOverDisplayControl.hideGameOverPage();
		}
	}; // end class GameOverControl

};// end namespace pong_cleangui_dg_adv
