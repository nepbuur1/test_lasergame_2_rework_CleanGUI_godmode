// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

#include "pdga_IDepot.h"
#include "pdga_GameDisplayControl.h"
#include "pdga_IBallSpawner.h"

// UC07_PresentHandling: een present crate wordt door UC04 in het veld gezet,
// verliest een teller bij elke balaanraking (die begint op NofCrateHits), en
// levert bij nul een handvol nieuwe ballen op de plek waar hij stond.
//
// Net als een bal is dit geen taak maar een object in een Depot; zie
// pdga_Depot.h voor waarom.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class CrateControl
	{
	private:
		IDepot* pDepot;

		// Waar de ballen vandaan komen als deze crate opengaat. Via een interface en
		// niet via het depot zelf: een crate hoort niet te weten dat er zoiets als
		// een baldepot bestaat. Zie pdga_IBallSpawner.h.
		IBallSpawner* pBallSpawner;

		int indexSlot;

		int posX;			// het MIDDEN van de crate, in speelveldcoordinaten
		int posY;
		int nofHitsLeft;

	public:
		// Default constructor, want Depot<CrateControl,N> bouwt alle N
		// exemplaren in een keer.
		CrateControl() :
			pDepot(nullptr), pBallSpawner(nullptr), indexSlot(-1),
			posX(0), posY(0), nofHitsLeft(0)
		{
		}

		void init(IDepot* pDepot, IBallSpawner* pBallSpawner, int indexSlot)
		{
			this->pDepot = pDepot;
			this->pBallSpawner = pBallSpawner;
			this->indexSlot = indexSlot;
		}

		void spawnAt(int posX, int posY)
		{
			this->posX = posX;
			this->posY = posY;
			nofHitsLeft = NofCrateHits;
		}

		// Een crate doet uit zichzelf niets - hij staat stil en wacht op ballen.
		// De functie moet er zijn omdat het Depot hem per ronde aanroept, en dat
		// is precies de plek waar later een animatie of een aftellende klok komt
		// te staan.
		void update()
		{
		}

		int getPosX() const { return posX; }
		int getPosY() const { return posY; }

		// Raakt een bal op (ballPosX, ballPosY) met straal ballRadius dit kistje?
		// Vierkant tegen vierkant: de bal is rond, maar op twaalf pixels is het
		// verschil met zijn omhullende vierkant een pixel of twee, en dat is bij
		// een kistje van achtentwintig niet te zien.
		bool isHitAtPos(int ballPosX, int ballPosY, int ballRadius) const
		{
			int halfSize = CrateSizePix/2;
			int dx = (ballPosX > posX) ? (ballPosX - posX) : (posX - ballPosX);
			int dy = (ballPosY > posY) ? (ballPosY - posY) : (posY - ballPosY);
			return (dx < halfSize + ballRadius) && (dy < halfSize + ballRadius);
		}

		// Een bal heeft dit kistje geraakt (UC05 stap 1c). De teller omlaag, en bij
		// nul gaat hij open: NofBallsPerCrate ballen op deze plek, en daarna geeft
		// hij zichzelf terug aan het depot (UC07 stap 3).
		//
		// Dat dit hier staat en niet bij wie de crates bewaart, is precies de reden
		// dat een crate zijn teller en zijn plek zelf heeft. Zichzelf vrijgeven is
		// hetzelfde patroon als bij een bal die het scherm verlaat - zie
		// BallControl.
		//
		// Dit draait binnen de update() van de bal die het meldt, en dus binnen de
		// lus van het baldepot. Dat mag: een Depot perst zijn administratie niet
		// samen, dus bijkomende ballen en dit vrijgegeven slot brengen die lus niet
		// in de war.
		void wasHit()
		{
			if (nofHitsLeft > 0)
			{
				nofHitsLeft--;
			}

			if (!isOpened())
			{
				return;
			}

			if (pBallSpawner != nullptr)
			{
				pBallSpawner->spawnBallsAt(posX, posY, NofBallsPerCrate);
			}

			assert(pDepot != nullptr);
			pDepot->release(indexSlot);
		}

		bool isOpened() const
		{
			return (nofHitsLeft <= 0);
		}

		CrateState getCrateState() const
		{
			return CrateState(posX, posY, nofHitsLeft);
		}
	}; // end class CrateControl

};// end namespace pong_cleangui_dg_adv
