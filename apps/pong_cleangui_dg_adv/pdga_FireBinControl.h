// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

#include "pdga_IDepot.h"
#include "pdga_IDepotItem.h"
#include "pdga_GameDisplayControl.h"

// FireBinHandling: een vuurkorf wordt door in het veld gezet, staat er
// ongeveer vijf seconden, en verdwijnt dan weer. Een bal die hem raakt vliegt in
// brand - dat staat in BallControl, niet hier: de korf doet niets
// met ballen, hij staat er alleen.
//
// Anders dan de present crate telt de fire bin wel automatisch af: hij houdt zijn eigen levensduur
// bij en geeft zichzelf terug aan het depot. En anders dan bij de crate is de cel
// van de spritesheet hier geen teller maar een animatiefase - het vuur brandt
// door zolang de korf er staat.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class FireBinControl : public IDepotItem
	{
		enum State
		{
			// Het slot is niet actief in het spel. Normaal betekent dit ook dat
			// het Depot het niet uitgegeven heeft en update() dus niet aanroept.
			STATE_IN_DEPOT,

			// De korf staat in het veld, animeert en telt zijn levensduur af.
			STATE_BURNING
		};

	private:
		IDepot* pDepot;
		int indexSlot;

		int posX;			// het MIDDEN van de korf, in speelveldcoordinaten
		int posY;

		int32_t nofStepsLeft;		// wat er van de levensduur over is
		int32_t indexAnimationStep;
		State state;

		// Het inkomende spawn-bericht en zijn payload. Net als BallControl
		// verandert spawnAt() de actieve toestand niet zelf: de eerstvolgende
		// update() neemt het bericht aan en begint een nieuw leven van de korf.
		//
		// Een gewone bool volstaat, want Spawnables en deze update() draaien in
		// dezelfde taak. Er is dus geen CleanRTOS Flag of Queue nodig.
		bool bFlagSpawnAt;
		int spawnAtPosX;
		int spawnAtPosY;

	public:
		// Default constructor, want Depot<FireBinControl,N> bouwt alle N
		// exemplaren in een keer.
		FireBinControl() :
			pDepot(nullptr), indexSlot(-1), posX(0), posY(0),
			nofStepsLeft(0), indexAnimationStep(0), state(STATE_IN_DEPOT),
			bFlagSpawnAt(false), spawnAtPosX(0), spawnAtPosY(0)
		{
		}

		void init(IDepot* pDepot, int indexSlot)
		{
			this->pDepot = pDepot;
			this->indexSlot = indexSlot;
		}

		void spawnAt(int posX, int posY)
		{
			spawnAtPosX = posX;
			spawnAtPosY = posY;
			bFlagSpawnAt = true;
		}

		/*override*/ void update()
		{
			switch (state)
			{
			case STATE_IN_DEPOT:
				if (bFlagSpawnAt)
				{
					bFlagSpawnAt = false;
					startAt(spawnAtPosX, spawnAtPosY);
					state = STATE_BURNING;
				}
				break;

			case STATE_BURNING:
				indexAnimationStep++;
				nofStepsLeft--;
				if (nofStepsLeft <= 0)
				{
					// Uitgebrand. Tijd om zichzelf op te heffen.
					assert(pDepot != nullptr);
					pDepot->release(indexSlot);
					state = STATE_IN_DEPOT;
				}
				break;

			default:
				break;
			}
		}

		// Door Depot aangeroepen bij release() en releaseAll(). Alleen de
		// runtime-toestand wordt gewist.
		/*override*/ void reset()
		{
			posX = 0;
			posY = 0;
			nofStepsLeft = 0;
			indexAnimationStep = 0;
			bFlagSpawnAt = false;
			spawnAtPosX = 0;
			spawnAtPosY = 0;

			state = STATE_IN_DEPOT; // Normaal gesproken geen toestandswijziging door
			                        // publieke interface functies.
									// Dit is een uitzondering, omdat het object
									// als het ware geherinitialiseerd wordt,
									// bijvoorbeeld via releaseAll() van het Depot.
									// (en update niet meer wordt aangeroepen totdat
									//  het object opnieuww geinitialisserd is)
		}

		int getPosX() const { return posX; }
		int getPosY() const { return posY; }

		// Vierkant tegen vierkant, net als bij de crate: de bal is rond, maar op
		// twaalf pixels is het verschil met zijn omhullende vierkant een pixel of
		// twee, en dat is bij een korf van achtentwintig niet te zien.
		bool isTouchedAtPos(int ballPosX, int ballPosY, int ballRadius) const
		{
			if (state != STATE_BURNING)
			{ 
				// De firebin zit nog ongebruikt in depot.
				return false;
			}

			int halfSize = FireBinSizePix/2;
			int dx = (ballPosX > posX) ? (ballPosX - posX) : (posX - ballPosX);
			int dy = (ballPosY > posY) ? (ballPosY - posY) : (posY - ballPosY);
			return (dx < halfSize + ballRadius) && (dy < halfSize + ballRadius);
		}

		FireBinState getFireBinState() const
		{
			if (state != STATE_BURNING)
			{ 
				// De firebin zit nog ongebruikt in depot.
				return FireBinState();
			}

			return FireBinState(posX, posY, indexAnimationStep);
		}

	private:
		// Het werk achter het spawn-bericht. Alleen update() roept dit aan. De
		// omgekeerde overgang naar STATE_IN_DEPOT loopt via Depot::release(), dat
		// reset() op dit item aanroept.
		void startAt(int posX, int posY)
		{
			this->posX = posX;
			this->posY = posY;
			nofStepsLeft = NofFireBinSteps;
			indexAnimationStep = 0;
		}
	}; // end class FireBinControl

};// end namespace pong_cleangui_dg_adv
