// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

#include "pdga_Depot.h"
#include "pdga_GameDisplayControl.h"
#include "pdga_BallControl.h"
#include "pdga_CrateControl.h"
#include "pdga_FireBinControl.h"
#include "pdga_IBallCollisionCheck.h"
#include "pdga_IBallSpawner.h"
#include "pdga_IBallListener.h"

// Alles wat er tijdens een spel in het veld kan staan, en wat het van elkaar moet
// weten.
//
// Dit object bezit de drie depots. Dat is het verschil met de situatie hiervoor,
// waarin ze los in de ino stonden en GameControl er soms rechtstreeks tegen praatte
// (ballDepot.obtain(), crateDepot.releaseAll(), de init-lussen) en soms via een
// tussenobject. Nu is er een deur: wie iets met een spelobject wil, komt hier langs.
//
// Wat er NIET in zit - dit is opslag met wat gemak eromheen, en niets meer:
// - De beeldklok. De volgorde waarin de depots per beeld worden bijgewerkt is een
//   zaak op zich; die staat in SpawnablesHandler.
// - De score. Die hoort bij het spel - zie GameControl. Een bal die op de peddel
//   kaatst meldt dat daar rechtstreeks, niet hierlangs (pdga_IBallListener.h).
// - Wat een treffer op een crate betekent. Dat staat in CrateControl::wasHit(); hier
//   wordt de mededeling alleen bij de juiste crate bezorgd.
//
// NB: door dit tussenobject kunnen alle ballen bij de peddelstand zonder ieder apart
// een Pool uit te lezen, en gaan hun standen in een keer als batch naar de
// GameDisplayControl.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	typedef Depot<BallControl, MaxNofBalls> BallDepot;
	typedef Depot<CrateControl, MaxNofCrates> CrateDepot;
	typedef Depot<FireBinControl, MaxNofFireBins> FireBinDepot;

	class Spawnables : public IBallCollisionCheck, public IBallSpawner
	{
	private:
		// Eigendom, geen verwijzingen. Ze staan in BSS zolang dit object een global
		// is - een Spawnables op een stack zou er meteen door omvallen.
		BallDepot    ballDepot;
		CrateDepot   crateDepot;
		FireBinDepot fireBinDepot;

		IGameDisplayControl& gameDisplayControl;

		// Worden elk beeld opnieuw gevuld en daarna weggeschreven. Als lid en niet
		// als lokale variabele: samen zijn ze bijna een kilobyte, en dat elk beeld
		// op de stack van de Handler-taak zetten is precies het soort ding waar een
		// stack-overflow uit voortkomt.
		BallStates ballStates;
		CrateStates crateStates;
		FireBinStates fireBinStates;

		// De peddelstand van het beeld dat nu berekend wordt. Een keer per beeld
		// gezet door SpawnablesHandler en daarna door alle ballen gelezen - zie de
		// toelichting bij IBallCollisionCheck::getPaddlePosition().
		int paddlePositionThisFrame;

	public:
		Spawnables(IGameDisplayControl& gameDisplayControl) :
			ballDepot("ballDepot"),
			crateDepot("crateDepot"),
			fireBinDepot("fireBinDepot"),
			gameDisplayControl(gameDisplayControl),
			paddlePositionThisFrame(0)
		{
			// Alle ballen, crates en vuurkorven bedraden, ook die voorlopig niets
			// doen. Ze bestaan al - het depot bouwt ze in een keer - en een object
			// dat pas bij het spawnen zijn verwijzingen zou krijgen, zou die
			// bedrading telkens opnieuw moeten doen.
			//
			// Hier en niet in GameControl: dit is de eigenaar van de depots, en
			// bedraden is eigenaarswerk. Het veld dat de ballen meekrijgen is dit
			// object zelf.
			for (int i = 0; i < MaxNofBalls; i++)
			{
				ballDepot.getItem(i).init(gameDisplayControl, this, &ballDepot, i);
			}

			// De crates krijgen dit object als IBallSpawner mee: een geopende crate
			// levert ballen op, en die liggen in het depot hier.
			for (int i = 0; i < MaxNofCrates; i++)
			{
				crateDepot.getItem(i).init(&crateDepot, this, i);
			}

			for (int i = 0; i < MaxNofFireBins; i++)
			{
				fireBinDepot.getItem(i).init(&fireBinDepot, i);
			}
		}

		// De ballen aan het spel knopen. Apart van de constructor, want GameControl
		// bestaat dan nog niet - dit object wordt er juist aan meegegeven.
		//
		// Bedrading van eigen exemplaren, en dus eigenaarswerk; het enige wat hier
		// gebeurt is doorgeven aan wie het aangaat.
		void setBallListener(IBallListener* pBallListener)
		{
			for (int i = 0; i < MaxNofBalls; i++)
			{
				ballDepot.getItem(i).setBallListener(pBallListener);
			}
		}

		// --- IBallCollisionCheck: wat een bal aan zijn omgeving vraagt ---

		// Een bal vraagt of hij ergens tegenaan zit. Zie IBallCollisionCheck.
		//
		// Zoekt en antwoordt, meer niet. Het slotnummer gaat mee zodat de
		// mededeling hieronder er niet nog een keer voor hoeft te zoeken.
		/*override*/ CrateHit getCrateHitAtPos(int ballPosX, int ballPosY, int ballRadius)
		{
			for (int i = 0; i < MaxNofCrates; i++)
			{
				if (!crateDepot.isInUse(i))
				{
					continue;
				}

				CrateControl& crate = crateDepot.getItem(i);
				if (!crate.isHitAtPos(ballPosX, ballPosY, ballRadius))
				{
					continue;
				}

				// Hooguit een crate per beeld; ze overlappen niet.
				return CrateHit(i, crate.getPosX(), crate.getPosY(), CrateSizePix);
			}

			return CrateHit();
		}

		// De bal kan de crate niet zelf bereiken - hij kent alleen dit veld - dus
		// gaat de mededeling hierlangs. Opzoeken en doorgeven, en verder niets: wat
		// een treffer betekent staat in CrateControl::wasHit().
		//
		// De bal heeft plek en maat al uit getCrateHitAtPos(); dat de crate hier kan
		// opengaan en zijn slot vrijgeven bederft zijn kaatsing dus niet.
		/*override*/ void crateWasHitAtPos(const CrateHit& hit)
		{
			if (!hit.bHit || !crateDepot.isInUse(hit.indexSlot))
			{
				return;
			}

			crateDepot.getItem(hit.indexSlot).wasHit();
		}

		// Staat de bal in een vuurkorf? Geen mededeling aan de korf: die doet niets
		// met ballen, hij staat er alleen (UC08). Wat er met de bal gebeurt staat
		// in BallControl.
		/*override*/ bool isFireBinTouchedAtPos(int ballPosX, int ballPosY, int ballRadius)
		{
			for (int i = 0; i < MaxNofFireBins; i++)
			{
				if (fireBinDepot.isInUse(i) &&
				    fireBinDepot.getItem(i).isTouchedAtPos(ballPosX, ballPosY, ballRadius))
				{
					return true;
				}
			}
			return false;
		}

		// Waar de ballen hun peddelstand vandaan halen; zie
		// IBallCollisionCheck::getPaddlePosition() voor waarom het zo loopt en niet elke bal
		// het zelf bij PaddleControl ophaalt.
		/*override*/ int getPaddlePosition()
		{
			return paddlePositionThisFrame;
		}

		// --- in het veld zetten ---

		// De eerste bal van een spel. Een leeg depot is geen fout maar een uitkomst
		// - het depot meldt het zelf - vandaar de bool.
		bool spawnBallInTheMiddle()
		{
			int indexBall = ballDepot.obtain();
			if (indexBall == BallDepot::InvalidIndex)
			{
				return false;
			}

			ballDepot.getItem(indexBall).spawnInTheMiddle();
			return true;
		}

		// Een crate in het veld zetten. Aangeroepen vanuit GameControl (UC04), dus
		// vanuit een andere taak dan waarin de beeldklok draait. Het gaat om het
		// vullen van een slot dat nog van niemand was, dus daar zit geen race in die
		// kwaad kan.
		bool spawnCrateAt(int posX, int posY)
		{
			int indexCrate = crateDepot.obtain();
			if (indexCrate == CrateDepot::InvalidIndex)
			{
				return false;
			}

			crateDepot.getItem(indexCrate).spawnAt(posX, posY);
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

			fireBinDepot.getItem(indexFireBin).spawnAt(posX, posY);
			return true;
		}

		// Het veld leegmaken aan het eind van een spel. Crates en vuurkorven
		// verdwijnen alleen op hun eigen voorwaarden - een crate als hij opengaat,
		// een korf als hij uitbrandt - dus een crate die niet is opengespeeld blijft
		// in zijn slot zitten. Zonder dit stonden de kistjes van het vorige spel er
		// bij het volgende gewoon weer, want het depot was ze nooit kwijtgeraakt.
		//
		// De ballen hoeven niet: die zijn op, dat is juist waarom het spel voorbij
		// is.
		void releaseCratesAndFireBins()
		{
			crateDepot.releaseAll();
			fireBinDepot.releaseAll();
		}

		// --- tellingen ---

		int getNofBallsInUse()    { return ballDepot.getNofInUse(); }
		int getNofCratesInUse()   { return crateDepot.getNofInUse(); }
		int getNofFireBinsInUse() { return fireBinDepot.getNofInUse(); }

		// --- per beeld; de volgorde staat in SpawnablesHandler ---

		void setPaddlePosition(int paddlePosition)
		{
			paddlePositionThisFrame = paddlePosition;
		}

		void updateCratesAndFireBins()
		{
			crateDepot.update();
			fireBinDepot.update();
		}

		void updateBalls()
		{
			ballDepot.update();
		}

		// Alle standen in EEN bericht naar de display.
		void publishStates(int32_t score)
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

		// --- IBallSpawner: waar een geopende crate zijn ballen vandaan haalt ---
		/*override*/ void spawnBallsAt(int posX, int posY, int nofBalls)
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
	}; // end class Spawnables

};// end namespace pong_cleangui_dg_adv
