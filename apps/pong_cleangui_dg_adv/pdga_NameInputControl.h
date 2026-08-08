// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <string.h>

#include "pdga_GameOverDisplayControl.h"

// UC03 stap 1: de speler om zijn naam vragen en pas terugkomen als hij hem heeft
// ingetypt.
//
// Dat zat verspreid over twee plekken die er geen van beide om hadden gevraagd. De
// volgorde - vragen, wachten, ophalen - stond middenin GameOverControl::doGameOver,
// en het herkennen van ENTER stond in de tekenlus van GameOverDisplayControl, die
// daarna een aparte listener nodig had om te vertellen wat ze net had gezien.
//
// Nu luistert dit object zelf naar het invoerveld, zoals GameControl zelf naar het
// GAME OVER-label luistert: de widget is van de DisplayControl, wat zijn
// gebeurtenissen betekenen is van degene die de logica heeft. De DisplayControl
// hoeft niet te weten dat ENTER iets afsluit, en er is geen tweede interface meer
// nodig om die wetenschap weer terug te sturen.
//
// Een control-object en geen taak, net als StartPageControl en GameOverControl:
// doQueryName() draait in de taak van de aanroeper en blokkeert die tot de naam er
// is. Het tekenen gebeurt intussen gewoon door in de taak van
// GameOverDisplayControl - die schuift het toetsenbord in beeld en werkt het veld
// bij bij elke aanslag.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class NameInputControl : public IInputfieldListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		IGameOverDisplayControl& gameOverDisplayControl;
		IInputfield* pNameInputfield;
		Flag flagNameEntered;

	public:
		NameInputControl(Task* pTask, IGameOverDisplayControl& gameOverDisplayControl) :
			pTask(pTask),
			gameOverDisplayControl(gameOverDisplayControl),
			pNameInputfield(nullptr),
			flagNameEntered(pTask)
		{
			pNameInputfield = gameOverDisplayControl.addNameInputfieldListener(this);
		}

		// Zie StartPageControl::doStartPage() en GameControl::doGame(): een deelstap
		// van de aanroeper, van begin tot eind, in diens eigen taak.
		//
		// De naam komt terug via strName en niet als returnwaarde: er is geen heap,
		// dus de buffer is van de aanroeper. nofCharsMax telt de afsluitende nul mee.
		void doQueryName(char* strName, int nofCharsMax)
		{
			// Wissen voor het tonen. Een melding van een vorige ronde is er niet -
			// die is toen afgewacht - maar dit is de plek waar dat gegarandeerd
			// wordt, en niet ergens verderop in de aanroeper.
			flagNameEntered.clear();

			gameOverDisplayControl.showNameInput();

			// UC03 stap 1b. Alles ertussen - het toetsenbord dat in beeld schuift,
			// het bijwerken van het veld bij elke aanslag - gebeurt in de taak van
			// de DisplayControl. Deze taak staat zolang stil.
			pTask->wait(flagNameEntered);

			// Pas hier uitlezen, en niet in inputNewKey(). Dat scheelt een pool: de
			// tekst staat al veilig in het veld, en Inputfield::getTextValue() zet
			// hem zelf via een pool over naar de vragende taak.
			crt::std::string<MaxNofNameFieldChars> strFieldContent;
			pNameInputfield->getTextValue(strFieldContent);

			strncpy(strName, strFieldContent.begin(), nofCharsMax - 1);
			strName[nofCharsMax - 1] = '\0';
		}

		// --- IInputfieldListener ---
		// Deze functies draaien in de taak die de aanraking opmerkt, niet in die van
		// de aanroeper van doQueryName(). Vandaar een Flag en niets anders.
		/* override */ void inputNewKey(IInputfield* pInputfield, const char* strKey)
		{
			if (strncmp(strKey, "ENTER", 5) == 0)
			{
				flagNameEntered.set();
			}
			else
			{
				// Elke andere toets verandert alleen het veld, en dat werkt de
				// DisplayControl zelf bij.
			}
		}

		/* override */ void inputfieldPressed(IInputfield* pInputfield)
		{
			// Het toetsenbord staat al omhoog zodra de naamfase begint; een tik op
			// het veld hoeft dus niets te doen.
		}

		/* override */ void outsideInputfieldPressed(IInputfield* pInputfield)
		{
			// Waarschijnlijk het toetsenbord zelf.
		}
	}; // end class NameInputControl

};// end namespace pong_cleangui_dg_adv
