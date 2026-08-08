// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

#include "pdga_IDepot.h"
#include "pdga_GameDisplayControl.h"

// UC08_FireBinHandling: een vuurkorf wordt door UC04 in het veld gezet, staat er
// ongeveer vijf seconden, en verdwijnt dan weer. Een bal die hem raakt vliegt in
// brand (UC05 stap 1d) - dat staat in BallControl, niet hier: de korf doet niets
// met ballen, hij staat er alleen.
//
// Anders dan de present crate telt deze wel af: hij houdt zijn eigen levensduur
// bij en geeft zichzelf terug aan het depot. En anders dan bij de crate is de cel
// van de spritesheet hier geen teller maar een animatiefase - het vuur brandt
// door zolang de korf er staat.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class FireBinControl
	{
	private:
		IDepot* pDepot;
		int indexSlot;

		int posX;			// het MIDDEN van de korf, in speelveldcoordinaten
		int posY;

		int32_t nofStepsLeft;		// wat er van de levensduur over is
		int32_t indexAnimationStep;

	public:
		// Default constructor, want Depot<FireBinControl,N> bouwt alle N
		// exemplaren in een keer.
		FireBinControl() :
			pDepot(nullptr), indexSlot(-1), posX(0), posY(0),
			nofStepsLeft(0), indexAnimationStep(0)
		{
		}

		void init(IDepot* pDepot, int indexSlot)
		{
			this->pDepot = pDepot;
			this->indexSlot = indexSlot;
		}

		void spawnAt(int posX, int posY)
		{
			this->posX = posX;
			this->posY = posY;
			nofStepsLeft = NofFireBinSteps;
			indexAnimationStep = 0;
		}

		void update()
		{
			indexAnimationStep++;

			nofStepsLeft--;
			if (nofStepsLeft <= 0)
			{
				// Uitgebrand. Zichzelf teruggeven mag hier, midden in de eigen
				// update(): een Depot perst zijn administratie niet samen, dus de
				// lus die ons aanroept raakt er niet van in de war. Het panel volgt
				// vanzelf - de display tekent alleen de slots die in gebruik zijn.
				assert(pDepot != nullptr);
				pDepot->release(indexSlot);
			}
		}

		int getPosX() const { return posX; }
		int getPosY() const { return posY; }

		// Vierkant tegen vierkant, net als bij de crate: de bal is rond, maar op
		// twaalf pixels is het verschil met zijn omhullende vierkant een pixel of
		// twee, en dat is bij een korf van achtentwintig niet te zien.
		bool isTouchedAtPos(int ballPosX, int ballPosY, int ballRadius) const
		{
			int halfSize = FireBinSizePix/2;
			int dx = (ballPosX > posX) ? (ballPosX - posX) : (posX - ballPosX);
			int dy = (ballPosY > posY) ? (ballPosY - posY) : (posY - ballPosY);
			return (dx < halfSize + ballRadius) && (dy < halfSize + ballRadius);
		}

		FireBinState getFireBinState() const
		{
			return FireBinState(posX, posY, indexAnimationStep);
		}
	}; // end class FireBinControl

};// end namespace pong_cleangui_dg_adv
