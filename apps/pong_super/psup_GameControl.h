// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_Logger.h>

#include <stdlib.h>
#include <esp_timer.h>

#include "psup_GameDisplayControl.h"
#include "psup_Spawnables.h"
#include "psup_IBallListener.h"
#include "psup_PaddleControl.h"

using namespace crt;
namespace pong_super
{
	// GamePlaying, invariant: per 3 seconden dat het spel loopt, komt er een object
	// (present crate of fire bin) bij.
	const int SpawnIntervalMs = 3000;

	// Diezelfde invariant, uitgedrukt in beeldstappen. 
	const int NofFramesPerSpawnInterval = SpawnIntervalMs / FrameIntervalMs;

	// Uitzondering: het GAME OVER-vignet blijft hooguit zo lang staan. Een
	// tik erop gaat voor.
	const int GameOverLabelTimeoutMs = 3000;

	class GameControl : public IBallListener, public IButtonListener
	{
	private:
		Task* pTask; // De taak waar dit control object wordt uitgevoerd / is samengevoegd.
		GameDisplayControl& gameDisplayControl;
		PaddleControl& paddleControl;

		// Dit object bezit de depots en dus de vijftig ballen, de crates en de vuurkorven.
		Spawnables spawnables;

		// De score van het lopende spel. 

		// Geen Pool nodig : ophogen en uitlezen gebeuren allebei in deze taak. Een bal
		// die kaatst meldt het via ballBouncedOnPaddle(), en publishStates() leest hem
		// elk beeld - allebei binnen de updateSpawnables() die de lus van doGame()
		// zelf aanroept.
		int32_t score;

		Flag flagGameOverLabelPressed;
		IButton* pGameOverLabel;

		Timer timerGameOverLabel;

		// De invariant-klok, afgeteld in beeldstappen; zie NofFramesPerSpawnInterval.
		int nofFramesUntilSpawn;

		// De beeldklok van het spel: FrameIntervalMs, 25 beelden per seconde.
		// Verschillende andere taken zijn samengenomen met deze taak,
		// steeds een processing-slice krijgend via een update() call
		// (FSM, Finite State Machine Paradigm),
		// om te voorkomen dat er teveel taken (en taak-synchronisatie-overhead)
		// zouden komen.
		Timer timerFrame;

	public:
		GameControl(Task* pTask, GameDisplayControl& gameDisplayControl,
					PaddleControl& paddleControl) :
			pTask(pTask),
			gameDisplayControl(gameDisplayControl),
			paddleControl(paddleControl),
			spawnables(gameDisplayControl),
			score(0),
			flagGameOverLabelPressed(pTask),
			pGameOverLabel(nullptr),
			timerGameOverLabel(pTask),
			nofFramesUntilSpawn(NofFramesPerSpawnInterval),
			timerFrame(pTask)
		{
			// Eenmalig de seed zetten (ipv in elk game object).
			srand((unsigned int)esp_timer_get_time());

			// De ballen melden hun peddelkaatsingen rechtstreeks aan het spel.
			spawnables.setBallListener(this);

			pGameOverLabel = gameDisplayControl.addGameOverLabelListener(this);
		}

		// --- IBallListener: wat een bal meldt ---
		// Alleen een kaatsing op de peddel is een verdienste van de
		// speler. De wanden en de crates leveren niets op.
		/*override*/ void ballBouncedOnPaddle()
		{
			score++;
		}

		// De score van het zojuist gespeelde spel.
		int32_t getScore()
		{
			return score;
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

			pTask->dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

			// Alles wat per spel opnieuw begint, bij elkaar: de score telt omhoog
			// vanaf nul (ballBouncedOnPaddle) en de spawncounter begint vol.
			score = 0;
			nofFramesUntilSpawn = NofFramesPerSpawnInterval;

			paddleControl.enable();
			(void)spawnables.spawnBallInTheMiddle();

			// One and only state for now:
			// STATE_GAME_RUNNING:
			logger.logText("STATE_GAME_RUNNING");

			timerFrame.start_periodic(FrameIntervalMs * 1000);

			while (true)
			{
				// NB: door te wachten op een periodieke timer krijg je 
				// strakker reproduceerbare timing dan met vTaskDelay
				// (omdat bij laatsgenoemde je ook afhankelijk bent van hoeveel
				//  tijd is doorgebracht tijdens updateSpawnables)
				pTask->wait(timerFrame);

				bool bAllBallsGone = updateSpawnables();
				if (bAllBallsGone)
				{
					break;		// De laatste bal is weg.
				}

				nofFramesUntilSpawn--;
				if (nofFramesUntilSpawn <= 0)
				{
					nofFramesUntilSpawn = NofFramesPerSpawnInterval;
					spawnObject();
				}
			}

			// De game is afgelopen. Zet de beeldklok voor de spawnables uit. 
			timerFrame.stop();
			timerFrame.clear();

			// Het veld leegmaken.
			spawnables.releaseCratesAndFireBins();

			// Een keer publiceren, want releaseCratesAndFireBins() haalt ze alleen uit
			// de depots - het scherm weet er pas van bij de eerstvolgende
			// publishStates().
			spawnables.publishStates(score);

			// Paddleknoppen uitzetten.
			paddleControl.disable();

			// "Game Over" over het spelscherm heen zetten, en weg na
			// een tik of drie seconden - wat het eerst komt. 
			// Het spelbord blijft eronder staan. 
			// Pas daarna de Game Pagina verlaten.
			doShowGameOverLabel();
			gameDisplayControl.hideGamePage();
		}

	private:
		// Hier wordt bepaald wat er per beeld met de spelobjecten gebeurt, en
		// in welke volgorde. Aangeroepen op elke tik van timerFrame.
		//
		// Geeft true als er na deze beeldstap geen bal meer in het veld is, en het
		// spel dus afgelopen is. 
		//
		// NB: deze functie staat niet in Spawnables zelf.
		// Reden: Spawnables zie ik als een entity, een datacontainer met
		// eenvoudige convenience functies - voor massa aanroepen, setten en getten.
		// Hier, in GameControl wordt bepaald of en wanneer ze worden aangeroepen.
		bool updateSpawnables()
		{
			// De peddelstand een keer ophalen, voordat er iets beweegt. Daarmee
			// rekenen alle ballen met deze tijdelijke kopie, en gaat de
			// lock van de Pool een keer om in plaats van vijftig keer.
			spawnables.setPaddlePosition(paddleControl.getPaddlePosition());

			// Crates en vuurkorven eerst. De korven tellen hun levensduur af en
			// geven zichzelf terug als ze op zijn.
			spawnables.updateCratesAndFireBins();

			// Een bal die daarbij het scherm verlaat geeft zichzelf
			// terug aan het depot, en een bal die een crate opent laat er tien
			// bijkomen.
			spawnables.updateBalls();

			// De score gaat in hetzelfde beeldbericht mee als de standen. Twee losse
			// berichten zouden twee hertekeningen kunnen opleveren.
			spawnables.publishStates(score);

			// Check of het spel al ten einde is gekomen.
			return (spawnables.getNofBallsInUse() == 0);
		}

		void doShowGameOverLabel()
		{
			logger.logText("STATE_SHOW_GAME_OVER_LABEL");

			flagGameOverLabelPressed.clear();
			gameDisplayControl.showGameOverLabel();

			timerGameOverLabel.start(GameOverLabelTimeoutMs * 1000);
			pTask->waitAny(flagGameOverLabelPressed + timerGameOverLabel);

			// Welke van de twee het was maakt niet uit - het vignet gaat zo weg. 
			flagGameOverLabelPressed.clear();

			// Stop de klok, anders tikt hij straks middenin het volgende spel.
			timerGameOverLabel.stop();
			timerGameOverLabel.clear();

			gameDisplayControl.hideGameOverLabel();
		}

		// Het beslismoment over het spawnen van een vuurkorf of een present crate.
		//
		// De kans op een vuurkorf is evenredig met het aantal ballen in het veld,
		// en vanaf NofBallsForCertainFireBin is het er altijd een. Dat is de rem op
		// de present crates: elke geopende crate levert er tien bij, dus zonder
		// deze weging loopt het veld vol en komt er nooit meer een eind aan het
		// spel.
		void spawnObject()
		{
			int posX, posY;
			pickRandomSpawnPosition(posX, posY);

			int nofBalls = spawnables.getNofBallsInUse();
			bool bFireBin = (nofBalls >= NofBallsForCertainFireBin) ||
			                ((int)(rand() % NofBallsForCertainFireBin) < nofBalls);

			// Zit de gekozen soort vol, dan de andere.
			if (bFireBin)
			{
				(void)spawnFireBin(posX, posY, nofBalls);
			}
			else
			{
				(void)spawnCrate(posX, posY, nofBalls);
			}
		}

		// Geeft terug of het gelukt is; een vol depot is geen fout maar een uitkomst,
		// en het depot meldt het zelf.
		bool spawnFireBin(int posX, int posY, int nofBalls)
		{
			bool bSpawned = spawnables.spawnFireBinAt(posX, posY);
			if (!bSpawned)
			{
				return false;
			}

#ifdef PONG_MEASURE
			ESP_LOGI("pong", "vuurkorf op (%d,%d) bij %d ballen, nu %d in het veld",
				posX, posY, nofBalls, spawnables.getNofFireBinsInUse());
#else
			(void)nofBalls;
#endif
			return true;
		}

		// Zelfde afspraak als spawnFireBin().
		bool spawnCrate(int posX, int posY, int nofBalls)
		{
			bool bSpawned = spawnables.spawnCrateAt(posX, posY);
			if (!bSpawned)
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

		// Spawn ergens in de bovenste helft van het spel. Met de halve crate
		// marge aan alle kanten, zodat hij niet half in een wand of half boven de
		// helft komt te staan.
		void pickRandomSpawnPosition(int& posX, int& posY)
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

};// end namespace pong_super
