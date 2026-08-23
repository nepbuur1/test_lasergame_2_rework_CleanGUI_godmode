// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

// De DisplayControl van de startpagina, met een eigen PageRoot en een eigen
// taak. Zie pdg_GameDisplayControl.h voor waarom het verbergen niets tekent.

using namespace crt;
namespace pong_cleangui_dg
{
	// Zie pdg_BallControl.h: logger komt uit namespace crt, niet uit deze.

	// Wat StartPageControl van deze pagina mag vragen, staat in het publieke blok
	// na de constructor. Hier stond een IStartPageDisplayControl die datzelfde
	// rijtje nog een keer opsomde - een interface met een implementatie, in
	// hetzelfde bestand dat de aanroeper toch al includeert.
	class StartPageDisplayControl : public Task
	{
		enum State
		{
			STATE_HIDDEN,
			STATE_SHOWN
		};

	private:
		IDisplay& display;
		TFT_eSPI_IFreeFonts& freeFonts;

		int screenWidth;
		int screenHeight;

		// Schermvullend, net als bij de andere twee pagina's. Daar leunt de
		// paginawissel op: wie in beeld komt wist wat de vorige achterliet.
		Panel<0> backgroundPanel;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonStart;

		Flag flagShowStartPage;
		Flag flagDeshowStartPage;

		State state;

	public:
		StartPageDisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber,
							IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
			display(display),
			freeFonts(freeFonts),
			screenWidth(0),
			screenHeight(0),
			flagShowStartPage(this),
			flagDeshowStartPage(this),
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
		flagDeshowStartPage.clear();	// alleen de laatste wens telt.
		flagShowStartPage.set();
	}

	void hideStartPage()
	{
		flagShowStartPage.clear();
		flagDeshowStartPage.set();
	}

	private:
		void initPanels()
		{
			backgroundPanel.setPanelProps("startBackground", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(screenWidth, screenHeight), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00000000, /*colBg*/0x00000000);

			// Let op: locPos is de LINKERBOVENHOEK van de widget. De Alignment
			// hieronder geldt alleen voor de tekst binnen de knop, niet voor de
			// plek van de knop zelf. Een knop van 800 promille breed hoort dus op
			// x=100: (1000-800)/2.
			buttonStart.setTouchScreenButtonProps
				("start",/*locPos*/Vec2(100, 562), CoordType::Promillage,
				/*size*/Vec2(800, 225), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::TopLeft,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "Click here to Start", /*buttonFont*/ 1, /*colFont*/ 0x0000FFFF, /*fontScale*/ 0, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);
		}

		// Zie de toelichting bij GameDisplayControl::showPage/hidePage - dit is
		// dezelfde afspraak, en die moet voor alle drie de pagina's gelden.
		void showPage(PageRoot<20>& page)
		{
			page.show(true);

			// Dragend, geen dubbeling: show() maakt niet langer aanraakbaar.
			// Weg is een startknop die netjes in beeld staat en nergens op reageert.
			page.enable(true);
		}

		void hidePage(PageRoot<20>& page)
		{
			// Dragend, geen dubbeling: onzichtbaar maken maakt niet doof. Een knop
			// die uit beeld is meldt anders nog gewoon drukken, en disable() is de
			// enige plek die de bPressed opruimt van de druk waarmee deze pagina
			// zojuist is weggezet.
			page.disable(true);

			page.makeNonDrawable(true);		// met opzet geen hide(); zie regel 112.
		}

		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

#ifdef LOAD_GFXFF							  // Don't select font indices 0 and 1 - they are reserved.
			freeFonts.setFreeFont(10, FSB9);  // Free serif 9 pix font.
			freeFonts.setFreeFont(12, FSB12); // Free serif 12 pix font.
#endif
			initPanels();

			PageRoot<20> pageStart("rootStartPageDisplay",display);
			pageStart.addChildWidget(backgroundPanel);	// eerst, want alles hierna tekent erop.
			pageStart.addChildWidget(buttonStart);

			hidePage(pageStart);	// StartPageControl vraagt hem zo meteen zelf op.

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				waitAny(flagShowStartPage + flagDeshowStartPage);

				switch(state)
				{
					case STATE_HIDDEN:
						if(hasFired(flagShowStartPage))
						{
							showPage(pageStart);
							state = STATE_SHOWN;
						}
						else if(hasFired(flagDeshowStartPage))
						{
							// al uit beeld: niets te doen.
						}
						break;

					case STATE_SHOWN:
						if(hasFired(flagDeshowStartPage))
						{
							hidePage(pageStart);
							state = STATE_HIDDEN;
						}
						else if(hasFired(flagShowStartPage))
						{
							// al in beeld: niets te doen.
						}
						break;

					default:
						assert(false);
				}
			}
		}
	}; // end class StartPageDisplayControl

};// end namespace pong_cleangui_dg
