// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>

#include <stdlib.h>
#include <esp_timer.h>

#include "pdga_GameDisplayControl.h"
#include "pdga_BallControl.h"
#include "pdga_PresentCrateControl.h"
#include "pdga_GameObjectsControl.h"
#include "pdga_PaddleControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// UC04_GamePlaying, invariant: staat er drie seconden lang geen bal door de
	// onderlijn, dan komt er een object bij.
	const int SpawnIntervalMs = 3000;

	class GameControl : public IGameObjectsListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		IGameDisplayControl& gameDisplayControl;
		BallDepot& ballDepot;
		CrateDepot& crateDepot;
		GameObjectsControl& gameObjectsControl;
		PaddleControl& paddleControl;

		Flag flagAllBallsGone;

		// De invariant-klok van UC04. Een Timer en geen vTaskDelay: er moet
		// tegelijk op het einde van het spel gewacht kunnen worden, en dat gaat
		// alleen in dezelfde waitAny.
		Timer timerSpawn;

	public:
		GameControl(Task* pTask, IGameDisplayControl& gameDisplayControl,
					BallDepot& ballDepot, CrateDepot& crateDepot,
					GameObjectsControl& gameObjectsControl, PaddleControl& paddleControl) :
			pTask(pTask),
			gameDisplayControl(gameDisplayControl),
			ballDepot(ballDepot),
			crateDepot(crateDepot),
			gameObjectsControl(gameObjectsControl),
			paddleControl(paddleControl),
			flagAllBallsGone(pTask),
			timerSpawn(pTask)
		{
			// Eenmalig zaaien, hier en niet in elk spelobject: srand() zet de reeks
			// terug, dus vijftig keer achter elkaar zaaien met vrijwel dezelfde
			// klokstand levert juist minder toeval op.
			srand((unsigned int)esp_timer_get_time());

			gameObjectsControl.setGameObjectsListener(this);

			// Alle ballen en crates bedraden, ook die voorlopig niets doen. Ze
			// bestaan al - het depot bouwt ze in een keer - en een object dat pas
			// bij het spawnen zijn verwijzingen zou krijgen, zou die bedrading
			// telkens opnieuw moeten doen.
			for (int i = 0; i < MaxNofBalls; i++)
			{
				ballDepot.getItem(i).init(gameDisplayControl, gameObjectsControl.getPaddlePositionPtr(),
				                          &gameObjectsControl, &ballDepot, i);
			}

			for (int i = 0; i < MaxNofCrates; i++)
			{
				crateDepot.getItem(i).init(&crateDepot, i);
			}
		}

		/*override*/ void allBallsGone()
		{
			flagAllBallsGone.set();
		}

		// Elke do..()-functie vraagt zijn eigen pagina op en geeft hem aan het eind
		// weer vrij. Dat MainControl ze strikt na elkaar aanroept is wat garandeert
		// dat er nooit twee PageRoots tegelijk in beeld staan - de eis die boven
		// crt_PageRoot.h staat.
		/*override keyword not supported*/
		void doGame()
		{
			gameDisplayControl.showGamePage();
			flagAllBallsGone.clear();	// een afgelopen spel mag dit spel niet meteen beeindigen.

			pTask->dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

			// De peddel eerst: die moet er staan voordat de bal eraan komt.
			paddleControl.enable();

			// Een bal uit het depot halen en in het spel zetten. Een leeg depot is
			// geen fout maar een uitkomst - het depot meldt het zelf.
			int indexBall = ballDepot.obtain();
			if (indexBall != BallDepot::InvalidIndex)
			{
				ballDepot.getItem(indexBall).spawn();
			}

			// One and only state for now:
			// STATE_GAME_RUNNING:
			logger.logText("STATE_GAME_RUNNING");

			timerSpawn.start_periodic(SpawnIntervalMs * 1000);

			while (true)
			{
				pTask->waitAny(flagAllBallsGone + timerSpawn);

				if (pTask->hasFired(flagAllBallsGone))
				{
					break;
				}

				if (pTask->hasFired(timerSpawn))
				{
					spawnObject();
				}
			}

			timerSpawn.stop();

			// De peddelknoppen horen bij het spel, niet bij de game-over-pagina.
			paddleControl.disable();
			gameDisplayControl.hideGamePage();
		}

	private:
		// Het beslismoment van UC04. Daar hoort een keuze tussen een mijn
		// (UC05_MineHandling) en een present crate (UC07_PresentHandling); de
		// mijnen bestaan nog niet, dus voorlopig is het altijd een crate. Als de
		// mijnen erbij komen, is dit de ene plek waar dat wordt beslist.
		void spawnObject()
		{
			int posX, posY;
			pickSpawnPosition(posX, posY);

			if (!gameObjectsControl.spawnCrateAt(posX, posY))
			{
				return;		// alle crates staan al in het veld; het depot meldt het.
			}

			ESP_LOGI("pong", "present crate op (%d,%d), nu %d in het veld",
				posX, posY, crateDepot.getNofInUse());
		}

		// "Ergens in de bovenste helft van het spel" (UC04). Met de halve crate
		// marge aan alle kanten, zodat hij niet half in een wand of half boven de
		// helft komt te staan.
		void pickSpawnPosition(int& posX, int& posY)
		{
			int fieldWidth  = gameDisplayControl.getWidth();
			int fieldHeight = gameDisplayControl.getHeight();
			int halfCrate   = CrateSizePix/2;

			int minX = WallThicknessPix + halfCrate;
			int maxX = fieldWidth - WallThicknessPix - halfCrate;
			int minY = fieldHeight/2 + halfCrate;
			int maxY = fieldHeight - WallThicknessPix - halfCrate;

			posX = minX + (int)(rand() % (unsigned int)(maxX - minX + 1));
			posY = minY + (int)(rand() % (unsigned int)(maxY - minY + 1));
		}
	}; // end class GameControl

};// end namespace pong_cleangui_dg_adv
