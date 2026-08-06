// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include "pdga_PlayButton.h"

// De DisplayControl van de game-over-pagina, met een eigen PageRoot en een eigen
// taak. Zie pdga_GameDisplayControl.h voor waarom het verbergen niets tekent.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// Zie pdga_BallControl.h: logger komt uit namespace crt, niet uit deze.

	class IGameOverDisplayControl
	{
		public:
			virtual IButton* addGameOverPageStartButtonListener(IButtonListener* pButtonListener) = 0;
			virtual void showGameOverPage() = 0;
			virtual void hideGameOverPage() = 0;
	};

	class GameOverDisplayControl : public Task, public IGameOverDisplayControl
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

		// "GAME OVER" is nu een plaatje en geen Text meer. Het kader eromheen zit
		// er in getekend, dus het label dat er eerst onder lag is verdwenen: bij
		// de PAL8-adapter zou dat toch een vierkante bak zijn geworden - een
		// TFT_eSprite heeft geen afgeronde rechthoek - en in het plaatje is die
		// ronding er gewoon.
		Panel<0> gameOverPanel;
		PlayButton buttonStart;

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

			// Precies zo groot als het plaatje en in pixels, om dezelfde reden als
			// bij de knop - zie initPlayButton(). Het staat waar het kader stond:
			// midden op het scherm horizontaal, en verticaal zo dat het midden van
			// het plaatje op dezelfde hoogte uitkomt als het oude label.
			Vec2 gameOverSizePix(imageGameOver.width, imageGameOver.height);
			gameOverPanel.setPanelProps
				("gameOverPanel",/*locPos*/Vec2((screenWidth - gameOverSizePix.x)/2, 52), CoordType::Pixels,
				/*size*/gameOverSizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid,	/*colFg*/0x00202020, /*colBg*/0x00000000);
			gameOverPanel.setImage(&imageGameOver);

			initPlayButton(buttonStart, screenWidth);
		}

		// Zie de toelichting bij GameDisplayControl::showPage/hidePage - dit is
		// dezelfde afspraak, en die moet voor alle drie de pagina's gelden.
		void showPage(PageRoot<20>& page)
		{
			page.setDrawable(true, true);
			page.show(true);
			page.enable(true);
		}

		void hidePage(PageRoot<20>& page)
		{
			page.setDrawable(false, true);
			page.hide(true);
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
			pageGameOver.addChildWidget(gameOverPanel);
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

};// end namespace pong_cleangui_dg_adv
