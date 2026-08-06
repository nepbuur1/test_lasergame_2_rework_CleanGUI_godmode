// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>

#include <stdlib.h>
#include <esp_timer.h>

#include "pdga_GameDisplayControl.h"
#include "pdga_BallControl.h"
#include "pdga_PresentCrateControl.h"
#include "pdga_FireBinControl.h"
#include "pdga_GameObjectsControl.h"
#include "pdga_PaddleControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// UC04_GamePlaying, invariant: staat er drie seconden lang geen bal door de
	// onderlijn, dan komt er een object bij.
	const int SpawnIntervalMs = 3000;

	// UC04, uitzondering: het GAME OVER-vignet blijft hooguit zo lang staan. Een
	// tik erop gaat voor.
	const int GameOverLabelTimeoutMs = 3000;

	class GameControl : public IGameObjectsListener, public IButtonListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		IGameDisplayControl& gameDisplayControl;
		BallDepot& ballDepot;
		CrateDepot& crateDepot;
		FireBinDepot& fireBinDepot;
		GameObjectsControl& gameObjectsControl;
		PaddleControl& paddleControl;

		Flag flagAllBallsGone;
		Flag flagGameOverLabelPressed;
		IButton* pGameOverLabel;

		// Eenmalig, voor de uitzondering van UC04.
		Timer timerGameOverLabel;

		// De invariant-klok van UC04. Een Timer en geen vTaskDelay: er moet
		// tegelijk op het einde van het spel gewacht kunnen worden, en dat gaat
		// alleen in dezelfde waitAny.
		Timer timerSpawn;

	public:
		GameControl(Task* pTask, IGameDisplayControl& gameDisplayControl,
					BallDepot& ballDepot, CrateDepot& crateDepot, FireBinDepot& fireBinDepot,
					GameObjectsControl& gameObjectsControl, PaddleControl& paddleControl) :
			pTask(pTask),
			gameDisplayControl(gameDisplayControl),
			ballDepot(ballDepot),
			crateDepot(crateDepot),
			fireBinDepot(fireBinDepot),
			gameObjectsControl(gameObjectsControl),
			paddleControl(paddleControl),
			flagAllBallsGone(pTask),
			flagGameOverLabelPressed(pTask),
			pGameOverLabel(nullptr),
			timerGameOverLabel(pTask),
			timerSpawn(pTask)
		{
			// Eenmalig zaaien, hier en niet in elk spelobject: srand() zet de reeks
			// terug, dus vijftig keer achter elkaar zaaien met vrijwel dezelfde
			// klokstand levert juist minder toeval op.
			srand((unsigned int)esp_timer_get_time());

			gameObjectsControl.setGameObjectsListener(this);
			pGameOverLabel = gameDisplayControl.addGameOverLabelListener(this);

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

			for (int i = 0; i < MaxNofFireBins; i++)
			{
				fireBinDepot.getItem(i).init(&fireBinDepot, i);
			}
		}

		// De score van het zojuist gespeelde spel, voor UC03. Uitgelezen nadat
		// doGame() is teruggekeerd, dus als er niets meer aan telt.
		int32_t getScore()
		{
			return gameObjectsControl.getScore();
		}

		/*override*/ void allBallsGone()
		{
			flagAllBallsGone.set();
		}

		/*override*/ void buttonPressed(IButton* pButton)
		{
			if (pButton == pGameOverLabel)
			{
				flagGameOverLabelPressed.set();
			}
		}

		/*override*/ void buttonReleased(IButton* pButton)
		{
			// niets te doen.
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

			// Elk spel begint op nul (UC05 stap 1a telt hem omhoog).
			gameObjectsControl.resetScore();

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

			// Het veld leegmaken. Crates en vuurkorven verdwijnen alleen op hun
			// eigen voorwaarden - een crate als hij opengaat, een korf als hij
			// uitbrandt - dus een crate die niet is opengespeeld blijft in zijn
			// slot zitten. Zonder dit stonden de kistjes van het vorige spel er bij
			// het volgende gewoon weer, want het depot was ze nooit kwijtgeraakt.
			//
			// Hier en niet bij het begin van het volgende spel: de standen gaan pas
			// bij de eerstvolgende beeldstap naar de display, en die valt tussen
			// showGamePage() en de eerste hertekening in. Opruimen aan deze kant
			// geeft de game-over-pagina ruim de tijd om dat door te laten lopen.
			//
			// De ballen hoeven niet: die zijn op, dat was juist de reden dat we hier
			// staan.
			crateDepot.releaseAll();
			fireBinDepot.releaseAll();

			// De peddelknoppen horen bij het spel, niet bij de game-over-pagina.
			paddleControl.disable();

			// UC04, uitzondering: "Game Over" over het spelscherm heen, en weg na
			// een tik of drie seconden - wat het eerst komt. Het spelbord blijft
			// eronder staan; pas daarna gaat de pagina uit.
			showGameOverLabel();

			gameDisplayControl.hideGamePage();
		}

	private:
		void showGameOverLabel()
		{
			flagGameOverLabelPressed.clear();
			gameDisplayControl.showGameOverLabel();

			timerGameOverLabel.start(GameOverLabelTimeoutMs * 1000);
			pTask->waitAny(flagGameOverLabelPressed + timerGameOverLabel);

			// Allebei aflezen, ook al doen we met de uitkomst niets: hasFired() is
			// wat het event-bit wist. Zonder dit blijft het bit van de timer na de
			// eerste keer staan, en komt elke volgende waitAny meteen terug - dan
			// flitst het vignet voorbij in plaats van drie seconden te blijven.
			pTask->hasFired(flagGameOverLabelPressed);
			pTask->hasFired(timerGameOverLabel);

			timerGameOverLabel.stop();

			gameDisplayControl.hideGameOverLabel();
		}

		// Het beslismoment van UC04: een vuurkorf of een present crate.
		//
		// De kans op een vuurkorf is evenredig met het aantal ballen in het veld,
		// en vanaf NofBallsForCertainFireBin is het er altijd een. Dat is de rem op
		// de present crates: elke geopende crate levert er tien bij, dus zonder
		// deze weging loopt het veld vol en komt er nooit meer een eind aan het
		// spel.
		void spawnObject()
		{
			int posX, posY;
			pickSpawnPosition(posX, posY);

			int nofBalls = gameObjectsControl.getNofBallsInUse();
			bool bFireBin = (nofBalls >= NofBallsForCertainFireBin) ||
			                ((int)(rand() % NofBallsForCertainFireBin) < nofBalls);

			// Zit de gekozen soort vol, dan de andere. Zonder deze terugval gaat de
			// tik verloren: met een enkele bal valt de keuze in 39 van de 40
			// gevallen op een crate, en staan die alle vijf al in het veld, dan
			// gebeurt er drie seconden lang niets - en daarna weer niet. Het
			// speelveld staat dan stil terwijl de invariant juist zegt dat er iets
			// bij hoort te komen.
			if (bFireBin)
			{
				if (spawnFireBin(posX, posY, nofBalls)) { return; }
				spawnCrate(posX, posY, nofBalls);
			}
			else
			{
				if (spawnCrate(posX, posY, nofBalls)) { return; }
				spawnFireBin(posX, posY, nofBalls);
			}
		}

		bool spawnFireBin(int posX, int posY, int nofBalls)
		{
			if (!gameObjectsControl.spawnFireBinAt(posX, posY))
			{
				return false;		// vol depot meldt zichzelf.
			}

#ifdef PONG_MEASURE
			ESP_LOGI("pong", "vuurkorf op (%d,%d) bij %d ballen, nu %d in het veld",
				posX, posY, nofBalls, fireBinDepot.getNofInUse());
#else
			(void)nofBalls;
#endif
			return true;
		}

		bool spawnCrate(int posX, int posY, int nofBalls)
		{
			if (!gameObjectsControl.spawnCrateAt(posX, posY))
			{
				return false;
			}

#ifdef PONG_MEASURE
			ESP_LOGI("pong", "present crate op (%d,%d) bij %d ballen, nu %d in het veld",
				posX, posY, nofBalls, crateDepot.getNofInUse());
#else
			(void)nofBalls;
#endif
			return true;
		}

		// "Ergens in de bovenste helft van het spel" (UC04). Met de halve crate
		// marge aan alle kanten, zodat hij niet half in een wand of half boven de
		// helft komt te staan.
		void pickSpawnPosition(int& posX, int& posY)
		{
			int fieldWidth  = gameDisplayControl.getWidth();
			int fieldHeight = gameDisplayControl.getHeight();
			int halfCrate   = CrateSizePix/2;		// crate en vuurkorf zijn even groot

			int minX = WallThicknessPix + halfCrate;
			int maxX = fieldWidth - WallThicknessPix - halfCrate;
			int minY = fieldHeight/2 + halfCrate;
			int maxY = fieldHeight - WallThicknessPix - halfCrate;

			posX = minX + (int)(rand() % (unsigned int)(maxX - minX + 1));
			posY = minY + (int)(rand() % (unsigned int)(maxY - minY + 1));
		}
	}; // end class GameControl

};// end namespace pong_cleangui_dg_adv
