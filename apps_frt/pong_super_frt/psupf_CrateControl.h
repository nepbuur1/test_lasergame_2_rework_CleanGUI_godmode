// by Marius Versteegen, 2026

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>

#include "psupf_IDepot.h"
#include "psupf_IDepotItem.h"
#include "psupf_GameDisplayControl.h"
#include "psupf_IBallSpawner.h"

// Een present crate verlaagt een teller bij elke balaanraking (die begint op NofCrateHits), en
// levert bij nul een handvol nieuwe ballen op de plek waar hij stond.

using namespace crt;
namespace pong_super_frt
{
	class CrateControl : public IDepotItem
	{
	private:
		IDepot* pDepot;

		// Waar de ballen vandaan komen als deze crate opengaat. 
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
		// De functie moet er zijn omdat het Depot afleiding van IDepotItem vereist.
		/*override*/ void update()
		{
		}

		// Door Depot aangeroepen bij release() en releaseAll().
		/*override*/ void reset()
		{
			posX = 0;
			posY = 0;
			nofHitsLeft = 0;
		}

		int getPosX() const { return posX; }
		int getPosY() const { return posY; }

		// Raakt een bal op (ballPosX, ballPosY) met straal ballRadius dit kistje?
		bool isHitAtPos(int ballPosX, int ballPosY, int ballRadius) const
		{
			int halfSize = CrateSizePix/2;
			int dx = (ballPosX > posX) ? (ballPosX - posX) : (posX - ballPosX);
			int dy = (ballPosY > posY) ? (ballPosY - posY) : (posY - ballPosY);
			return (dx < halfSize + ballRadius) && (dy < halfSize + ballRadius);
		}

		// Verwerking van het bericht dat een bal dit kistje heeft geraakt: 
		// De teller omlaag, en bij nul gaat hij open: er spawnen NofBallsPerCrate 
		// ballen op deze plek, en daarna geeft hij zichzelf terug aan het depot.
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

};// end namespace pong_super_frt
