// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>

//#include <crt_TFT_eSPI_DisplayAdapter.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_cg
{
	// Zie pcg_BallControl.h: logger komt uit namespace crt, niet uit deze.

	struct TwoNumbers
	{
		TwoNumbers():number1(0),number2(0){}
		TwoNumbers(int number1, int number2):number1(number1),number2(number2){}

		int number1;
		int number2;
	};

	// ---- Spelmaten, allemaal in pixels ------------------------------------
	//
	// Het spel rekende eerst in cellen van een raster: een bal die per stap een
	// hele cel verspringt. Dat is precies wat een tekstconsole aankan en wat een
	// scherm niet nodig heeft. Alles hieronder is nu in pixels, en de bal legt er
	// een paar per beeld af - dat is het verschil tussen verspringen en bewegen.
	//
	// De oorsprong van het speelveld ligt LINKSONDER: y=0 is de peddellijn en y
	// loopt omhoog. SceneDisplayControl is de enige die dat naar schermpixels
	// omrekent (het scherm telt van boven).
	const int FrameIntervalMs      = 40;	// 25 beelden per seconde.
	const int WallThicknessPix     = 10;
	const int BallSizePix          = 12;
	const int PaddleWidthPix       = 50;
	const int PaddleHeightPix      = 12;
	const int BallSpeedPixPerFrame = 4;		// 100 pixels per seconde.
	const int PaddleStepPix        = 6;		// 150 pixels per seconde zolang de knop ingedrukt blijft.

	class IDisplayControl
	{
		public:
			virtual int getWidth() = 0;
			virtual int getHeight() = 0;
			virtual IButton* addLeftButtonListener(IButtonListener* pButtonListener) = 0;
			virtual IButton* addRightButtonListener(IButtonListener* pButtonListener) = 0;
			virtual IButton* addStartPageStartButtonListener(IButtonListener* pButtonListener) = 0;
			virtual IButton* addGameOverPageStartButtonListener(IButtonListener* pButtonListener) = 0;
			virtual void updateBallPosition(int x, int y) = 0;
			virtual void updatePaddlePosition(int x) = 0;
			virtual void enable() = 0;
			virtual void disable() = 0;
	};

	template <int WIDTH, int HEIGHT>
	class DisplayControl : public Task, public IDisplayControl
	{
		enum State
		{
			STATE_START,
			STATE_GAME,
			STATE_GAMEOVER
		};

	private:
		IDisplay& display;
		TFT_eSPI_IFreeFonts& freeFonts;

		// Het speelveld in pixels, met de oorsprong LINKSONDER. Deze klasse is de
		// enige die dat naar schermpixels omrekent - het scherm telt van boven.
		int width;			// speelveldbreedte in pixels
		int height;			// speelveldhoogte in pixels
		int frameDelayMs;

		// DisplayControl bevat de Widgets van alle pagina's van de game:
		// - De Start Game pagina
		// - De Game Over pagina
		// - De Pagina met de Game elementen

		// Via de functies selectStartPage, selectGameOverPage en selectGamePage
		// kun je kiezen welke pagina wordt weergegeven.

		// Sommige Widgets kunnen gekoppeld zijn aan TouchscreenButtons.
		// Middels de calls add addLeftPaddleButtonListener, 
		// addRightPanelButtonListener, addStartPageStartButtonListener
		// en addGameOverPageStartButtonListener kunnen de geinteresseerde 
		// controlobject zichzelf abonneren op berichten van die knoppen.

		// De panels krijgen hun maat en plek pas in main(), zodra de veldmaat
		// bekend is. Ze hier al invullen zou betekenen dat de initialisatielijst
		// leden leest die nog niet bestaan.
		
		// Game Panel Widgets
		// gamePanel heeft een extra nut: moveTo() laat de vrijgekomen strook
		// door de boom opnieuw tekenen, en zonder iets dat daar wel wat tekent
		// blijft de oude bal gewoon staan.
		Panel<8> gamePanel;
		Panel<0> ballPanel;
		Panel<0> bottomLinePanel;
		Panel<0> leftBorderPanel;
		Panel<0> rightBorderPanel;
		Panel<0> topBorderPanel;
		Panel<0> paddlePanel;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonLeft;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonRight;

		// GameOver Panel Widgets
		Panel<2> gameOverPanel;
		Panel<1> gameOverLabelPanel;
		Text<0> gameOverText;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonGameOverStart;

		// StartPage Widgets
		Panel<1> startPagePanel;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonStartPageStart;

		Pool<TwoNumbers> poolBallPosition;
		Pool<int> poolPaddlePosition;
		Flag flagSelectStartPage;
		Flag flagSelectGamePage;
		Flag flagSelectGameOverPage;
		Flag flagShowPaddle;
		Flag flagHidePaddle;
		Flag flagBallPositionUpdated;
		Flag flagPaddlePositionUpdated;
		
		State state;

	public:
		DisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, int frameDelayMs,
							IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
			display(display),
			freeFonts(freeFonts),
			width(0),
			height(0),
			frameDelayMs(frameDelayMs),
			flagSelectStartPage(this),
			flagSelectGamePage(this),
			flagSelectGameOverPage(this),
			flagShowPaddle(this),
			flagHidePaddle(this),
			flagBallPositionUpdated(this),
			flagPaddlePositionUpdated(this),
			state (STATE_START)
		{
			// getScreenSize() mag hier al: AsyncDisplay geeft de rotatie door in
			// zijn eigen constructor, dus de adapter kent de gedraaide maat al.
			// Het speelveld krijgt de bovenste driekwart van het scherm; de
			// onderste kwart is van de peddelknoppen van PaddleControl.
			Vec2 screenSize = display.getScreenSize();
			width  = screenSize.x;
			height = (screenSize.y * 3)/4;

			poolBallPosition.write(TwoNumbers(width/2, height/2));
			poolPaddlePosition.write(width/2);
			start();
		}

    int getWidth()
    {
		return width;
    }

	int getHeight()
	{
		return height;
	}

	IButton* addLeftButtonListener(IButtonListener* pButtonListener)
	{
		buttonLeft.addButtonListener(pButtonListener);
		return &buttonLeft;
	}

	IButton* addRightButtonListener(IButtonListener* pButtonListener)
	{
		buttonRight.addButtonListener(pButtonListener);
		return &buttonRight;
	}

	IButton* addStartPageStartButtonListener(IButtonListener* pButtonListener)
	{
		buttonStartPageStart.addButtonListener(pButtonListener);
		return &buttonStartPageStart;
	}

	IButton* addGameOverPageStartButtonListener(IButtonListener* pButtonListener)
	{
		buttonGameOverStart.addButtonListener(pButtonListener);
		return &buttonGameOverStart;
	}

	void updateBallPosition(int x, int y)
	{
		poolBallPosition.write(TwoNumbers(x,y));
		flagBallPositionUpdated.set();
	}

	void updatePaddlePosition(int x)
	{
		poolPaddlePosition.write(x);
		flagPaddlePositionUpdated.set();
	}

	void selectStartPage()
	{
		flagSelectGameOverPage.clear(); // make sure that only one page is selected.
		flagSelectGamePage.clear();
		flagSelectStartPage.set();
	}

	void selectGamePage()
	{
		flagSelectGameOverPage.clear(); // make sure that only one page is selected.
		flagSelectStartPage.clear();
		flagSelectGamePage.set();
	}

	void selectGameOverPage()
	{
		flagSelectGamePage.clear(); // make sure that only one page is selected.
		flagSelectStartPage.clear();
		flagSelectGameOverPage.set();
	}

	void showPaddle()
	{
		flagShowPaddle.set();
	}

	void hidePaddle()
	{
		flagHidePaddle.set();
	}

	private:
		// De linkerbovenhoek waarmee een widget van sizePix met zijn midden op
		// speelveldpunt (x,y) terechtkomt. Twee dingen zitten hierin:
		//
		//  - de y-as klapt om. Het spel telt van onderen (peddellijn op y=0), het
		//    scherm van boven. Dit is de enige plek die dat weet - in de
		//    console-versie deed CharMatrix::setChar() precies hetzelfde.
		//  - een Widget wordt op zijn locPos met de LINKERBOVENHOEK geplaatst.
		//    De Alignment-parameter doet daar niet aan mee: die geldt alleen voor
		//    een Image of een Text binnen een widget. Zelf uitrekenen dus.
		Vec2 centreToTopLeftPix(int x, int y, const Vec2& sizePix) const
		{
			return Vec2(x - sizePix.x/2, (height-1-y) - sizePix.y/2);
		}

		// Maten en plekken van alle panels. Pas hier te bepalen: in de constructor
		// is de schermmaat nog niet in een speelveld vertaald.
		void initGamePanels()
		{
			gamePanel.setPanelProps("backgroundPanel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(width, height), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00000000, /*colBg*/0x00000000);

			ballPanel.setPanelProps("ballPanel", /*locPos*/centreToTopLeftPix(width/2, height/2, ballSizePix()), CoordType::Pixels,
				/*size*/ballSizePix(), /*cornerRadius*/BallSizePix/2, CoordType::Pixels,
				Alignment::MidMid, /*colFg*/0x00FFFF00, /*colBg*/0x00000000);

			paddlePanel.setPanelProps("paddlePanel", /*locPos*/centreToTopLeftPix(width/2, PaddleHeightPix/2, paddleSizePix()), CoordType::Pixels,
				/*size*/paddleSizePix(), /*cornerRadius*/PaddleHeightPix/3, CoordType::Pixels,
				Alignment::MidMid, /*colFg*/0x0000FFff, /*colBg*/0x00000000);

			// De wanden sluiten het speelveld links, rechts en boven af. Hun dikte
			// is waar BallControl tegenaan kaatst.
			leftBorderPanel.setPanelProps("leftBorderPanel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(WallThicknessPix, height), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x0000FFff, /*colBg*/0x00000000);

			rightBorderPanel.setPanelProps("rightBorderPanel", /*locPos*/Vec2(width - WallThicknessPix, 0), CoordType::Pixels,
				/*size*/Vec2(WallThicknessPix, height), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x0000FFff, /*colBg*/0x00000000);

			topBorderPanel.setPanelProps("topBorderPanel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(width, WallThicknessPix), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x0000FFff, /*colBg*/0x00000000);

			// Onderaan zit geen wand maar de doellijn: daar gaat de bal eruit.
			bottomLinePanel.setPanelProps("bottomLinePanel", /*locPos*/Vec2(0, height - 2), CoordType::Pixels,
				/*size*/Vec2(width, 2), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00404040, /*colBg*/0x00000000);
		}

		void initGameOverPanels()
		{
			gameOverPanel.setPanelProps("gameOverPanel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(width, height), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00000000, /*colBg*/0x00000000);

			gameOverLabelPanel.setPanelProps
				("gameOverPanel",/*locPos*/Vec2(100, 250), CoordType::Promillage,
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

			buttonGameOverStart.setPanelProps
				("start",/*locPos*/Vec2(100, 562), CoordType::Promillage,
				/*size*/Vec2(800, 225), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::TopLeft,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "Click here to Start", /*buttonFont*/ 1, /*colFont*/ 0x0000FFFF, /*fontScale*/ 0, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);
		}

		void initStartPagePanels()
		{
			startPagePanel.setPanelProps("startPagePanel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(width, height), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00000000, /*colBg*/0x00000000);

			buttonStartPageStart.setPanelProps
				("start",/*locPos*/Vec2(100, 562), CoordType::Promillage,
				/*size*/Vec2(800, 225), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::TopLeft,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "Click here to Start", /*buttonFont*/ 1, /*colFont*/ 0x0000FFFF, /*fontScale*/ 0, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);
		}

		void initBallAndPanelPosition()
		{
			// Bal en peddel eerst op hun nieuwe plek zetten, en pas
			// daarna tonen. Anders tekent show(true) ze nog even op de
			// plek van het vorige spel - en voor de bal is dat onder de
			// doellijn, buiten het speelveld. De moveTo() die daar
			// meteen op volgt herstelt die plek niet: buiten
			// backgroundPanel is er niets dat daar iets tekent, dus
			// bleef er een tweede bal staan.
			//
			// setLocPos en niet moveTo: er staat op dit moment nog
			// niets op het scherm, dus er valt niets te herstellen.
			TwoNumbers ballPosition;
			poolBallPosition.read(ballPosition);
			ballPanel.setLocPos(centreToTopLeftPix(ballPosition.number1, ballPosition.number2, ballSizePix()),
								CoordType::Pixels, Alignment::MidMid);

			int paddlePosition;
			poolPaddlePosition.read(paddlePosition);
			paddlePanel.setLocPos(centreToTopLeftPix(paddlePosition, PaddleHeightPix/2, paddleSizePix()),
									CoordType::Pixels, Alignment::MidMid);

			flagBallPositionUpdated.clear();		// zojuist verwerkt; show(true) tekent ze.
			flagPaddlePositionUpdated.clear();
		}

		static Vec2 ballSizePix()   { return Vec2(BallSizePix, BallSizePix); }
		static Vec2 paddleSizePix() { return Vec2(PaddleWidthPix, PaddleHeightPix); }

		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

#ifdef LOAD_GFXFF							  // Don't select font indices 0 and 1 - they are reserved.
			freeFonts.setFreeFont(10, FSB9);  // Free serif 9 pix font.
			freeFonts.setFreeFont(12, FSB12); // Free serif 12 pix font.
#endif

			PageRoot<20> pageRoot("rootSceneDisplay",display);

			// Game Page toevoegen aan de pageRoot
			initGamePanels();
			pageRoot.addChildWidget(gamePanel);	// eerst, want alles hierna tekent erop.
			gamePanel.addChildWidget(bottomLinePanel);
			gamePanel.addChildWidget(leftBorderPanel);
			gamePanel.addChildWidget(rightBorderPanel);
			gamePanel.addChildWidget(topBorderPanel);
			gamePanel.addChildWidget(ballPanel);
			gamePanel.addChildWidget(paddlePanel);
			gamePanel.hide(true);
			gamePanel.disable(true);

			// Game Over Page toevoegen
			initGameOverPanels();
			pageRoot.addChildWidget(gameOverPanel);	// eerst, want alles hierna tekent erop.
			gameOverPanel.addChildWidget(gameOverLabelPanel);
			gameOverLabelPanel.addChildWidget(gameOverText);
			gameOverPanel.addChildWidget(buttonGameOverStart);

			// Start Page toevoegen
			initStartPagePanels();
			pageRoot.addChildWidget(startPagePanel);	// eerst, want alles hierna tekent erop.
			startPagePanel.addChildWidget(buttonStartPageStart);

			// Start the game by showing the Start Page Panel:
			startPagePanel.show(true);

			// Beeldtelling, om te kunnen nagaan of de 25 per seconde ook echt
			// gehaald worden in plaats van dat aan te nemen.
			int32_t nofFrames = 0;
			int64_t timeOfLastReportUs = esp_timer_get_time();

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.
				switch(state)
				{
					case STATE_GAME:
					{
						waitAny(flagSelectGameOverPage + flagSelectGamePage + flagSelectStartPage +
							    flagBallPositionUpdated + flagPaddlePositionUpdated);
						
						if(hasFired(flagSelectGamePage))
						{
							// state blijft STATE_GAME
							break;
						}
						else if(hasFired(flagSelectGameOverPage))
						{
							gamePanel.hide(true);
							gamePanel.disable(true);
							gameOverPanel.show(true);
							gameOverPanel.enabled(true);
							state = STATE_GAME_OVER;
						}
						else if(hasFired(flagSelectStartPage))
						{
							gamePanel.hide(true);
							gamePanel.disable(true);
							startPanel.show(true);
							startPanel.enabled(true);
							state = STATE_START;
						}
						else if(hasFired(flagShowPaddle))
						{
							paddlePanel.show(true);
							paddlePanel.enable(true);
						}
						else if(hasFired(flagHidePaddle))
						{
							paddlePanel.hide(true);
							paddlePanel.disable(true);
						}
						else if(hasFired(flagPaddlePositionUpdated))
						{
							int paddlePosition;
							poolPaddlePosition.read(paddlePosition);
							// moveTo() en niet setLocPos(): die laatste verzet alleen de
							// coordinaten en tekent niets, zodat de peddel op het scherm
							// blijft staan waar hij stond. moveTo() herstelt bovendien
							// alleen de strook die vrijkomt - dat is wat dit betaalbaar
							// maakt bij 25 beelden per seconde.
							paddlePanel.moveTo(centreToTopLeftPix(paddlePosition, PaddleHeightPix/2, paddleSizePix()), CoordType::Pixels);
							// state blijft STATE_GAME
						}
						else if(hasFired(flagBallPositionUpdated))
						{
							TwoNumbers ballPosition;
							poolBallPosition.read(ballPosition);
							ballPanel.moveTo(centreToTopLeftPix(ballPosition.number1, ballPosition.number2, ballSizePix()), CoordType::Pixels);

							nofFrames++;
							int64_t nowUs = esp_timer_get_time();
							if (nowUs - timeOfLastReportUs >= 1000000)
							{
								ESP_LOGI("pong", "%d fps", (int)nofFrames);
								nofFrames = 0;
								timeOfLastReportUs = nowUs;
							}
							// state blijft STATE_GAME
							break;
						}
						break;
				    }

					case STATE_GAME_OVER:
					{
						wait(flagSelectGamePage);

						initBallAndPaddlePosition();
						pageGame.show(true);
						pageGame.enable(true);
						state = STATE_GAME;
						break;
					}

					case STATE_START:
					{
						wait(flagSelectGamePage);

						initBallAndPaddlePosition();
						pageGame.show(true);
						pageGame.enable(true);
						state = STATE_GAME;
						break;
					}
					default:
						assert(false);
				}
			}
		}
	}; // end class SceneDisplayControl

};// end namespace crt
