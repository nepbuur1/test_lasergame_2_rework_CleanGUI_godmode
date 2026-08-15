// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_Logger.h>

#include "pdga_GameOverDisplayControl.h"
#include "pdga_HallOfFame.h"
#include "pdga_NameInputControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// UC03_GameOverHandling. De volgorde staat hier en niet in de DisplayControl:
	// die weet hoe de pagina eruitziet, deze weet wat er moet gebeuren.
	class GameOverControl : public IButtonListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		IGameOverDisplayControl& gameOverDisplayControl;
		HallOfFame& hallOfFame;

		// De naaminvoer van stap 1 als een enkele aanroep. Ook samengevoegd met de
		// taak van pTask, dus doQueryName() draait in dezelfde taak als doGameOver.
		NameInputControl nameInputControl;

		IButton* pGameOverPageStartButton;
		Flag flagStartPressed;

	public:
		GameOverControl(Task* pTask, IGameOverDisplayControl& gameOverDisplayControl, HallOfFame& hallOfFame) :
			pTask(pTask),
			gameOverDisplayControl(gameOverDisplayControl),
			hallOfFame(hallOfFame),
			nameInputControl(pTask, gameOverDisplayControl),
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
		void doGameOver(int32_t score)
		{
			gameOverDisplayControl.showGameOverPage();
			pTask->dumpStackHighWaterMarkIfIncreased(); // This function call takes about 0.25ms! It should be called while debugging only.

			// UC03 stap 1: alleen als de score een plek verdient.
			if (hallOfFame.isQualifyingScore(score))
			{
				char strName[MaxNofNameChars + 1];
				// STATE_WAITING_FOR_USER_NAME
				nameInputControl.doQueryName(strName, sizeof(strName));

				hallOfFame.insert(strName, score);
				hallOfFame.save();		// UC03 stap 1c.

				ESP_LOGI("pong", "top-5: '%s' met %d punten opgeslagen", strName, (int)score);
			}

			gameOverDisplayControl.showHallOfFame(hallOfFame.getHallOfFameData());
			flagStartPressed.clear();

			// STATE_WAITING_FOR_START
			pTask->wait(flagStartPressed);

			gameOverDisplayControl.hideGameOverPage();
		}
	}; // end class GameOverControl

};// end namespace pong_cleangui_dg_adv
