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
	// loopt omhoog. DisplayControl is de enige die dat naar schermpixels
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
			// Per pagina een show en een hide, net als in de _dg-variant. Elk
			// control-object toont zijn eigen pagina aan het begin van zijn
			// do..()-functie en verbergt hem aan het eind; MainControl roept die
			// strikt na elkaar aan, dus er staat er nooit meer dan een in beeld.
			//
			// Bewust geen select..(): dat zou beloven dat het tonen van de ene
			// pagina de andere vanzelf uitzet, en dat doet het niet.
			virtual void showStartPage() = 0;
			virtual void hideStartPage() = 0;
			virtual void showGamePage() = 0;
			virtual void hideGamePage() = 0;
			virtual void showGameOverPage() = 0;
			virtual void hideGameOverPage() = 0;
			virtual void showPaddle() = 0;
			virtual void hidePaddle() = 0;
	};

	template <int WIDTH, int HEIGHT>
	class DisplayControl : public Task, public IDisplayControl
	{
	private:
		IDisplay& display;
		TFT_eSPI_IFreeFonts& freeFonts;

		// Het speelveld in pixels, met de oorsprong LINKSONDER. Deze klasse is de
		// enige die dat naar schermpixels omrekent - het scherm telt van boven.
		int screenWidth;	// schermbreedte in pixels
		int screenHeight;	// schermhoogte in pixels
		int width;			// speelveldbreedte in pixels
		int height;			// speelveldhoogte in pixels
		int frameDelayMs;

		// DisplayControl bevat de Widgets van alle pagina's van de game:
		// - De Start Game pagina
		// - De Game Over pagina
		// - De Pagina met de Game elementen

		// Via de functieparen showStartPage/hideStartPage, showGamePage/hideGamePage
		// en showGameOverPage/hideGameOverPage kun je kiezen welke pagina wordt
		// weergegeven.

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
		// blijft de oude bal gewoon staan. Het beslaat daarom het HELE scherm:
		// het speelveld is alleen de bovenste driekwart, maar de peddelknoppen
		// in de onderste kwart horen bij dezelfde pagina.
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
		Flag flagShowStartPage;
		Flag flagHideStartPage;
		Flag flagShowGamePage;
		Flag flagHideGamePage;
		Flag flagShowGameOverPage;
		Flag flagHideGameOverPage;
		Flag flagShowPaddle;
		Flag flagHidePaddle;
		Flag flagBallPositionUpdated;
		Flag flagPaddlePositionUpdated;

	public:
		DisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, int frameDelayMs,
							IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
			display(display),
			freeFonts(freeFonts),
			screenWidth(0),
			screenHeight(0),
			width(0),
			height(0),
			frameDelayMs(frameDelayMs),
			flagShowStartPage(this),
			flagHideStartPage(this),
			flagShowGamePage(this),
			flagHideGamePage(this),
			flagShowGameOverPage(this),
			flagHideGameOverPage(this),
			flagShowPaddle(this),
			flagHidePaddle(this),
			flagBallPositionUpdated(this),
			flagPaddlePositionUpdated(this)
		{
			// getScreenSize() mag hier al: AsyncDisplay geeft de rotatie door in
			// zijn eigen constructor, dus de adapter kent de gedraaide maat al.
			// Het speelveld krijgt de bovenste driekwart van het scherm; de
			// onderste kwart is van de peddelknoppen.
			Vec2 screenSize = display.getScreenSize();
			screenWidth  = screenSize.x;
			screenHeight = screenSize.y;
			width  = screenWidth;
			height = (screenHeight * 3)/4;

			poolBallPosition.write(TwoNumbers(width/2, height/2));
			poolPaddlePosition.write(width/2);

			// De knoppen moeten hun aanrakingen van het display krijgen. Dit is de
			// enige plek waar dat gebeurt: alle knoppen van alle pagina's wonen
			// hier. Uitgeschakelde knoppen negeren een aanraking zelf (zie
			// TouchscreenButton::touchPressed), dus alle vier mogen ze meteen mee.
			display.addTouchListener(&buttonLeft);
			display.addTouchListener(&buttonRight);
			display.addTouchListener(&buttonStartPageStart);
			display.addTouchListener(&buttonGameOverStart);

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

	// Per pagina een paar: alleen de laatste wens telt, dus de tegenhanger gaat
	// leeg. Wat de andere pagina's doen staat hier los van - dat regelt de
	// aanroeper met zijn eigen show/hide.
	void showStartPage()
	{
		flagHideStartPage.clear();
		flagShowStartPage.set();
	}

	void hideStartPage()
	{
		flagShowStartPage.clear();
		flagHideStartPage.set();
	}

	void showGamePage()
	{
		flagHideGamePage.clear();
		flagShowGamePage.set();
	}

	void hideGamePage()
	{
		flagShowGamePage.clear();
		flagHideGamePage.set();
	}

	void showGameOverPage()
	{
		flagHideGameOverPage.clear();
		flagShowGameOverPage.set();
	}

	void hideGameOverPage()
	{
		flagShowGameOverPage.clear();
		flagHideGameOverPage.set();
	}

	void showPaddle()
	{
		flagHidePaddle.clear();
		flagShowPaddle.set();
	}

	void hidePaddle()
	{
		flagShowPaddle.clear();
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
			// Het hele scherm: het speelveld erboven, de peddelknoppen eronder.
			gamePanel.setPanelProps("gamePanel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(screenWidth, screenHeight), /*cornerRadius*/0, CoordType::Pixels,
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

			// De peddelknoppen staan in de onderste kwart, onder de doellijn.
			// Let op: locPos is de LINKERBOVENHOEK van de widget. De Alignment
			// hieronder geldt alleen voor de tekst binnen de knop, niet voor de
			// plek van de knop zelf.
			buttonLeft.setTouchScreenButtonProps
				("left",/*locPos*/Vec2(20, 790), CoordType::Promillage,
				/*size*/Vec2(250, 190), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::TopLeft,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "<-", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 0, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);

			buttonRight.setTouchScreenButtonProps
				("right",/*locPos*/Vec2(730, 790), CoordType::Promillage,
				/*size*/Vec2(250, 190), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::TopLeft,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "->", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 0, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);
		}

		void initGameOverPanels()
		{
			gameOverPanel.setPanelProps("gameOverPanel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
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

			buttonGameOverStart.setTouchScreenButtonProps
				("start",/*locPos*/Vec2(100, 562), CoordType::Promillage,
				/*size*/Vec2(800, 225), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::TopLeft,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "Click here to Start", /*buttonFont*/ 1, /*colFont*/ 0x0000FFFF, /*fontScale*/ 0, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);
		}

		void initStartPagePanels()
		{
			startPagePanel.setPanelProps("startPagePanel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(screenWidth, screenHeight), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00000000, /*colBg*/0x00000000);

			buttonStartPageStart.setTouchScreenButtonProps
				("start",/*locPos*/Vec2(100, 562), CoordType::Promillage,
				/*size*/Vec2(800, 225), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::TopLeft,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "Click here to Start", /*buttonFont*/ 1, /*colFont*/ 0x0000FFFF, /*fontScale*/ 0, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);
		}

		void initBallAndPaddlePosition()
		{
			// Bal en peddel eerst op hun nieuwe plek zetten, en pas
			// daarna tonen. Anders tekent show(true) ze nog even op de
			// plek van het vorige spel - en voor de bal is dat onder de
			// doellijn, buiten het speelveld. De moveTo() die daar
			// meteen op volgt herstelt die plek niet: buiten
			// gamePanel is er niets dat daar iets tekent, dus
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

		// Een pagina in en uit beeld. IWidget& en niet het paneeltype zelf: de
		// drie pagina's zijn Panel<8>, Panel<2> en Panel<1>, en show/hide/enable/
		// disable/setDrawable zitten allemaal in IWidget.
		//
		// Deze twee zijn gelijk aan die van de _dg-variant. Het verbergen tekent
		// niets - setDrawable(false) eerst, zodat Panel::hide() zijn wisvlak
		// overslaat. Hier is dat niet nodig om races te vermijden (er tekent maar
		// een taak), maar het scheelt wel de zwarte flits tussen twee pagina's in,
		// en het houdt de twee varianten vergelijkbaar.
		//
		// Het mag omdat elke pagina begint met een schermvullend paneel: de
		// volgende pagina wist wat de vorige achterlaat.
		void showPage(IWidget& page)
		{
			page.setDrawable(true, true);
			page.show(true);
			page.enable(true);
		}

		void hidePage(IWidget& page)
		{
			page.setDrawable(false, true);
			page.hide(true);
			page.disable(true);
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

			// De root moet zelf zichtbaar staan, anders tekent hij nooit iets.
			// redrawRegion() breekt namelijk meteen af op een onzichtbare widget,
			// en moveTo() laat juist via de root de vrijgekomen strook herstellen
			// en de bal opnieuw tekenen. Zonder deze regel verschijnt het beeld
			// wel (show() tekent rechtstreeks) maar beweegt er daarna niets meer.
			//
			// show(false) en niet show(true): true zou alle drie de pagina's
			// tegelijk tonen. De root zelf tekent niets - hij is een kale Widget -
			// dus dit zet alleen de vlag om.
			pageRoot.show(false);

			// Game Page toevoegen aan de pageRoot
			initGamePanels();
			pageRoot.addChildWidget(gamePanel);	// eerst, want alles hierna tekent erop.
			gamePanel.addChildWidget(bottomLinePanel);
			gamePanel.addChildWidget(leftBorderPanel);
			gamePanel.addChildWidget(rightBorderPanel);
			gamePanel.addChildWidget(topBorderPanel);
			gamePanel.addChildWidget(ballPanel);
			gamePanel.addChildWidget(paddlePanel);
			gamePanel.addChildWidget(buttonLeft);
			gamePanel.addChildWidget(buttonRight);
			hidePage(gamePanel);

			// Game Over Page toevoegen
			initGameOverPanels();
			pageRoot.addChildWidget(gameOverPanel);	// eerst, want alles hierna tekent erop.
			gameOverPanel.addChildWidget(gameOverLabelPanel);
			gameOverLabelPanel.addChildWidget(gameOverText);
			gameOverPanel.addChildWidget(buttonGameOverStart);
			hidePage(gameOverPanel);

			// Start Page toevoegen
			initStartPagePanels();
			pageRoot.addChildWidget(startPagePanel);	// eerst, want alles hierna tekent erop.
			startPagePanel.addChildWidget(buttonStartPageStart);
			hidePage(startPagePanel);
			// Geen showStartPage() hier: StartPageControl vraagt hem zo meteen zelf
			// op, net als in de _dg-variant.

			// Beeldtelling, om te kunnen nagaan of de 25 per seconde ook echt
			// gehaald worden in plaats van dat aan te nemen.
			int32_t nofFrames = 0;
			int64_t timeOfLastReportUs = esp_timer_get_time();

			// Geen toestand meer over welke pagina "de huidige" is: elke pagina
			// heeft zijn eigen show en hide, en de aanroeper houdt dat paar bij.
			// Dat maakt deze lus een platte lijst gebeurtenissen - en spiegelt de
			// drie losse DisplayControls van de _dg-variant.
			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				// Alles waar deze taak op reageert in een keer: wie hier niet in
				// staat wordt niet gewekt, en dan blijft zijn wens hangen.
				waitAny(flagShowStartPage + flagHideStartPage +
					    flagShowGamePage + flagHideGamePage +
					    flagShowGameOverPage + flagHideGameOverPage +
					    flagShowPaddle + flagHidePaddle +
					    flagBallPositionUpdated + flagPaddlePositionUpdated);

				if(hasFired(flagShowGamePage))
				{
					initBallAndPaddlePosition();
					showPage(gamePanel);
				}
				else if(hasFired(flagHideGamePage))
				{
					hidePage(gamePanel);
					// Een peddelwens van het vorige spel mag niet blijven staan:
					// PaddleControl zet flagHidePaddle op het moment dat het spel
					// eindigt, en die zou anders in het volgende spel de peddel
					// meteen weer laten verdwijnen.
					flagShowPaddle.clear();
					flagHidePaddle.clear();
				}
				else if(hasFired(flagShowStartPage))
				{
					showPage(startPagePanel);
				}
				else if(hasFired(flagHideStartPage))
				{
					hidePage(startPagePanel);
				}
				else if(hasFired(flagShowGameOverPage))
				{
					showPage(gameOverPanel);
				}
				else if(hasFired(flagHideGameOverPage))
				{
					hidePage(gameOverPanel);
				}
				else if(hasFired(flagShowPaddle))
				{
					paddlePanel.show(true);
					paddlePanel.enable(true);
					buttonLeft.show(true);
					buttonLeft.enable(true);
					buttonRight.show(true);
					buttonRight.enable(true);
				}
				else if(hasFired(flagHidePaddle))
				{
					paddlePanel.hide(true);
					paddlePanel.disable(true);
					buttonLeft.hide(true);
					buttonLeft.disable(true);
					buttonRight.hide(true);
					buttonRight.disable(true);
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
					//
					// Staat de gamepagina niet in beeld, dan verzet dit alleen de
					// coordinaten: moveTo() keert vroeg terug op een onzichtbare
					// widget.
					paddlePanel.moveTo(centreToTopLeftPix(paddlePosition, PaddleHeightPix/2, paddleSizePix()), CoordType::Pixels);
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
				}
			}
		}
	}; // end class DisplayControl

};// end namespace pong_cleangui_cg
