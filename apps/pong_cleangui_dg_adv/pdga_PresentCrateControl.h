// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

#include "pdga_IDepot.h"
#include "pdga_GameDisplayControl.h"
#include "pdga_IBallField.h"

// UC07_PresentHandling: een present crate wordt door UC04 in het veld gezet,
// verliest een teller bij elke balaanraking (die begint op NofCrateHits), en
// levert bij nul een handvol nieuwe ballen op de plek waar hij stond.
//
// Net als een bal is dit geen taak maar een object in een Depot; zie
// pdga_Depot.h voor waarom.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class PresentCrateControl
	{
	private:
		IDepot* pDepot;
		int indexSlot;

		int posX;			// het MIDDEN van de crate, in speelveldcoordinaten
		int posY;
		int nofHitsLeft;

	public:
		// Default constructor, want Depot<PresentCrateControl,N> bouwt alle N
		// exemplaren in een keer.
		PresentCrateControl() :
			pDepot(nullptr), indexSlot(-1), posX(0), posY(0), nofHitsLeft(0)
		{
		}

		void init(IDepot* pDepot, int indexSlot)
		{
			this->pDepot = pDepot;
			this->indexSlot = indexSlot;
		}

		void spawn(int posX, int posY)
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
		bool overlaps(int ballPosX, int ballPosY, int ballRadius) const
		{
			int halfSize = CrateSizePix/2;
			int dx = (ballPosX > posX) ? (ballPosX - posX) : (posX - ballPosX);
			int dy = (ballPosY > posY) ? (ballPosY - posY) : (posY - ballPosY);
			return (dx < halfSize + ballRadius) && (dy < halfSize + ballRadius);
		}

		void registerHit()
		{
			if (nofHitsLeft > 0)
			{
				nofHitsLeft--;
			}
		}

		bool isOpened() const
		{
			return (nofHitsLeft <= 0);
		}

		CrateState getCrateState() const
		{
			return CrateState(posX, posY, nofHitsLeft);
		}
	}; // end class PresentCrateControl

};// end namespace pong_cleangui_dg_adv
