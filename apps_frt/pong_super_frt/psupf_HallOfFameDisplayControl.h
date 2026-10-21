// by Marius Versteegen, 2026

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <crt_CleanGUI.h>
#include <stdio.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include "images/_pal8/_allImages.h"
#include "psupf_TorchesDisplayHelpers.h"
#include "psupf_HallOfFame.h"

// De DisplayControl van de hall-of-fame-pagina (UC03 stap 2), met een eigen
// PageRoot en een eigen taak. Zie psupf_GameDisplayControl.h voor waarom het
// verbergen niets tekent.
//
// De pagina toont de top-5 met daaronder de knop "Play Again", en fakkels ernaast
// zoals op de startpagina.
//
// De naaminvoer van UC03 stap 1 zat hier eerder als tweede fase bij in - zelfde
// taak, zelfde PageRoot, met makeDrawable/makeNonDrawable om te wisselen. Die
// woont nu in NameInputDisplayControl; zie de kop van dat bestand voor wat het
// delen kostte. Wat hier is overgebleven is een pagina die maar een ding toont,
// en dus geen fasen meer kent.
//
// Het GAME OVER-vignet hoort niet op deze pagina: dat toont GameControl over het
// spelscherm heen, zoals de uitzondering in UC04 zegt.

using namespace crt;
namespace pong_super_frt
{
	// Zie psupf_BallControl.h: logger komt uit namespace crt, niet uit deze.

	typedef TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> PlayAgainButton;

	// Waar de "Play Again"-knop staat: onder de hall of fame. Een eigen hoogte en
	// niet die van de startpagina, want deze pagina heeft er meer boven staan.
	const int PlayAgainButtonTopPix = 176;

	// De verticale indeling van de hall of fame, uitgerekend in plaats van
	// opgeschreven: de titel bovenaan, de knop onderaan, en de lijst precies
	// midden in wat daartussen overblijft. Verschuift de knop of wordt de titel
	// een andere maat, dan schuift de lijst vanzelf mee.
	const int HallOfFameTitleTopPix   = 10;
	const int HallOfFameLineHeightPix = 18;

	const int HallOfFameListHeightPix = NofHallOfFameEntries * HallOfFameLineHeightPix;
	const int HallOfFameSpaceTopPix   = HallOfFameTitleTopPix + imageTitleHallOfFame.height;
	const int HallOfFameTopPix        = HallOfFameSpaceTopPix +
		(PlayAgainButtonTopPix - HallOfFameSpaceTopPix - HallOfFameListHeightPix)/2;

	// Wat GameOverControl van deze pagina mag vragen, staat in het publieke blok na
	// de constructor. Zie de toelichting boven
	// psupf_GameDisplayControl.h::GameDisplayControl voor waarom daar geen
	// interface meer boven staat.
	class HallOfFameDisplayControl
	{

		// --- FreeRTOS (was CleanRTOS) ---
		static constexpr EventBits_t eventBitShowHallOfFamePage = (1 << 0);
		static constexpr EventBits_t eventBitHideHallOfFamePage = (1 << 1);
		static constexpr EventBits_t eventBitTimerTorch = (1 << 2);
		EventGroupHandle_t eventGroup;
		EventBits_t firedBits;		// wat de laatste wachtoproep op meer tegelijk opleverde; zie hasFired()
		TaskHandle_t taskHandle;

		enum State
		{
			STATE_HIDDEN,		// pagina uit beeld
			STATE_SHOWN			// in beeld: de lijst met de knop eronder; de fakkels branden
		};

		// De naam met zijn plaatsnummer ("1  marius") en de score staan in twee
		// aparte kolommen, elk met een eigen Text.
		//
		// Een regel met spaties ertussen werkt niet: font 2 is proportioneel, dus
		// een naam van tien smalle letters is smaller dan een van tien brede, en
		// dan zwabbert de scorekolom. Twee Texts, waarvan de ene links begint en de
		// andere rechts eindigt, staan wel recht onder elkaar.
		static const int MaxNofNameLineChars  = MaxNofNameChars + 6;
		static const int MaxNofScoreLineChars = 8;
		static const int HallOfFameFont       = 2;

		// Wat er tussen de twee kolommen zit.
		static const int HallOfFameGapPix = 20;

		// Achtergrond, titel, de twee kolommen, de knop en twee fakkels; de rest is
		// lucht voor wie er een regel bij zet.
		static const size_t MaxNofPageChildren = 8 + 2*NofHallOfFameEntries;
		using PAGE = PageRoot<MaxNofPageChildren>;

	private:
		IDisplay& display;
		TFT_eSPI_IFreeFonts& freeFonts;

		int screenWidth;
		int screenHeight;

		// Schermvullend, net als bij de andere pagina's. Daar leunt de paginawissel
		// op: wie in beeld komt wist wat de vorige achterliet.
		Panel<0> backgroundPanel;

		// "HALL OF FAME" boven de lijst.
		Panel<0> hallOfFameTitlePanel;

		Text<0> arHallOfFameNames[NofHallOfFameEntries];
		Text<0> arHallOfFameScores[NofHallOfFameEntries];
		char arNameText[NofHallOfFameEntries][MaxNofNameLineChars + 1];
		char arScoreText[NofHallOfFameEntries][MaxNofScoreLineChars + 1];

		PlayAgainButton buttonPlayAgain;

		// Fakkels naast de "Play Again"-knop, net als op de startpagina.
		Panel<0> arTorchPanels[2];
		int32_t arIndexTorchFrame[2];

		TimerHandle_t timerTorch;

		// De lijst die getoond moet worden. Via een Pool: showHallOfFamePage() wordt
		// vanuit GameOverControl aangeroepen en de opbouw gebeurt hier.
		QueueHandle_t hallOfFameQueue;		// lengte 1: was Pool<HallOfFameData> poolHallOfFame

		// De lijst zoals hij het laatst uit de pool is gelezen. Een member en geen
		// lokale variabele: hij wordt in de case gelezen (zichtbaar in de STD) en dan
		// aan fillHallOfFameLines() gegeven.
		HallOfFameData snapshot{};

		State state;

	public:
		HallOfFameDisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber,
							IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts) :
			eventGroup(xEventGroupCreate()),
			firedBits(0),
			taskHandle(nullptr),
			display(display),
			freeFonts(freeFonts),
			screenWidth(0),
			screenHeight(0),
			arIndexTorchFrame{0, 5},
			timerTorch(xTimerCreate("timerTorch", 1, pdFALSE, this, timerTorchCallback)),
			state (STATE_HIDDEN)
		{
			hallOfFameQueue = xQueueCreate(1, sizeof(HallOfFameData));
			{ HallOfFameData initValue{}; xQueueOverwrite(hallOfFameQueue, &initValue); }
			Vec2 screenSize = display.getScreenSize();
			screenWidth  = screenSize.x;
			screenHeight = screenSize.y;

			display.addTouchListener(&buttonPlayAgain);

			xTaskCreatePinnedToCore(staticMain, taskName, taskSizeBytes, this, taskPriority, &taskHandle, taskCoreNumber);
		}

	IButton* addPlayAgainButtonListener(IButtonListener* pButtonListener)
	{
		buttonPlayAgain.addButtonListener(pButtonListener);
		return &buttonPlayAgain;
	}

	// De volgende twee wissen elkaars vlag: alleen de laatste wens telt.
	//
	// De lijst gaat mee met het tonen en niet in een aparte setter: er is geen
	// moment waarop deze pagina in beeld hoort te staan zonder dat er een lijst
	// bij hoort.
	void showHallOfFamePage(const HallOfFameData& hallOfFameData)
	{
		{ HallOfFameData poolValue = hallOfFameData; xQueueOverwrite(hallOfFameQueue, &poolValue); }
		xEventGroupClearBits(eventGroup, eventBitHideHallOfFamePage);
		xEventGroupSetBits(eventGroup, eventBitShowHallOfFamePage);
	}

	void hideHallOfFamePage()
	{
		xEventGroupClearBits(eventGroup, eventBitShowHallOfFamePage);
		xEventGroupSetBits(eventGroup, eventBitHideHallOfFamePage);
	}

	private:
		void initPanels()
		{
			backgroundPanel.setPanelProps("hallOfFameBackground", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(screenWidth, screenHeight), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00000000, /*colBg*/0x00000000);

			Vec2 titleSizePix(imageTitleHallOfFame.width, imageTitleHallOfFame.height);
			hallOfFameTitlePanel.setPanelProps
				("hallOfFameTitle",/*locPos*/Vec2((screenWidth - titleSizePix.x)/2, HallOfFameTitleTopPix),
				CoordType::Pixels,
				/*size*/titleSizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid,	/*colFg*/0x0000FFFF, /*colBg*/0x00000000);
			hallOfFameTitlePanel.setImage(&imageTitleHallOfFame);

			// De plekken worden pas in fillHallOfFameLines() bepaald: daar zijn de
			// tekstbreedtes bekend, en daarmee waar het blok gecentreerd moet staan.
			for (int i = 0; i < NofHallOfFameEntries; i++)
			{
				arNameText[i][0] = '\0';
				arScoreText[i][0] = '\0';

				int32_t yPix = HallOfFameTopPix + i*HallOfFameLineHeightPix;

				arHallOfFameNames[i].setTextProps
					(/*locPos*/Vec2(0, yPix), CoordType::Pixels, Alignment::TopLeft,
					arNameText[i], HallOfFameFont, /*color*/0x0000FFFF, /*fontScale*/1);

				arHallOfFameScores[i].setTextProps
					(/*locPos*/Vec2(0, yPix), CoordType::Pixels, Alignment::TopLeft,
					arScoreText[i], HallOfFameFont, /*color*/0x0000FFFF, /*fontScale*/1);
			}

			initPlayAgainButton();
			TorchesDisplayHelpers::initTorches(arTorchPanels, screenWidth, PlayAgainButtonTopPix, imageButtonPlayAgain.height);
		}

		// Dezelfde knop als op de startpagina, maar met "PLAY AGAIN" erop en lager
		// op het scherm - UC03 stap 2a. Een eigen plaatje omdat het woord langer
		// is; de letters blijven even groot als op de startpagina.
		void initPlayAgainButton()
		{
			// Alles in pixels en niet in promillage. Bij een knop die uit een plaatje
			// bestaat is dat geen smaak: promillage wordt naar hele pixels afgerond,
			// en een panel dat daardoor een pixel groter uitvalt dan het plaatje laat
			// Panel::needsBackground() de vlakke colPanel-kleur er alsnog onder
			// schilderen. Die rand van een pixel is precies zichtbaar genoeg om lelijk
			// te zijn.
			Vec2 sizePix(imageButtonPlayAgain.width, imageButtonPlayAgain.height);

			// Het label is leeg omdat de tekst in het plaatje zit - met de schuine
			// kant en het verloop eromheen, die geen enkele fillRect kan maken.
			// colPanel en colFont blijven wel ingevuld: het plaatje is transparant
			// rond zijn afgeronde hoeken, dus zodra iemand de setImage() hieronder
			// weghaalt is er weer een gewone rode knop met tekst.
			buttonPlayAgain.setTouchScreenButtonProps
				("playAgain",/*locPos*/Vec2((screenWidth - sizePix.x)/2, PlayAgainButtonTopPix), CoordType::Pixels,
				/*size*/sizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF,
				/*fontScale*/ 1, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);

			// fontScale 1 en niet 0: bij 0 zoekt TouchscreenButton de grootste schaal
			// die past, en dat is werk zonder betekenis voor een leeg label.
			buttonPlayAgain.setImage(&imageButtonPlayAgain);
		}

		// Rekent alleen met de lijst die hij krijgt; het lezen van de pool staat in de case.
		void fillHallOfFameLines(const HallOfFameData& hallOfFameData)
		{
			// Eerst de teksten, en meteen meten hoe breed de twee kolommen worden.
			int32_t maxNameWidthPix = 0;
			int32_t maxScoreWidthPix = 0;

			for (int i = 0; i < NofHallOfFameEntries; i++)
			{
				const HallOfFameEntry& entry = hallOfFameData.arEntry[i];
				bool bEmpty = (entry.strName[0] == '\0');

				snprintf(arNameText[i], sizeof(arNameText[i]), "%d  %s",
					i + 1, bEmpty ? "-" : entry.strName);
				snprintf(arScoreText[i], sizeof(arScoreText[i]), "%s",
					bEmpty ? "-" : "");
				if (!bEmpty)
				{
					snprintf(arScoreText[i], sizeof(arScoreText[i]), "%d", (int)entry.score);
				}

				int32_t nameWidthPix = display.getTextWidth(arNameText[i], HallOfFameFont);
				if (nameWidthPix > maxNameWidthPix) { maxNameWidthPix = nameWidthPix; }

				int32_t scoreWidthPix = display.getTextWidth(arScoreText[i], HallOfFameFont);
				if (scoreWidthPix > maxScoreWidthPix) { maxScoreWidthPix = scoreWidthPix; }
			}

			// Het blok als geheel in het midden: de namen beginnen allemaal op
			// dezelfde x, de scores eindigen allemaal op dezelfde x, en die twee
			// kolommen samen staan gecentreerd. De breedte volgt uit de langste
			// naam, dus hij verschuift mee met wat er in de lijst staat.
			int32_t blockWidthPix = maxNameWidthPix + HallOfFameGapPix + maxScoreWidthPix;
			int32_t blockLeftPix  = (screenWidth - blockWidthPix)/2;
			if (blockLeftPix < 0) { blockLeftPix = 0; }
			int32_t scoreRightPix = blockLeftPix + blockWidthPix;

			for (int i = 0; i < NofHallOfFameEntries; i++)
			{
				int32_t yPix = HallOfFameTopPix + i*HallOfFameLineHeightPix;

				arHallOfFameNames[i].setText(arNameText[i]);
				arHallOfFameNames[i].setLocPos(Vec2(blockLeftPix, yPix), CoordType::Pixels, Alignment::TopLeft);

				// Rechts uitlijnen door per regel zijn eigen breedte af te trekken.
				// Dat is nauwkeuriger dan Alignment::TopRight, dat op de rand van de
				// ouder uitlijnt en niet op de kolom.
				int32_t scoreWidthPix = display.getTextWidth(arScoreText[i], HallOfFameFont);
				arHallOfFameScores[i].setText(arScoreText[i]);
				arHallOfFameScores[i].setLocPos(Vec2(scoreRightPix - scoreWidthPix, yPix),
												CoordType::Pixels, Alignment::TopLeft);
			}
		}

		// Zie de toelichting bij GameDisplayControl::showPage/hidePage - dit is
		// dezelfde afspraak, en die moet voor alle pagina's gelden.
		void showPage(PAGE& page)
		{
			page.makeDrawable(true);
			page.enable(true);
			page.redraw(true);
		}

		void hidePage(PAGE& page)
		{
			// Zonder ook maar een tekencommando, want een andere pagina schrijft al
			// in dezelfde commandobuffer (zie de kop van dit bestand). Daarom geen
			// hide(): die zou vanaf de wortel over dit vlak heen gaan.
			// De disable staat er los bij; niet-tekenbaar maakt niet doof.
			page.makeNonDrawable(true);
			page.disable(true);
		}

		void enterHidden(PAGE& page)
		{
			// stop() laat een tik die al gevallen is gewoon staan - daar is clear()
			// voor (zie crt_Timer.h). Buiten beeld wacht niemand meer op de klok, dus
			// zonder deze regel valt die tik pas binnen als de pagina terugkomt, met
			// een fakkelstap en een volledige hertekening op het moment van
			// verschijnen.
			xTimerStop(timerTorch, 0);
			xEventGroupClearBits(eventGroup, eventBitTimerTorch);

			hidePage(page);
		}

		void defineDisplayHierarchy(IWidget& pageRoot)
		{
			pageRoot.addChildWidget(backgroundPanel);		// eerst, want alles hierna tekent erop.
			pageRoot.addChildWidget(hallOfFameTitlePanel);

			for (int i = 0; i < NofHallOfFameEntries; i++)
			{
				pageRoot.addChildWidget(arHallOfFameNames[i]);
				pageRoot.addChildWidget(arHallOfFameScores[i]);
			}

			pageRoot.addChildWidget(buttonPlayAgain);
			pageRoot.addChildWidget(arTorchPanels[0]);
			pageRoot.addChildWidget(arTorchPanels[1]);
		}

		void selectNextAnimationFrameOfTorches()
		{
			TorchesDisplayHelpers::stepTorches(arTorchPanels, arIndexTorchFrame);
		}


		// --- FreeRTOS-hulpfuncties ---
		static void staticMain(void* pvParameters)
		{
			static_cast<HallOfFameDisplayControl*>(pvParameters)->main();
		}

		static void timerTorchCallback(TimerHandle_t xTimer)
		{
			HallOfFameDisplayControl* instance = static_cast<HallOfFameDisplayControl*>(pvTimerGetTimerID(xTimer));
			xEventGroupSetBits(instance->eventGroup, eventBitTimerTorch);
		}

		// CleanRTOS-timers rekenen in microseconden, FreeRTOS-timers in ticks (minstens 1).
		static TickType_t usToTicks(uint64_t us)
		{
			TickType_t ticks = pdMS_TO_TICKS(us / 1000);
			return (ticks > 0) ? ticks : 1;
		}

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


		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

			initPanels();

			PAGE pageHallOfFame("rootHallOfFameDisplay",display);
			defineDisplayHierarchy(pageHallOfFame);

			hidePage(pageHallOfFame);	// bij het eerste opstarten is er nog niets verloren.

			// Elke toestand wacht alleen op de vlaggen die daar kunnen vallen. Dat mag
			// omdat "Play Again" alleen enabled is zolang de pagina in beeld staat en
			// TouchscreenButton::touchPressed terugkeert op !isEnabled(): een Hide kan
			// dus niet buiten beeld ontstaan.
			//
			// En let op hasFired(): latestResult van waitAny is de hele event group,
			// niet alleen de gevraagde bits. Vraag hem daarom alleen naar waitables
			// die ook echt in de wachtset stonden.
			while (true)
			{

				switch(state)
				{
					case STATE_HIDDEN:
						firedBits = xEventGroupWaitBits(eventGroup, eventBitShowHallOfFamePage, pdTRUE, pdFALSE, portMAX_DELAY);

						if(hasFired(eventBitShowHallOfFamePage))
						{
							xQueuePeek(hallOfFameQueue, &snapshot, 0);
							fillHallOfFameLines(snapshot);
							showPage(pageHallOfFame);

							// De klok loopt alleen zolang de fakkels in beeld staan:
							// elke tik is een volledige hertekening. Eenmalig, zodat
							// elke wachttijd gerandomiseerd kan worden.
							xEventGroupClearBits(eventGroup, eventBitTimerTorch);
							xTimerChangePeriod(timerTorch, usToTicks(TorchesDisplayHelpers::nextTorchIntervalUs()), portMAX_DELAY);
							state = STATE_SHOWN;
						}
						break;

					case STATE_SHOWN:
						firedBits = xEventGroupWaitBits(eventGroup, eventBitHideHallOfFamePage | eventBitShowHallOfFamePage | eventBitTimerTorch, pdFALSE, pdFALSE, portMAX_DELAY);

						if(hasFired(eventBitHideHallOfFamePage))
						{
							enterHidden(pageHallOfFame);
							state = STATE_HIDDEN;
						}
						else if(hasFired(eventBitShowHallOfFamePage))
						{
							// Een nieuwe lijst op de pagina die er al staat.
							xQueuePeek(hallOfFameQueue, &snapshot, 0);
							fillHallOfFameLines(snapshot);
							pageHallOfFame.redraw(true);

							// De klok liep al, maar hem hier opnieuw opwinden houdt de
							// regel overeind dat elke tak die de lijst neerzet ook de
							// fakkels aanzwengelt. Anders hangt het branden ervan aan het
							// feit dat een gemiste tik gelatched blijft staan, en dat is
							// een aanname die niemand hier ziet.
							xEventGroupClearBits(eventGroup, eventBitTimerTorch);
							xTimerChangePeriod(timerTorch, usToTicks(TorchesDisplayHelpers::nextTorchIntervalUs()), portMAX_DELAY);
						}
						else if(hasFired(eventBitTimerTorch))
						{
							selectNextAnimationFrameOfTorches();
							pageHallOfFame.redraw(true);

							// Eenmalige timer, zodat elke wachttijd gerandomiseerd kan
							// worden.
							xEventGroupClearBits(eventGroup, eventBitTimerTorch);
							xTimerChangePeriod(timerTorch, usToTicks(TorchesDisplayHelpers::nextTorchIntervalUs()), portMAX_DELAY);
						}
						break;

					default:
						// De wachtoproep zit binnen de switch, dus een toestand die hier
						// niet staat laat de lus vrij rondtollen - en assert is in een
						// release-build leeg. Terug naar een toestand die wel blokkeert.
						assert(false);
						state = STATE_HIDDEN;
						break;
				}
			}
		}
	}; // end class HallOfFameDisplayControl

};// end namespace pong_super_frt
