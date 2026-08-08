// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_Logger.h>

#include <stdlib.h>
#include <esp_timer.h>

#include "pdga_GameDisplayControl.h"
#include "pdga_Spawnables.h"
#include "pdga_SpawnablesHandler.h"
#include "pdga_IBallListener.h"
#include "pdga_ISpawnablesHandlerListener.h"
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

	class GameControl : public IBallListener, public ISpawnablesHandlerListener, public IButtonListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		IGameDisplayControl& gameDisplayControl;
		Spawnables& spawnables;
		SpawnablesHandler& spawnablesHandler;
		PaddleControl& paddleControl;

		// De score van het lopende spel (UC05 stap 1a). Hij woont hier omdat hij bij
		// het spel hoort en niet bij de objecten die hem verdienen.
		//
		// Let op de taken: opgehoogd en uitgelezen vanuit de Handler-taak - een bal
		// die kaatst meldt dat, en publishStates() vraagt hem elk beeld op - terwijl
		// dit object in de taak van MainControl woont. Geen Pool nodig: een
		// uitgelijnde int32_t is op de ESP32 ondeelbaar, en een beeld verschil in
		// wat het scherm toont is niet te zien. De lezer die er wel om geeft is
		// MainControl, via getScore(), en die leest pas nadat doGame() is
		// teruggekeerd - dus als er niets meer aan telt.
		int32_t score;

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
					Spawnables& spawnables, SpawnablesHandler& spawnablesHandler,
					PaddleControl& paddleControl) :
			pTask(pTask),
			gameDisplayControl(gameDisplayControl),
			spawnables(spawnables),
			spawnablesHandler(spawnablesHandler),
			paddleControl(paddleControl),
			score(0),
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

			// Twee aparte bedradingen, want het zijn twee verschillende rollen. De
			// ballen melden hun peddelkaatsingen (IBallListener) en gaan daarvoor
			// niet meer langs hun veld; de beeldklok meldt dat de laatste bal weg is
			// en vraagt elk beeld de score op (ISpawnablesHandlerListener).
			spawnables.setBallListener(this);
			spawnablesHandler.setListener(this);

			pGameOverLabel = gameDisplayControl.addGameOverLabelListener(this);
		}

		// --- IBallListener: wat een bal meldt ---

		// UC05 stap 1a: alleen een kaatsing op de peddel is een verdienste van de
		// speler. De wanden en de crates leveren niets op.
		/*override*/ void ballBouncedOnPaddle()
		{
			score++;
		}

		// --- ISpawnablesHandlerListener: wat de beeldklok meldt en vraagt ---

		// De score van het zojuist gespeelde spel, voor UC03. Ook elk beeld gevraagd
		// door de beeldklok, om hem op het scherm te zetten.
		/*override*/ int32_t getScore()
		{
			return score;
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

			// Elk spel begint op nul (ballBouncedOnPaddle telt hem omhoog).
			score = 0;

			// De peddel eerst: die moet er staan voordat de bal eraan komt.
			paddleControl.enable();

			// De eerste bal in het spel zetten. Een leeg depot is geen fout maar een
			// uitkomst - het depot meldt het zelf - dus de uitkomst boeit hier niet.
			spawnables.spawnBallInTheMiddle();

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

			// Het veld leegmaken; waarom dat nodig is staat bij
			// Spawnables::releaseCratesAndFireBins().
			//
			// Hier en niet bij het begin van het volgende spel: de standen gaan pas
			// bij de eerstvolgende beeldstap naar de display, en die valt tussen
			// showGamePage() en de eerste hertekening in. Opruimen aan deze kant
			// geeft de game-over-pagina ruim de tijd om dat door te laten lopen.
			spawnables.releaseCratesAndFireBins();

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

			// Welke van de twee het was maakt niet uit - het vignet gaat zo weg. Maar
			// gebeurd is gebeurd, en dat moet weg: waitAny laat de event-bits staan
			// (xClearOnExit is pdFALSE), dus zonder dit blijft het bit van de timer
			// na de eerste keer staan en komt elke volgende waitAny meteen terug -
			// dan flitst het vignet voorbij in plaats van drie seconden te blijven.
			flagGameOverLabelPressed.clear();

			// Stop de klok, anders tikt hij straks middenin het volgende spel.
			timerGameOverLabel.stop();
			timerGameOverLabel.clear();

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

			int nofBalls = spawnables.getNofBallsInUse();
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
			if (!spawnables.spawnFireBinAt(posX, posY))
			{
				return false;		// vol depot meldt zichzelf.
			}

#ifdef PONG_MEASURE
			ESP_LOGI("pong", "vuurkorf op (%d,%d) bij %d ballen, nu %d in het veld",
				posX, posY, nofBalls, spawnables.getNofFireBinsInUse());
#else
			(void)nofBalls;
#endif
			return true;
		}

		bool spawnCrate(int posX, int posY, int nofBalls)
		{
			if (!spawnables.spawnCrateAt(posX, posY))
			{
				return false;
			}

#ifdef PONG_MEASURE
			ESP_LOGI("pong", "present crate op (%d,%d) bij %d ballen, nu %d in het veld",
				posX, posY, nofBalls, spawnables.getNofCratesInUse());
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
