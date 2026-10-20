// by Marius Versteegen, 2026

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <crt_CleanGUI.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include "images/_pal8/_allImages.h"
#include "psupf_TorchesDisplayHelpers.h"

// De DisplayControl van de startpagina, met een eigen PageRoot en een eigen
// taak. Zie psupf_GameDisplayControl.h voor waarom het verbergen niets tekent.

using namespace crt;
namespace pong_super_frt
{
	// Zie psupf_BallControl.h: logger komt uit namespace crt, niet uit deze.

	typedef TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> PlayButton;

	// Gecentreerd tussen het regeltje onder de titel (dat op y=124 eindigt) en de
	// onderkant van het scherm: (124+240)/2 = 182, en de knop is 54 hoog.
	const int PlayButtonTopPix = 155;

	// Wat StartPageControl van deze pagina mag vragen, staat in het publieke blok
	// na de constructor: addStartPageStartButtonListener, showStartPage en
	// hideStartPage. Daar stond een IStartPageDisplayControl voor, met dezelfde
	// drie regels in hetzelfde bestand; zie de toelichting boven
	// psupf_GameDisplayControl.h::GameDisplayControl.
	class StartPageDisplayControl
	{

		// --- FreeRTOS (was CleanRTOS) ---
		static constexpr EventBits_t eventBitShowStartPage = (1 << 0);
		static constexpr EventBits_t eventBitHideStartPage = (1 << 1);
		static constexpr EventBits_t eventBitTimerTorch = (1 << 2);
		EventGroupHandle_t eventGroup;
		EventBits_t firedBits;		// wat de laatste wachtoproep op meer tegelijk opleverde; zie hasFired()
		TaskHandle_t taskHandle;

		enum State
		{
			STATE_HIDDEN,
			STATE_SHOWING
		};

	private:
		IDisplay& display;
		TFT_eSPI_IFreeFonts& freeFonts;

		int screenWidth;
		int screenHeight;

		// Schermvullend, net als bij de andere twee pagina's. Daar leunt de
		// paginawissel op: wie in beeld komt wist wat de vorige achterliet.
		Panel<0> backgroundPanel;

		// UC02 stap 1: de titel van het spel. Een plaatje en geen Text: de
		// slagschaduw en de omlijning eromheen zijn met drawString niet te maken.
		Panel<0> titlePanel;

		// Het regeltje onder de titel. Ook een plaatje: de letterafstand die het
		// zijn sci-fi-uiterlijk geeft, kent drawString niet.
		Panel<0> creditsPanel;

		PlayButton buttonStart;

		// Twee brandende fakkels naast de knop. Hun cel schuift per tik van
		// timerTorch een op; zie de toelichting bij TorchFrameIntervalMs.
		Panel<0> arTorchPanels[2];
		int32_t arIndexTorchFrame[2];

		TimerHandle_t timerTorch;

		State state;

	public:
		StartPageDisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber,
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
			Vec2 screenSize = display.getScreenSize();
			screenWidth  = screenSize.x;
			screenHeight = screenSize.y;

			display.addTouchListener(&buttonStart);

			xTaskCreatePinnedToCore(staticMain, taskName, taskSizeBytes, this, taskPriority, &taskHandle, taskCoreNumber);
		}

	IButton* addStartPageStartButtonListener(IButtonListener* pButtonListener)
	{
		buttonStart.addButtonListener(pButtonListener);
		return &buttonStart;
	}

	void showStartPage()
	{
		xEventGroupClearBits(eventGroup, eventBitHideStartPage);	// alleen de laatste wens telt.
		xEventGroupSetBits(eventGroup, eventBitShowStartPage);
	}

	void hideStartPage()
	{
		xEventGroupClearBits(eventGroup, eventBitShowStartPage);
		xEventGroupSetBits(eventGroup, eventBitHideStartPage);
	}

	private:
		void initPanels()
		{
			backgroundPanel.setPanelProps("startBackground", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(screenWidth, screenHeight), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00000000, /*colBg*/0x00000000);

			// Titel en regeltje samen boven de knop, die op PlayButtonTopPix (135)
			// staat. De titel is 72 hoog, het regeltje 20: op y=32 begint de titel
			// en eindigt het regeltje op 124, met elf pixels lucht tot de knop.
			Vec2 titleSizePix(imageTitleSuperPong.width, imageTitleSuperPong.height);
			titlePanel.setPanelProps("titlePanel",
				/*locPos*/Vec2((screenWidth - titleSizePix.x)/2, 32), CoordType::Pixels,
				/*size*/titleSizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colFg*/0x0000FFFF, /*colBg*/0x00000000);
			titlePanel.setImage(&imageTitleSuperPong);

			Vec2 creditsSizePix(imageCreditsLine.width, imageCreditsLine.height);
			creditsPanel.setPanelProps("creditsPanel",
				/*locPos*/Vec2((screenWidth - creditsSizePix.x)/2, 104), CoordType::Pixels,
				/*size*/creditsSizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colFg*/0x0000B4C8, /*colBg*/0x00000000);
			creditsPanel.setImage(&imageCreditsLine);

			initPlayButton();
			TorchesDisplayHelpers::initTorches(arTorchPanels, screenWidth, PlayButtonTopPix, imageButtonPlay.height);
		}

		void initPlayButton()
		{
			// Alles in pixels en niet in promillage. Bij een knop die uit een plaatje
			// bestaat is dat geen smaak: promillage wordt naar hele pixels afgerond,
			// en een panel dat daardoor een pixel groter uitvalt dan het plaatje laat
			// Panel::needsBackground() de vlakke colPanel-kleur er alsnog onder
			// schilderen. Die rand van een pixel is precies zichtbaar genoeg om lelijk
			// te zijn.
			Vec2 sizePix(imageButtonPlay.width, imageButtonPlay.height);

			// Het label is leeg omdat "PLAY" in het plaatje zit - met de schuine kant
			// en het verloop eromheen, die geen enkele fillRect kan maken. colPanel en
			// colFont blijven wel ingevuld: het plaatje is transparant rond zijn
			// afgeronde hoeken, dus zodra iemand de setImage() hieronder weghaalt is
			// er weer een gewone rode knop met tekst.
			buttonStart.setTouchScreenButtonProps
				("play",/*locPos*/Vec2((screenWidth - sizePix.x)/2, PlayButtonTopPix), CoordType::Pixels,
				/*size*/sizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF,
				/*fontScale*/ 1, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);

			// fontScale 1 en niet 0: bij 0 zoekt TouchscreenButton de grootste schaal
			// die past, en dat is werk zonder betekenis voor een leeg label.
			buttonStart.setImage(&imageButtonPlay);
		}

		// Zie de toelichting bij GameDisplayControl::showPage/hidePage - dit is
		// dezelfde afspraak, en die moet voor alle drie de pagina's gelden.
		//
		// De enable() en de disable() hieronder dragen het hele aan/uit-gedrag van
		// de playknop, want tekenbaar en aanraakbaar zijn losse assen. Zonder de
		// enable() is de knop al vanaf de eerste hidePage() dood en begint er
		// nooit een spel; zonder de disable() blijft hij het hele spel en de
		// game-over-pagina door aanraakbaar op dezelfde rect als de peddel-touch,
		// en vervalt de bPressed-reset van TouchscreenButton::disable().
		void showPage(IWidget& page)
		{
			// Los uitgeschreven omdat page hier een IWidget& is; show() en hide()
			// zijn geen interfacefuncties meer.
			page.makeDrawable(true);
			page.redraw(true);
			page.enable(true);
		}

		// Uit beeld en met opzet zonder ook maar een pixel te sturen - vandaar
		// makeNonDrawable en niet hide(): hide() wist zijn vlak wel degelijk, en
		// dat mag hier niet terwijl de taak van de volgende pagina al in dezelfde
		// commandobuffer tekent. Het hoeft ook niet: elke pagina begint met een
		// schermvullend backgroundPanel en wist daarmee wat hier blijft staan.
		void hidePage(IWidget& page)
		{
			page.makeNonDrawable(true);
			page.disable(true);
		}

		void defineDisplayHierarchy(IWidget& pageRoot)
		{
			// Let op: je kunt niet meer children aan pageRoot
			// toevoegen dan is gespecificeerd in zijn template parameter
			// (op dit moment is dat 20, zie main functie)
			pageRoot.addChildWidget(backgroundPanel);	// eerst, want alles hierna tekent erop.
			pageRoot.addChildWidget(titlePanel);
			pageRoot.addChildWidget(creditsPanel);
			pageRoot.addChildWidget(buttonStart);
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
			static_cast<StartPageDisplayControl*>(pvParameters)->main();
		}

		static void timerTorchCallback(TimerHandle_t xTimer)
		{
			StartPageDisplayControl* instance = static_cast<StartPageDisplayControl*>(pvTimerGetTimerID(xTimer));
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

			PageRoot<20> pageStart("rootStartPageDisplay",display);
			defineDisplayHierarchy(pageStart);

			hidePage(pageStart);	// StartPageControl vraagt hem zo meteen zelf op.

			while (true)
			{

				switch(state)
				{
					case STATE_HIDDEN:
						firedBits = xEventGroupWaitBits(eventGroup, eventBitShowStartPage, pdTRUE, pdFALSE, portMAX_DELAY);
						if(hasFired(eventBitShowStartPage))
						{
							showPage(pageStart);

							// De fakkels flakkeren alleen zolang de pagina in beeld
							// staat: elke tik is een volledige hertekening.
							xEventGroupClearBits(eventGroup, eventBitTimerTorch);
							xTimerChangePeriod(timerTorch, usToTicks(TorchesDisplayHelpers::nextTorchIntervalUs()), portMAX_DELAY);
							state = STATE_SHOWING;
						}
						break;

					case STATE_SHOWING:
						firedBits = xEventGroupWaitBits(eventGroup, eventBitHideStartPage | eventBitTimerTorch, pdFALSE, pdFALSE, portMAX_DELAY);
						if(hasFired(eventBitHideStartPage))
						{
							xTimerStop(timerTorch, 0);
							hidePage(pageStart);
							state = STATE_HIDDEN;
						}
						else if (hasFired(eventBitTimerTorch))
						{
							selectNextAnimationFrameOfTorches();
							pageStart.redraw(true);

							// Eenmalige timer, zodat elke wachttijd gerandomiseerd kan worden,
							// tbv een "stochastischer vlameffect".
							xEventGroupClearBits(eventGroup, eventBitTimerTorch);
							xTimerChangePeriod(timerTorch, usToTicks(TorchesDisplayHelpers::nextTorchIntervalUs()), portMAX_DELAY);
						}
						break;

					default:
						assert(false);
				}
			}
		}
	}; // end class StartPageDisplayControl

};// end namespace pong_super_frt
