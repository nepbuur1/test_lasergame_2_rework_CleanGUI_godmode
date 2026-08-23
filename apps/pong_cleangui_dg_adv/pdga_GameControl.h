// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_Logger.h>

#include <stdlib.h>
#include <esp_timer.h>

#include "pdga_GameDisplayControl.h"
#include "pdga_Spawnables.h"
#include "pdga_IBallListener.h"
#include "pdga_PaddleControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// UC04_GamePlaying, invariant: staat er drie seconden lang geen bal door de
	// onderlijn, dan komt er een object bij.
	const int SpawnIntervalMs = 3000;

	// Diezelfde invariant, uitgedrukt in beeldstappen. Geteld op de beeldklok in
	// plaats van op een eigen Timer: dat scheelt een esp_timer en een event-bit, en
	// het haalt een valkuil weg. Een timertik die vlak voor het einde van een spel
	// valt en niet meer wordt getoetst, blijft als bit staan en zet in het volgende
	// spel meteen een object neer; een teller die doGame() bij het begin zet, kan
	// dat niet.
	const int NofFramesPerSpawnInterval = SpawnIntervalMs / FrameIntervalMs;

	// UC04, uitzondering: het GAME OVER-vignet blijft hooguit zo lang staan. Een
	// tik erop gaat voor.
	const int GameOverLabelTimeoutMs = 3000;

	class GameControl : public IBallListener, public IButtonListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		GameDisplayControl& gameDisplayControl;
		PaddleControl& paddleControl;

		// Eigendom, geen verwijzing: dit object bezit de depots en dus de vijftig
		// ballen, de crates en de vuurkorven. Het bezit hoort hier omdat de beeldklok
		// hier draait - zie updateSpawnables(). Zolang GameControl in BSS staat, staat
		// dit dat ook: het zit in MainControl, en dat is een global in de ino. Op een
		// stack zou het er meteen door omvallen.
		Spawnables spawnables;

		// De score van het lopende spel (UC05 stap 1a). Hij woont hier omdat hij bij
		// het spel hoort en niet bij de objecten die hem verdienen.
		//
		// Geen Pool nodig, en sinds de beeldklok hier woont ook niet meer om een
		// subtiele reden: ophogen en uitlezen gebeuren allebei in deze taak. Een bal
		// die kaatst meldt het via ballBouncedOnPaddle(), en publishStates() leest hem
		// elk beeld - allebei binnen de updateSpawnables() die de lus van doGame()
		// zelf aanroept. Hier stond eerder een redenering over een ondeelbare int32_t
		// over twee taken heen; die is niet meer nodig.
		int32_t score;

		Flag flagGameOverLabelPressed;
		IButton* pGameOverLabel;

		// Eenmalig, voor de uitzondering van UC04.
		Timer timerGameOverLabel;

		// De invariant-klok van UC04, afgeteld in beeldstappen; zie
		// NofFramesPerSpawnInterval.
		int nofFramesUntilSpawn;

		// De beeldklok van het spel: FrameIntervalMs, 25 beelden per seconde.
		//
		// Deze woont hier, en niet meer in een eigen Handler-taak, om een reden die
		// niets met beeld te maken heeft: Depot::obtain() en release() zijn
		// lock-vrij (zie pdga_Depot.h). Zolang de beeldklok in een andere taak liep
		// dan de spawns hieronder, werden Spawnables en de drie depots door twee
		// taken aangeraakt, en moest het goedgaan op impliciete invarianten. Nu doet
		// een taak alles en is er niets meer om over te redeneren.
		//
		// Hij loopt alleen tijdens de spellus in doGame(), en daarbuiten staat het
		// spelbord dus stil - wat mag, want daarbuiten is er geen bal meer om te
		// bewegen. Wie hier een wacht bijbouwt hoeft er daarom niets voor te doen.
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
			// Eenmalig zaaien, hier en niet in elk spelobject: srand() zet de reeks
			// terug, dus vijftig keer achter elkaar zaaien met vrijwel dezelfde
			// klokstand levert juist minder toeval op.
			srand((unsigned int)esp_timer_get_time());

			// De ballen melden hun peddelkaatsingen rechtstreeks aan het spel en gaan
			// daarvoor niet langs hun veld; zie pdga_IBallListener.h. Dat blijft een
			// interface, want een bal mag GameControl niet kennen. Hier stond ook nog
			// een setListener() voor de beeldklok - die melding is nu de returnwaarde
			// van updateSpawnables(), want die functie staat in deze klasse zelf.
			spawnables.setBallListener(this);

			pGameOverLabel = gameDisplayControl.addGameOverLabelListener(this);
		}

		// --- IBallListener: wat een bal meldt ---

		// UC05 stap 1a: alleen een kaatsing op de peddel is een verdienste van de
		// speler. De wanden en de crates leveren niets op.
		/*override*/ void ballBouncedOnPaddle()
		{
			score++;
		}

		// De score van het zojuist gespeelde spel, voor UC03. Gelezen door
		// MainControl, nadat doGame() is teruggekeerd.
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
			// vanaf nul (ballBouncedOnPaddle) en de spawnklok begint vol.
			score = 0;
			nofFramesUntilSpawn = NofFramesPerSpawnInterval;

			// De peddel eerst: die moet er staan voordat de bal eraan komt.
			paddleControl.enable();

			// De eerste bal in het spel zetten. Een leeg depot is geen fout maar een
			// uitkomst - het depot meldt het zelf - dus de uitkomst boeit hier niet.
			(void)spawnables.spawnBallInTheMiddle();

			// One and only state for now:
			// STATE_GAME_RUNNING:
			logger.logText("STATE_GAME_RUNNING");

			// De beeldklok aan. Vanaf hier draait de hele spelupdate in deze taak, in
			// de lus hieronder, en hangt alles aan deze ene tik: het bewegen, het
			// tekenen, het einde van het spel en de invariant van UC04.
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
					break;		// UC04 stap 4: de laatste bal is weg.
				}

				nofFramesUntilSpawn--;
				if (nofFramesUntilSpawn <= 0)
				{
					nofFramesUntilSpawn = NofFramesPerSpawnInterval;
					spawnObject();
				}
			}

			// De beeldklok uit. Alles hierna gebeurt op eigen initiatief, niet meer op
			// een tik - zie het enkele publishStates() hieronder.
			timerFrame.stop();
			timerFrame.clear();

			// Het veld leegmaken; waarom dat nodig is staat bij
			// Spawnables::releaseCratesAndFireBins().
			//
			// Hier en niet bij het begin van het volgende spel: dan zou het opruimen
			// tussen showGamePage() en de eerste hertekening in vallen, en flitsten de
			// kistjes van het vorige spel nog een beeld lang mee.
			spawnables.releaseCratesAndFireBins();

			// Een keer publiceren, want releaseCratesAndFireBins() haalt ze alleen uit
			// de depots - het scherm weet er pas van bij de eerstvolgende
			// publishStates(). Een keer is genoeg: er is geen bal meer in het veld en
			// dus niets dat nog animeert, dus een tweede beeld zou hetzelfde tonen.
			spawnables.publishStates(score);

			// De peddelknoppen horen bij het spel, niet bij de game-over-pagina.
			paddleControl.disable();

			// UC04, uitzondering: "Game Over" over het spelscherm heen, en weg na
			// een tik of drie seconden - wat het eerst komt. Het spelbord blijft
			// eronder staan; pas daarna gaat de pagina uit.
			doShowGameOverLabel();

			gameDisplayControl.hideGamePage();
		}

	private:
		// De beeldklok van het spel: wat er per beeld met de spelobjecten gebeurt, en
		// in welke volgorde. Aangeroepen op elke tik van timerFrame.
		//
		// Dat dit een functie op zich is en niet los in de lus hierboven staat, is
		// precies om die volgorde. Die is geen implementatiedetail - er zitten drie
		// afspraken in die je aan de losse depots niet kunt zien - en zulke afspraken
		// horen op een plek te staan waar ze te lezen zijn in plaats van verspreid
		// over de objecten die eraan meedoen.
		//
		// De depots zijn ook niet zelf aan de klok te hangen, en dat is met opzet: dan
		// bepaalt de registratievolgorde of het beeld klopt, en dat is het soort
		// afhankelijkheid dat stilzwijgend stukgaat zodra iemand een regel verplaatst.
		//
		// Dat dit hier staat en niet in een eigen klasse met een eigen taak, is geen
		// beeldkwestie maar een taakkwestie: Depot::obtain() en release() zijn
		// lock-vrij (zie pdga_Depot.h), dus Spawnables en de drie depots horen door
		// precies een taak aangeraakt te worden. Zolang de beeldklok elders liep waren
		// dat er twee - deze functie aan de ene kant, de spawns en het opruimen in
		// doGame() aan de andere - en moest het goedgaan op invarianten die nergens
		// waren afgedwongen.
		//
		// Geeft true als er na deze beeldstap geen bal meer in het veld is, en het
		// spel dus afgelopen is (UC04 stap 4). Een returnwaarde en geen vlag: een vlag
		// zou toestand zijn die doGame() bij elk spel moet wissen, en die er dus ook
		// een keer kan blijven staan.
		//
		// NB: deze functie staat niet in Spawnables zelf.
		// Reden: Spawnables zie ik als een entity, een datacontainer met
		// eenvoudige convenience functies - voor massa aanroepen, setten en getten.
		// Hier, in GameControl wordt bepaald of en wanneer ze worden aangeroepen.
		bool updateSpawnables()
		{
			// De peddelstand een keer ophalen, voordat er iets beweegt. Daarmee
			// rekenen alle ballen van dit beeld met dezelfde peddel, en gaat het
			// slot van de Pool een keer om in plaats van vijftig keer.
			spawnables.setPaddlePosition(paddleControl.getPaddlePosition());

			// Crates en vuurkorven eerst. De korven tellen hun levensduur af en
			// geven zichzelf terug als ze op zijn (UC08); dat moet gebeurd zijn
			// voordat de ballen kijken of ze er een raken.
			spawnables.updateCratesAndFireBins();

			// Dan de ballen. Een bal die daarbij het scherm verlaat geeft zichzelf
			// terug aan het depot, en een bal die een crate opent laat er tien
			// bijkomen. Allebei mag midden in die lus: een Depot perst zijn
			// administratie niet samen. Een bal die in een hoger slot bijkomt doet
			// dit beeld nog een stap, een in een lager slot pas het volgende - en
			// geen van beide is te zien.
			spawnables.updateBalls();

			// De score gaat in hetzelfde beeldbericht mee als de standen; twee losse
			// berichten zouden twee hertekeningen kunnen opleveren.
			spawnables.publishStates(score);

			// UC04 stap 4. De kale stand volstaat, want doGame() zet de eerste bal in
			// het veld voordat hij de beeldklok start: draait deze functie, dan is er
			// al een bal geweest. Verplaats die spawn dus niet naar achteren.
			//
			// Hier stond een toets op de OVERGANG naar nul, met een vorige stand
			// erbij. Die was nodig toen de beeldklok een eigen taak had en eeuwig
			// doortikte, ook waar nul ballen de ruststand is. Nu is hij niet alleen
			// overbodig maar ronduit ongewenst: mocht de eerste spawn ooit mislukken,
			// dan zou de overgang nooit komen en zou de lus in doGame() nooit meer
			// afbreken.
			return (spawnables.getNofBallsInUse() == 0);
		}

		void doShowGameOverLabel()
		{
			logger.logText("STATE_SHOW_GAME_OVER_LABEL");

			flagGameOverLabelPressed.clear();
			gameDisplayControl.showGameOverLabel();

			// Geen beeldklok nodig: het spelbord staat hier stil. Het is al in zijn
			// eindstand gepubliceerd door de aanroeper, en zonder ballen valt er niets
			// meer te animeren.
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
			pickRandomSpawnPosition(posX, posY);

			int nofBalls = spawnables.getNofBallsInUse();
			bool bFireBin = (nofBalls >= NofBallsForCertainFireBin) ||
			                ((int)(rand() % NofBallsForCertainFireBin) < nofBalls);

			// Zit de gekozen soort vol, dan de andere. Zonder deze terugval gaat de
			// tik verloren: met een enkele bal valt de keuze in 39 van de 40
			// gevallen op een crate, en staan die alle vijf al in het veld, dan
			// gebeurt er drie seconden lang niets - en daarna weer niet. Het
			// speelveld staat dan stil terwijl de invariant juist zegt dat er iets
			// bij hoort te komen.
			// De tweede poging levert wel een uitkomst op, maar er is niets meer mee te
			// doen: zijn ze allebei vol, dan gaat deze tik verloren en komt er over
			// drie seconden een nieuwe. Vandaar de (void) - bewust genegeerd, niet
			// vergeten.
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

		// "Ergens in de bovenste helft van het spel" (UC04). Met de halve crate
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

};// end namespace pong_cleangui_dg_adv
