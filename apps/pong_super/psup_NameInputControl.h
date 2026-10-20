// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <string.h>

#include "psup_NameInputDisplayControl.h"

// UC03 stap 1: de speler om zijn naam vragen en pas terugkomen als hij hem heeft
// ingetypt.
//
// Dat zat verspreid over twee plekken die er geen van beide om hadden gevraagd. De
// volgorde - vragen, wachten, ophalen - stond middenin GameOverControl::doGameOver,
// en het herkennen van ENTER stond in de tekenlus van de DisplayControl, die
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
// NameInputDisplayControl - die schuift het toetsenbord in beeld en werkt het
// veld bij bij elke aanslag.
//
// Dat het hele bezoek aan de naampagina - tonen, wachten, weer weg - binnen deze
// ene aanroep past, is wat GameOverControl ervan af houdt: die kent alleen de hall
// of fame.

using namespace crt;
namespace pong_super
{
	class NameInputControl : public IInputfieldListener, public INameInputPageListener
	{
	private:
		Task* pTask; // The task that this control object is merged to.
		NameInputDisplayControl& nameInputDisplayControl;
		IInputfield* pNameInputfield;
		Flag flagNameEntered;

		// Gezet in nameInputPageHidden() hieronder, zodra de pagina echt uit beeld
		// is.
		Flag flagPageHidden;

	public:
		NameInputControl(Task* pTask, NameInputDisplayControl& nameInputDisplayControl) :
			pTask(pTask),
			nameInputDisplayControl(nameInputDisplayControl),
			pNameInputfield(nullptr),
			flagNameEntered(pTask),
			flagPageHidden(pTask)
		{
			pNameInputfield = nameInputDisplayControl.addNameInputfieldListener(this);
			nameInputDisplayControl.setNameInputPageListener(this);
		}

		// Zie StartPageControl::doStartPage() en GameControl::doGame(): een deelstap
		// van de aanroeper, van begin tot eind, in diens eigen taak. En net als daar
		// staat de pagina bij terugkeer weer uit beeld, zodat de aanroeper meteen de
		// volgende mag laten tekenen.
		//
		// De naam komt terug via strName en niet als returnwaarde: er is geen heap,
		// dus de buffer is van de aanroeper. nofCharsMax telt de afsluitende nul mee.
		void doQueryName(char* strName, int nofCharsMax)
		{
			// Wissen voor het tonen. Een melding van een vorige ronde is er niet -
			// die is toen afgewacht - maar dit is de plek waar dat gegarandeerd
			// wordt, en niet ergens verderop in de aanroeper.
			flagNameEntered.clear();
			flagPageHidden.clear();

			nameInputDisplayControl.showNameInputPage();

			// UC03 stap 1b. Alles ertussen - het toetsenbord dat in beeld schuift,
			// het bijwerken van het veld bij elke aanslag - gebeurt in de taak van
			// de DisplayControl. Deze taak staat zolang stil.
			pTask->wait(flagNameEntered);

			// Pas hier uitlezen, en niet in inputNewKey(). Dat scheelt een pool: de
			// tekst staat al veilig in het veld, en Inputfield::getTextValue() zet
			// hem zelf via een pool over naar de vragende taak.
			crt::std::string<MaxNofNameFieldChars> strFieldContent;
			pNameInputfield->getTextValue(strFieldContent);
			copyStringToNullTerminatedBuffer(strFieldContent, strName, nofCharsMax);

			// En de pagina weer weg. Wachten tot dat gebeurd is en niet alleen
			// vragen: het toetsenbord schuift eerst naar beneden, en dat is nog een
			// paar honderd milliseconden tekenwerk op een pagina die tot dan toe in
			// beeld staat. Het einde daarvan komt binnen via nameInputPageHidden().
			nameInputDisplayControl.hideNameInputPage();
			pTask->wait(flagPageHidden);
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
			// Het toetsenbord staat al omhoog zodra de naampagina in beeld komt; een
			// tik op het veld hoeft dus niets te doen.
		}

		/* override */ void outsideInputfieldPressed(IInputfield* pInputfield)
		{
			// Waarschijnlijk het toetsenbord zelf.
		}

		// --- INameInputPageListener ---
		// Draait in de taak van de pagina, net als de functies hierboven in die van
		// de aanraking. En net als daar is een vlag zetten het enige dat hier hoort:
		// de taak die op de naam wacht, wacht op deze.
		/* override */ void nameInputPageHidden()
		{
			flagPageHidden.set();
		}

	private:
		// Zet str in buffer als C-string: hooguit bufferSize-1 tekens plus een
		// afsluitende nul. bufferSize telt die nul dus mee.
		//
		// Op lengte kopieren en niet met strncpy vanaf begin(): crt::std::string
		// is lengtegebaseerd en houdt GEEN afsluitende nul bij. operator= is
		// clear() += rhs (zie crt_string.h), en dat schrijft alleen length()
		// tekens - wat er in de bufferruimte daarachter staat blijft ongemoeid.
		// In doQueryName() is die ruimte stack, dus rommel, en strncpy leest daar
		// dwars doorheen tot hij toevallig een nulbyte tegenkomt. Dat is precies wat
		// er als vraagtekens achter de naam in de hall of fame terechtkwam.
		static void copyStringToNullTerminatedBuffer(const crt::std::string_base& str, char* buffer, int bufferSize)
		{
			int nofChars = (int)str.length();
			if (nofChars > (bufferSize - 1))
			{
				nofChars = bufferSize - 1;
			}
			memcpy(buffer, str.begin(), nofChars);
			buffer[nofChars] = '\0';
		}
	}; // end class NameInputControl

};// end namespace pong_super
