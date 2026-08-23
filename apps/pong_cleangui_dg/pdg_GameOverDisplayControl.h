// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

// De DisplayControl van de game-over-pagina, met een eigen PageRoot en een eigen
// taak. Zie pdg_GameDisplayControl.h voor waarom het verbergen niets tekent.

using namespace crt;
namespace pong_cleangui_dg
{
	// Zie pdg_BallControl.h: logger komt uit namespace crt, niet uit deze.

	// Wat GameOverControl van deze pagina mag vragen, staat in het publieke blok na
	// de constructor. Zie pdg_StartPageDisplayControl.h voor waarom daar geen
	// interface meer boven staat.
	class GameOverDisplayControl : public Task
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
		Panel<1> gameOverLabelPanel;
		Text<0> gameOverText;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonStart;

		Flag flagShowGameOverPage;
		Flag flagDeshowGameOverPage;

		State state;

	public:
		GameOverDisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber,
							IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
			display(display),
			freeFonts(freeFonts),
			screenWidth(0),
			screenHeight(0),
			flagShowGameOverPage(this),
			flagDeshowGameOverPage(this),
			state (STATE_HIDDEN)
		{
			Vec2 screenSize = display.getScreenSize();
			screenWidth  = screenSize.x;
			screenHeight = screenSize.y;

			display.addTouchListener(&buttonStart);

			start();
		}

	IButton* addGameOverPageStartButtonListener(IButtonListener* pButtonListener)
	{
		buttonStart.addButtonListener(pButtonListener);
		return &buttonStart;
	}

	void showGameOverPage()
	{
		flagDeshowGameOverPage.clear();	// alleen de laatste wens telt.
		flagShowGameOverPage.set();
	}

	void hideGameOverPage()
	{
		flagShowGameOverPage.clear();
		flagDeshowGameOverPage.set();
	}

	private:
		void initPanels()
		{
			backgroundPanel.setPanelProps("gameOverBackground", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(screenWidth, screenHeight), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00000000, /*colBg*/0x00000000);

			gameOverLabelPanel.setPanelProps
				("gameOverLabelPanel",/*locPos*/Vec2(100, 250), CoordType::Promillage,
				/*size*/Vec2(800, 200), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::TopLeft,	/*colFg*/0x00202020, /*colBg*/0x00000000);

			// locPos (0,0): voor een MidMid-tekst is de oorsprong al het midden van
			// de ouder (zie Text::calcOriginPix). Er nog eens 500 promille bij
			// optellen schoof het label een halve paneelbreedte naar rechts.
			// fontScale is hier expliciet: het automatisch schalen op 0 zit in
			// TouchscreenButton, niet in een kale Text.
			gameOverText.setTextProps
				(/*locPos*/Vec2(0, 0), CoordType::Promillage, Alignment::MidMid,
				"GAME OVER", /*font*/2, /*color*/0x00FF4040, /*fontScale*/2);

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

			// Dragend, geen dubbeling: tekenen maakt niets meer aanraakbaar. Wie
			// deze regel weghaalt laat de startknop doof achter, en dan is er geen
			// weg meer uit de game-over-pagina.
			page.enable(true);
		}

		// Verbergen mag hier niets tekenen (zie GameDisplayControl), dus gaat
		// alleen de tekenvlag om. hide() zou juist wel over dit vlak gaan, en dat
		// kruist het tekenwerk van de taak van de pagina die daarna in beeld komt.
		void hidePage(PageRoot<20>& page)
		{
			page.makeNonDrawable(true);

			// Ook dragend: de vlag hierboven houdt de pagina alleen uit het
			// tekenwerk, niet uit de touchafhandeling.
			page.disable(true);
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

			PageRoot<20> pageGameOver("rootGameOverDisplay",display);
			pageGameOver.addChildWidget(backgroundPanel);		// eerst, want alles hierna tekent erop.
			pageGameOver.addChildWidget(gameOverLabelPanel);	// pas hierna heeft gameOverLabelPanel een display,
			gameOverLabelPanel.addChildWidget(gameOverText);	// en daar staat addChildWidget op: van boven naar beneden.
			pageGameOver.addChildWidget(buttonStart);

			hidePage(pageGameOver);		// bij het eerste opstarten is er nog niets verloren.

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				waitAny(flagShowGameOverPage + flagDeshowGameOverPage);

				switch(state)
				{
					case STATE_HIDDEN:
						if(hasFired(flagShowGameOverPage))
						{
							showPage(pageGameOver);
							state = STATE_SHOWN;
						}
						else if(hasFired(flagDeshowGameOverPage))
						{
							// al uit beeld: niets te doen.
						}
						break;

					case STATE_SHOWN:
						if(hasFired(flagDeshowGameOverPage))
						{
							hidePage(pageGameOver);
							state = STATE_HIDDEN;
						}
						else if(hasFired(flagShowGameOverPage))
						{
							// al in beeld: niets te doen.
						}
						break;

					default:
						assert(false);
				}
			}
		}
	}; // end class GameOverDisplayControl

};// end namespace pong_cleangui_dg
