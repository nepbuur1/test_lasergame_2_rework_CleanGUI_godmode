// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include "images/_pal8/_allImages.h"
#include "pdga_TorchesDisplayHelpers.h"

// De DisplayControl van de startpagina, met een eigen PageRoot en een eigen
// taak. Zie pdga_GameDisplayControl.h voor waarom het verbergen niets tekent.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// Zie pdga_BallControl.h: logger komt uit namespace crt, niet uit deze.

	typedef TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> PlayButton;

	// Gecentreerd tussen het regeltje onder de titel (dat op y=124 eindigt) en de
	// onderkant van het scherm: (124+240)/2 = 182, en de knop is 54 hoog.
	const int PlayButtonTopPix = 155;

	class IStartPageDisplayControl
	{
		public:
			virtual IButton* addStartPageStartButtonListener(IButtonListener* pButtonListener) = 0;
			virtual void showStartPage() = 0;
			virtual void hideStartPage() = 0;
	};

	class StartPageDisplayControl : public Task, public IStartPageDisplayControl
	{
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

		Flag flagShowStartPage;
		Flag flagHideStartPage;
		Timer timerTorch;

		State state;

	public:
		StartPageDisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber,
							IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
			display(display),
			freeFonts(freeFonts),
			screenWidth(0),
			screenHeight(0),
			arIndexTorchFrame{0, 5},
			flagShowStartPage(this),
			flagHideStartPage(this),
			timerTorch(this),
			state (STATE_HIDDEN)
		{
			Vec2 screenSize = display.getScreenSize();
			screenWidth  = screenSize.x;
			screenHeight = screenSize.y;

			display.addTouchListener(&buttonStart);

			start();
		}

	IButton* addStartPageStartButtonListener(IButtonListener* pButtonListener)
	{
		buttonStart.addButtonListener(pButtonListener);
		return &buttonStart;
	}

	void showStartPage()
	{
		flagHideStartPage.clear();	// alleen de laatste wens telt.
		flagShowStartPage.set();
	}

	void hideStartPage()
	{
		flagShowStartPage.clear();
		flagHideStartPage.set();
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
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				switch(state)
				{
					case STATE_HIDDEN:
						wait(flagShowStartPage);
						if(hasFired(flagShowStartPage))
						{
							showPage(pageStart);

							// De fakkels flakkeren alleen zolang de pagina in beeld
							// staat: elke tik is een volledige hertekening.
							timerTorch.start(TorchesDisplayHelpers::nextTorchIntervalUs());
							state = STATE_SHOWING;
						}
						break;

					case STATE_SHOWING:
						waitAny(flagHideStartPage + timerTorch);
						if(hasFired(flagHideStartPage))
						{
							timerTorch.stop();
							hidePage(pageStart);
							state = STATE_HIDDEN;
						}
						else if (hasFired(timerTorch))
						{
							selectNextAnimationFrameOfTorches();
							pageStart.redraw(true);

							// Eenmalige timer, zodat elke wachttijd gerandomiseerd kan worden,
							// tbv een "stochastischer vlameffect".
							timerTorch.start(TorchesDisplayHelpers::nextTorchIntervalUs());
						}
						break;

					default:
						assert(false);
				}
			}
		}
	}; // end class StartPageDisplayControl

};// end namespace pong_cleangui_dg_adv
