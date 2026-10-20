// by Marius Versteegen, 2023

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>

#include "psupf_HallOfFameDisplayControl.h"
#include "psupf_HallOfFame.h"
#include "psupf_NameInputControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_super_frt
{
	// UC03_GameOverHandling. De volgorde staat hier en niet in de DisplayControls:
	// die weten hoe hun pagina eruitziet, deze weet wat er moet gebeuren.
	//
	// Het zijn er twee geworden: de naaminvoer van stap 1 is een eigen pagina met
	// een eigen DisplayControl (zie de kop van psupf_NameInputDisplayControl.h).
	// Deze klasse kent alleen de hall of fame; de naampagina zit achter
	// NameInputControl, dat er als enige iets van hoeft te weten.
	class GameOverControl : public IButtonListener
	{

		// --- FreeRTOS (was CleanRTOS) ---
		static constexpr EventBits_t eventBitPlayAgainPressed = (1 << 0);
		EventGroupHandle_t eventGroup;
		EventBits_t firedBits;		// wat de laatste wachtoproep op meer tegelijk opleverde; zie hasFired()

	private:
		HallOfFameDisplayControl& hallOfFameDisplayControl;
		HallOfFame& hallOfFame;

		// De naaminvoer van stap 1 als een enkele aanroep. Ook samengevoegd met de
		// taak van pTask, dus doQueryName() draait in dezelfde taak als doGameOver.
		NameInputControl nameInputControl;

		IButton* pPlayAgainButton;

	public:
		GameOverControl(NameInputDisplayControl& nameInputDisplayControl,
						HallOfFameDisplayControl& hallOfFameDisplayControl, HallOfFame& hallOfFame) :
			eventGroup(xEventGroupCreate()),
			firedBits(0),
			hallOfFameDisplayControl(hallOfFameDisplayControl),
			hallOfFame(hallOfFame),
			nameInputControl(nameInputDisplayControl),
			pPlayAgainButton(nullptr)
		{
			pPlayAgainButton = hallOfFameDisplayControl.addPlayAgainButtonListener(this);
		}

		void buttonPressed(IButton* pButton)
		{
			ESP_LOGI("pong_super_frt", "%s", pButton->getButtonName());

			if(pButton == pPlayAgainButton)
			{
				xEventGroupSetBits(eventGroup, eventBitPlayAgainPressed);
			}
			else
			{
				;// not interested.
			}
		}

		// must override pure virtual function
		void buttonReleased(IButton* pButton)
		{
			// do nothing
		};

		// Zie GameControl::doGame(): tonen aan het begin, verbergen aan het eind.
		// Dat MainControl deze functies strikt na elkaar aanroept is wat
		// garandeert dat er nooit twee PageRoots tegelijk in beeld staan.
		//
		// Hier zijn het er twee achter elkaar, en die volgorde is dragend: de
		// naampagina staat in beeld en is weer weg voordat de hall of fame begint te
		// tekenen. Dat doQueryName() pas terugkeert als haar pagina echt weg is, is
		// waar dat op rust - zie psupf_INameInputPageListener.h.
		void doGameOver(int32_t score)
		{

			// UC03 stap 1: alleen als de score een plek verdient. Het spelscherm
			// blijft tot zolang staan - GameControl heeft zijn pagina al
			// niet-tekenbaar gemaakt, maar wist niets. Hier stond eerder een
			// showGameOverPage() die de hall of fame van de vórige ronde een moment
			// liet zien voordat er om een naam werd gevraagd; die flits is met de
			// aparte pagina's vanzelf verdwenen.
			if (hallOfFame.isQualifyingScore(score))
			{
				char strName[MaxNofNameChars + 1];
				// STATE_WAITING_FOR_USER_NAME
				nameInputControl.doQueryName(strName, sizeof(strName));

				hallOfFame.insert(strName, score);
				hallOfFame.save();		// UC03 stap 1c.

				ESP_LOGI("pong", "top-5: '%s' met %d punten opgeslagen", strName, (int)score);
			}

			// UC03 stap 2: de lijst, met de knop eronder.
			hallOfFameDisplayControl.showHallOfFamePage(hallOfFame.getHallOfFameData());
			xEventGroupClearBits(eventGroup, eventBitPlayAgainPressed);

			// STATE_WAITING_FOR_START
			firedBits = xEventGroupWaitBits(eventGroup, eventBitPlayAgainPressed, pdTRUE, pdFALSE, portMAX_DELAY);

			hallOfFameDisplayControl.hideHallOfFamePage();
		}
	private:
		// --- FreeRTOS-hulpfuncties ---
		// Zoals CleanRTOS' hasFired(): kijkt of het bit bij de laatste wachtoproep
		// gevuurd had, en wist alleen dat bit.
		bool hasFired(EventBits_t eventBit)
		{
			bool bFired = ((firedBits & eventBit) != 0);
			if (bFired)
			{
				xEventGroupClearBits(eventGroup, eventBit);
			}
			return bFired;
		}


	}; // end class GameOverControl

};// end namespace pong_super_frt
