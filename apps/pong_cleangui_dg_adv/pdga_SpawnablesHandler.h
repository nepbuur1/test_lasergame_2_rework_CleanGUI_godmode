// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_IHandlerListener.h>

#include "pdga_Spawnables.h"
#include "pdga_ISpawnablesHandlerListener.h"
#include "pdga_PaddleControl.h"

// De beeldklok van het spel: wat er per beeld met de spelobjecten gebeurt, en in
// welke volgorde.
//
// Dat het een eigen object is en niet een methode van Spawnables, is precies om die
// volgorde. Die is geen implementatiedetail - er zitten drie afspraken in die je aan
// de losse depots niet kunt zien - en zulke afspraken horen op een plek te staan waar
// ze te lezen zijn in plaats van verspreid over de objecten die eraan meedoen.
//
// Dit is ook het enige dat zich bij de Handler aanmeldt, en dat is met opzet. De
// depots zijn zelf ook IHandlerListener en hadden er dus rechtstreeks bij gekund, met
// nog een listener erachter voor het publiceren - maar dan bepaalt de
// registratievolgorde bij de Handler of het beeld klopt, en dat is het soort
// afhankelijkheid dat stilzwijgend stukgaat zodra iemand een regel verplaatst. Hier
// staat de volgorde in de functie zelf.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class SpawnablesHandler : public IHandlerListener
	{
	private:
		Spawnables& spawnables;
		PaddleControl& paddleControl;
		ISpawnablesHandlerListener* pListener;

		// Om de overgang naar nul te kunnen zien. Alleen "er zijn er nul" is niet
		// genoeg: voordat de eerste bal wordt gespawnd is dat ook zo, en dan zou
		// het spel meteen aflopen.
		int nofBallsInUsePrev;

	public:
		SpawnablesHandler(Spawnables& spawnables, PaddleControl& paddleControl) :
			spawnables(spawnables),
			paddleControl(paddleControl),
			pListener(nullptr),
			nofBallsInUsePrev(0)
		{
		}

		void setListener(ISpawnablesHandlerListener* pListener)
		{
			this->pListener = pListener;
		}

		/*override*/ void update()
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

			// De score komt van het spel en gaat in hetzelfde beeldbericht mee als
			// de standen; twee losse berichten zouden twee hertekeningen kunnen
			// opleveren.
			spawnables.publishStates((pListener != nullptr) ? pListener->getScore() : 0);

			// UC04 stap 4. De overgang telt, niet de stand: voor de eerste spawn
			// staat de teller ook op nul.
			int nofBallsInUse = spawnables.getNofBallsInUse();
			if ((nofBallsInUsePrev > 0) && (nofBallsInUse == 0) && (pListener != nullptr))
			{
				pListener->allBallsGone();
			}
			nofBallsInUsePrev = nofBallsInUse;
		}
	}; // end class SpawnablesHandler

};// end namespace pong_cleangui_dg_adv
