// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include "pdga_Depot.h"
#include <crt_IHandlerListener.h>

#include "pdga_GameDisplayControl.h"
#include "pdga_BallControl.h"

// Wat er per beeld met de ballen gebeurt: eerst zetten ze allemaal een stap, en
// daarna gaan hun standen in EEN bericht naar de display.
//
// Dit is het enige dat zich bij de Handler aanmeldt, en dat is met opzet. Het
// depot zelf is ook een IHandlerListener en had er dus rechtstreeks bij gekund,
// met een tweede listener erachter voor het publiceren - maar dan bepaalt de
// registratievolgorde bij de Handler of het beeld klopt. Dat is precies het
// soort afhankelijkheid dat stilzwijgend stukgaat zodra iemand een regel
// verplaatst. Hier staat de volgorde in de functie zelf.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	typedef Depot<BallControl, MaxNofBalls> BallDepot;

	class BallsControl : public IHandlerListener
	{
	private:
		BallDepot& ballDepot;
		IGameDisplayControl& gameDisplayControl;

		// Wordt elk beeld opnieuw opgebouwd en daarna weggeschreven. Als lid en
		// niet als lokale variabele: hij is ~800 bytes, en dat elk beeld op de
		// stack van de Handler-taak zetten is precies het soort ding waar een
		// stack-overflow uit voortkomt.
		BallStates ballStates;

	public:
		BallsControl(BallDepot& ballDepot, IGameDisplayControl& gameDisplayControl) :
			ballDepot(ballDepot), gameDisplayControl(gameDisplayControl)
		{
		}

		/*override*/ void update()
		{
			// Eerst alle actieve ballen een stap. Een bal die daarbij het scherm
			// verlaat geeft zichzelf terug aan het depot; dat mag midden in deze
			// lus, want het depot perst zijn administratie niet samen.
			ballDepot.update();

			// En dan pas publiceren. Een slot dat niet (meer) in gebruik is levert
			// een lege BallState op, en daar zet de display het panel op
			// niet-tekenbaar - zo verdwijnt de bal van het scherm zonder dat hij
			// daar zelf een bericht voor hoeft te sturen.
			for (int i = 0; i < MaxNofBalls; i++)
			{
				ballStates.arBall[i] = ballDepot.isInUse(i)
					? ballDepot.getItem(i).getBallState()
					: BallState();
			}

			ballStates.indexFrame++;
			gameDisplayControl.updateBallStates(ballStates);
		}
	}; // end class BallsControl

};// end namespace pong_cleangui_dg_adv
