// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_IHandlerListener.h>

#include "pdga_Depot.h"
#include "pdga_GameDisplayControl.h"
#include "pdga_BallControl.h"
#include "pdga_PresentCrateControl.h"
#include "pdga_FireBinControl.h"
#include "pdga_IBallField.h"
#include "pdga_PaddleControl.h"

// Wat er per beeld met de spelobjecten gebeurt: de ballen zetten een stap - en
// melden onderweg hun treffers aan de crates - en daarna gaan alle standen in
// EEN bericht naar de display.
//
// Dit is het enige dat zich bij de Handler aanmeldt, en dat is met opzet. De
// depots zijn zelf ook IHandlerListener en hadden er dus rechtstreeks bij gekund,
// met nog een listener erachter voor het publiceren - maar dan bepaalt de
// registratievolgorde bij de Handler of het beeld klopt, en dat is het soort
// afhankelijkheid dat stilzwijgend stukgaat zodra iemand een regel verplaatst.
// Hier staat de volgorde in de functie zelf.

// NB: door gebruik van dit extra tussenobject, GameObjectsControl, 
// kunnen alle ballen toegang krijgen tot de paddleposition, zonder 
// individueel een Pool uit te lezen. zelfde voor het schrijven 
// van hun state naar de GameDisplayControl. Dat kan nu in een keer
// voor de complete array/batch.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	typedef Depot<BallControl, MaxNofBalls> BallDepot;
	typedef Depot<PresentCrateControl, MaxNofCrates> CrateDepot;
	typedef Depot<FireBinControl, MaxNofFireBins> FireBinDepot;

	// UC04 stap 4: het spel loopt tot er geen ballen meer in het spel zijn. Wie de
	// depots afloopt is de enige die dat weet, dus die meldt het.
	class IGameObjectsListener
	{
	public:
		virtual void allBallsGone() = 0;
	};

	class GameObjectsControl : public IHandlerListener, public IBallField
	{
	private:
		BallDepot& ballDepot;
		CrateDepot& crateDepot;
		FireBinDepot& fireBinDepot;
		IGameDisplayControl& gameDisplayControl;
		PaddleControl& paddleControl;
		IGameObjectsListener* pListener;

		// De peddelstand van het beeld dat nu berekend wordt. Een keer per beeld
		// opgehaald en daarna door alle ballen gelezen - zie de toelichting bij
		// pPaddlePosition in pdga_BallControl.h.
		int paddlePositionThisFrame;

		// Worden elk beeld opnieuw gevuld en daarna weggeschreven. Als lid en niet
		// als lokale variabele: samen zijn ze bijna een kilobyte, en dat elk beeld
		// op de stack van de Handler-taak zetten is precies het soort ding waar een
		// stack-overflow uit voortkomt.
		BallStates ballStates;
		CrateStates crateStates;
		FireBinStates fireBinStates;

		// De score van het lopende spel (UC05 stap 1a). Hij woont hier omdat dit de
		// plek is waar de ballen hun kaatsingen melden; GameControl zet hem bij het
		// begin van een spel terug.
		int32_t score;

		// Om de overgang naar nul te kunnen zien. Alleen "er zijn er nul" is niet
		// genoeg: voordat de eerste bal wordt gespawnd is dat ook zo, en dan zou
		// het spel meteen aflopen.
		int nofBallsInUsePrev;

	public:
		GameObjectsControl(BallDepot& ballDepot, CrateDepot& crateDepot, FireBinDepot& fireBinDepot,
		                   IGameDisplayControl& gameDisplayControl, PaddleControl& paddleControl) :
			ballDepot(ballDepot), crateDepot(crateDepot), fireBinDepot(fireBinDepot),
			gameDisplayControl(gameDisplayControl), paddleControl(paddleControl),
			pListener(nullptr), paddlePositionThisFrame(0),
			score(0),
			nofBallsInUsePrev(0)
		{
		}

		// Waar de ballen hun peddelstand vandaan halen. Blijft geldig zolang dit
		// object bestaat, en dat is de hele looptijd.
		const int* getPaddlePositionPtr() const
		{
			return &paddlePositionThisFrame;
		}

		void setGameObjectsListener(IGameObjectsListener* pListener)
		{
			this->pListener = pListener;
		}

		// Een bal meldt dat hij ergens tegenaan zit. Zie IBallField.
		//
		// Dit draait binnen ballDepot.update() hieronder, dus binnen de update()
		// van de bal die het meldt. Dat mag: het crate-depot wordt op dat moment
		// niet afgelopen, en een crate vrijgeven verandert alleen een bool.
		/*override*/ CrateHit reportCrateHit(int ballPosX, int ballPosY, int ballRadius)
		{
			for (int i = 0; i < MaxNofCrates; i++)
			{
				if (!crateDepot.isInUse(i))
				{
					continue;
				}

				PresentCrateControl& crate = crateDepot.getItem(i);
				if (!crate.overlaps(ballPosX, ballPosY, ballRadius))
				{
					continue;
				}

				// De plek onthouden voordat de crate wordt vrijgegeven: de bal
				// heeft hem nodig om tegen te kaatsen, en een vrijgegeven slot mag
				// meteen weer worden uitgegeven.
				CrateHit hit(crate.getPosX(), crate.getPosY(), CrateSizePix);

				crate.registerHit();
				if (crate.isOpened())
				{
					// UC07 stap 3: de crate verdwijnt en levert ballen op de plek
					// waar hij stond.
					spawnBallsAt(hit.posX, hit.posY, NofBallsPerCrate);
					crateDepot.release(i);
				}

				return hit;		// hooguit een crate per beeld; ze overlappen niet.
			}

			return CrateHit();
		}

		// Staat de bal in een vuurkorf? Geen melding aan de korf: die doet niets
		// met ballen, hij staat er alleen (UC08). Wat er met de bal gebeurt staat
		// in BallControl.
		/*override*/ bool touchesFireBin(int ballPosX, int ballPosY, int ballRadius)
		{
			for (int i = 0; i < MaxNofFireBins; i++)
			{
				if (fireBinDepot.isInUse(i) &&
				    fireBinDepot.getItem(i).overlaps(ballPosX, ballPosY, ballRadius))
				{
					return true;
				}
			}
			return false;
		}

		// Een crate in het veld zetten. Aangeroepen vanuit GameControl (UC04), dus
		// vanuit een andere taak dan waarin update() draait. Het gaat om het vullen
		// van een slot dat nog van niemand was, dus daar zit geen race in die kwaad
		// kan.
		bool spawnCrateAt(int posX, int posY)
		{
			int indexCrate = crateDepot.obtain();
			if (indexCrate == CrateDepot::InvalidIndex)
			{
				return false;
			}

			crateDepot.getItem(indexCrate).spawn(posX, posY);
			return true;
		}

		// Een vuurkorf in het veld zetten. Zelfde afspraak als spawnCrateAt().
		bool spawnFireBinAt(int posX, int posY)
		{
			int indexFireBin = fireBinDepot.obtain();
			if (indexFireBin == FireBinDepot::InvalidIndex)
			{
				return false;
			}

			fireBinDepot.getItem(indexFireBin).spawn(posX, posY);
			return true;
		}

		int getNofBallsInUse()
		{
			return ballDepot.getNofInUse();
		}

		// UC05 stap 1a. Wordt aangeroepen vanuit de update() van een bal, dus in
		// dezelfde taak als waarin hij hieronder wordt uitgelezen.
		/*override*/ void reportPaddleBounce()
		{
			score++;
		}

		void resetScore()
		{
			score = 0;
		}

		int32_t getScore()
		{
			return score;
		}

		/*override*/ void update()
		{
			// De peddelstand een keer ophalen, voordat er iets beweegt. Daarmee
			// rekenen alle ballen van dit beeld met dezelfde peddel, en gaat het
			// slot van de Pool een keer om in plaats van vijftig keer.
			paddlePositionThisFrame = paddleControl.getPaddlePos();

			// Crates en vuurkorven eerst. De korven tellen hun levensduur af en
			// geven zichzelf terug als ze op zijn (UC08); dat moet gebeurd zijn
			// voordat de ballen kijken of ze er een raken.
			crateDepot.update();
			fireBinDepot.update();

			// Dan de ballen. Een bal die daarbij het scherm verlaat geeft zichzelf
			// terug aan het depot, en een bal die een crate opent laat er tien
			// bijkomen. Allebei mag midden in deze lus: een Depot perst zijn
			// administratie niet samen. Een bal die in een hoger slot bijkomt doet
			// dit beeld nog een stap, een in een lager slot pas het volgende - en
			// geen van beide is te zien.
			ballDepot.update();

			publishStates();

			// UC04 stap 4. De overgang telt, niet de stand: voor de eerste spawn
			// staat de teller ook op nul.
			int nofBallsInUse = ballDepot.getNofInUse();
			if ((nofBallsInUsePrev > 0) && (nofBallsInUse == 0) && (pListener != nullptr))
			{
				pListener->allBallsGone();
			}
			nofBallsInUsePrev = nofBallsInUse;
		}

	private:
		void spawnBallsAt(int posX, int posY, int nofBalls)
		{
			int nofSpawned = 0;
			for (int i = 0; i < nofBalls; i++)
			{
				int indexBall = ballDepot.obtain();
				if (indexBall == BallDepot::InvalidIndex)
				{
					break;		// depot vol; het depot meldt dat zelf.
				}

				ballDepot.getItem(indexBall).spawnAt(posX, posY);
				nofSpawned++;
			}

#ifdef PONG_MEASURE
			ESP_LOGI("pong", "present crate open: %d ballen erbij, nu %d in het spel",
				nofSpawned, ballDepot.getNofInUse());
#else
			(void)nofSpawned;
#endif
		}

		void publishStates()
		{
			// Een slot dat niet (meer) in gebruik is levert een lege stand op, en
			// daar zet de display het panel op niet-tekenbaar - zo verdwijnt een
			// verbrande bal of een geopende crate van het scherm zonder dat daar
			// een apart bericht voor nodig is.
			for (int i = 0; i < MaxNofBalls; i++)
			{
				ballStates.arBall[i] = ballDepot.isInUse(i)
					? ballDepot.getItem(i).getBallState()
					: BallState();
			}

			for (int i = 0; i < MaxNofCrates; i++)
			{
				crateStates.arCrate[i] = crateDepot.isInUse(i)
					? crateDepot.getItem(i).getCrateState()
					: CrateState();
			}

			for (int i = 0; i < MaxNofFireBins; i++)
			{
				fireBinStates.arFireBin[i] = fireBinDepot.isInUse(i)
					? fireBinDepot.getItem(i).getFireBinState()
					: FireBinState();
			}

			ballStates.indexFrame++;
			gameDisplayControl.updateGameObjects(ballStates, crateStates, fireBinStates, score);
		}
	}; // end class GameObjectsControl

};// end namespace pong_cleangui_dg_adv
