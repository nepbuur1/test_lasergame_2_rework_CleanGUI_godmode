// by Marius Versteegen, 2023

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <crt_CleanGUI.h>

//#include <crt_TFT_eSPI_DisplayAdapter.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_simple_frt
{
	// Zie psimf_BallControl.h: logger komt uit namespace crt, niet uit deze.

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

	// Wat de control-objecten van deze klasse mogen vragen, staat in het publieke
	// blok na de constructor. Hier stond een IDisplayControl die datzelfde rijtje
	// nog een keer opsomde - een interface met een implementatie, in hetzelfde
	// bestand dat de aanroepers toch al includeren.
	//
	// De template-parameters die hier stonden (<int WIDTH, int HEIGHT>) zijn ook
	// weg: ze werden nergens in de klasse gebruikt. Ze komen uit de console-versie
	// (apps/pong/pong/crt_SceneDisplayControl.h), waar ze de maat van een
	// CharMatrix bepalen; hier tekenen widgets het beeld en volgt elke maat uit de
	// constanten hierboven.
	class DisplayControl
	{

		// --- FreeRTOS (was CleanRTOS) ---
		static constexpr EventBits_t eventBitShowStartPage = (1 << 0);
		static constexpr EventBits_t eventBitHideStartPage = (1 << 1);
		static constexpr EventBits_t eventBitShowGamePage = (1 << 2);
		static constexpr EventBits_t eventBitHideGamePage = (1 << 3);
		static constexpr EventBits_t eventBitShowGameOverPage = (1 << 4);
		static constexpr EventBits_t eventBitHideGameOverPage = (1 << 5);
		static constexpr EventBits_t eventBitShowPaddle = (1 << 6);
		static constexpr EventBits_t eventBitHidePaddle = (1 << 7);
		static constexpr EventBits_t eventBitBallPositionUpdated = (1 << 8);
		static constexpr EventBits_t eventBitPaddlePositionUpdated = (1 << 9);
		EventGroupHandle_t eventGroup;
		EventBits_t firedBits;		// wat de laatste wachtoproep op meer tegelijk opleverde; zie hasFired()
		TaskHandle_t taskHandle;

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
		Panel<2> startPagePanel;
		Text<0> startPageTitle;		// "Pong" boven de knop; zie initStartPagePanels()
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonStartPageStart;

		QueueHandle_t ballPositionQueue;		// lengte 1: was Pool<TwoNumbers> poolBallPosition
		QueueHandle_t paddlePositionQueue;		// lengte 1: was Pool<int> poolPaddlePosition

	public:
		DisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, int frameDelayMs,
							IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts) :
			eventGroup(xEventGroupCreate()),
			firedBits(0),
			taskHandle(nullptr),
			display(display),
			freeFonts(freeFonts),
			screenWidth(0),
			screenHeight(0),
			width(0),
			height(0),
			frameDelayMs(frameDelayMs)
		{
			ballPositionQueue = xQueueCreate(1, sizeof(TwoNumbers));
			{ TwoNumbers initValue{}; xQueueOverwrite(ballPositionQueue, &initValue); }
			paddlePositionQueue = xQueueCreate(1, sizeof(int));
			{ int initValue{}; xQueueOverwrite(paddlePositionQueue, &initValue); }
			// getScreenSize() mag hier al: AsyncDisplay geeft de rotatie door in
			// zijn eigen constructor, dus de adapter kent de gedraaide maat al.
			// Het speelveld krijgt de bovenste driekwart van het scherm; de
			// onderste kwart is van de peddelknoppen.
			Vec2 screenSize = display.getScreenSize();
			screenWidth  = screenSize.x;
			screenHeight = screenSize.y;
			width  = screenWidth;
			height = (screenHeight * 3)/4;

			{ TwoNumbers poolValue = TwoNumbers(width/2, height/2); xQueueOverwrite(ballPositionQueue, &poolValue); }
			{ int poolValue = width/2; xQueueOverwrite(paddlePositionQueue, &poolValue); }

			// De knoppen moeten hun aanrakingen van het display krijgen. Dit is de
			// enige plek waar dat gebeurt: alle knoppen van alle pagina's wonen
			// hier. Uitgeschakelde knoppen negeren een aanraking zelf (zie
			// TouchscreenButton::touchPressed), dus alle vier mogen ze meteen mee.
			display.addTouchListener(&buttonLeft);
			display.addTouchListener(&buttonRight);
			display.addTouchListener(&buttonStartPageStart);
			display.addTouchListener(&buttonGameOverStart);

			xTaskCreatePinnedToCore(staticMain, taskName, taskSizeBytes, this, taskPriority, &taskHandle, taskCoreNumber);
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
		{ TwoNumbers poolValue = TwoNumbers(x,y); xQueueOverwrite(ballPositionQueue, &poolValue); }
		xEventGroupSetBits(eventGroup, eventBitBallPositionUpdated);
	}

	void updatePaddlePosition(int x)
	{
		{ int poolValue = x; xQueueOverwrite(paddlePositionQueue, &poolValue); }
		xEventGroupSetBits(eventGroup, eventBitPaddlePositionUpdated);
	}

	// Per pagina een paar: alleen de laatste wens telt, dus de tegenhanger gaat
	// leeg. Wat de andere pagina's doen staat hier los van - dat regelt de
	// aanroeper met zijn eigen show/hide.
	//
	// Elk control-object toont zijn eigen pagina aan het begin van zijn
	// do..()-functie en verbergt hem aan het eind; MainControl roept die strikt na
	// elkaar aan, dus er staat er nooit meer dan een in beeld.
	//
	// Bewust geen select..(): dat zou beloven dat het tonen van de ene pagina de
	// andere vanzelf uitzet, en dat doet het niet.
	void showStartPage()
	{
		xEventGroupClearBits(eventGroup, eventBitHideStartPage);
		xEventGroupSetBits(eventGroup, eventBitShowStartPage);
	}

	void hideStartPage()
	{
		xEventGroupClearBits(eventGroup, eventBitShowStartPage);
		xEventGroupSetBits(eventGroup, eventBitHideStartPage);
	}

	void showGamePage()
	{
		xEventGroupClearBits(eventGroup, eventBitHideGamePage);
		xEventGroupSetBits(eventGroup, eventBitShowGamePage);
	}

	void hideGamePage()
	{
		xEventGroupClearBits(eventGroup, eventBitShowGamePage);
		xEventGroupSetBits(eventGroup, eventBitHideGamePage);
	}

	void showGameOverPage()
	{
		xEventGroupClearBits(eventGroup, eventBitHideGameOverPage);
		xEventGroupSetBits(eventGroup, eventBitShowGameOverPage);
	}

	void hideGameOverPage()
	{
		xEventGroupClearBits(eventGroup, eventBitShowGameOverPage);
		xEventGroupSetBits(eventGroup, eventBitHideGameOverPage);
	}

	void showPaddle()
	{
		xEventGroupClearBits(eventGroup, eventBitHidePaddle);
		xEventGroupSetBits(eventGroup, eventBitShowPaddle);
	}

	void hidePaddle()
	{
		xEventGroupClearBits(eventGroup, eventBitShowPaddle);
		xEventGroupSetBits(eventGroup, eventBitHidePaddle);
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

			// De titel. Waar pong_super een plaatje heeft ("Super Pong"),
			// is het hier gewone tekst: deze variant gebruikt de RGB565-adapter en
			// heeft geen plaatjes. TopMid: met locPos (0, y) staat de regel
			// horizontaal gecentreerd op hoogte y (zie Text::calcOriginPix). Font 4
			// op schaal 2 is zo'n 52 pixels hoog, dus van y=40 tot ruim boven de knop
			// (die begint op 562 promille, op dit scherm ~135 pixels).
			startPageTitle.setTextProps
				(/*locPos*/Vec2(0, 40), CoordType::Pixels, Alignment::TopMid,
				"Pong", /*font*/4, /*color*/0x0000FFFF, /*fontScale*/2);

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
			// daarna tonen. Anders tekent redraw(true) ze nog even op de
			// plek van het vorige spel - en voor de bal is dat onder de
			// doellijn, buiten het speelveld. De moveTo() die daar
			// meteen op volgt herstelt die plek niet: buiten
			// gamePanel is er niets dat daar iets tekent, dus
			// bleef er een tweede bal staan.
			//
			// setLocPos en niet moveTo: er staat op dit moment nog
			// niets op het scherm, dus er valt niets te herstellen.
			TwoNumbers ballPosition;
			xQueuePeek(ballPositionQueue, &ballPosition, 0);
			ballPanel.setLocPos(centreToTopLeftPix(ballPosition.number1, ballPosition.number2, ballSizePix()),
								CoordType::Pixels, Alignment::MidMid);

			int paddlePosition;
			xQueuePeek(paddlePositionQueue, &paddlePosition, 0);
			paddlePanel.setLocPos(centreToTopLeftPix(paddlePosition, PaddleHeightPix/2, paddleSizePix()),
									CoordType::Pixels, Alignment::MidMid);

			xEventGroupClearBits(eventGroup, eventBitBallPositionUpdated);		// zojuist verwerkt; redraw(true) tekent ze.
			xEventGroupClearBits(eventGroup, eventBitPaddlePositionUpdated);
		}

		// Een pagina in en uit beeld. IWidget& en niet het paneeltype zelf: de
		// drie pagina's zijn Panel<8>, Panel<2> en Panel<1>. Los uitgeschreven
		// omdat show() en hide() geen interfacefuncties zijn; makeDrawable,
		// makeNonDrawable, redraw, enable en disable zijn dat wel.
		//
		// De enable() en de disable() dragen het hele aan/uit-gedrag van de
		// knoppen op deze pagina's: tekenbaar en aanraakbaar zijn losse assen, dus
		// tonen enablet niet mee en verbergen disabelt niet mee. Wie ze opruimt
		// laat de knoppen van een pagina die niet in beeld staat gewoon
		// doorwerken, en verliest de bPressed-reset van
		// TouchscreenButton::disable() - waarna een knop die onder je vinger
		// verdween voorgoed ingedrukt blijft staan.
		void showPage(IWidget& page)
		{
			page.makeDrawable(true);
			page.redraw(true);
			page.enable(true);
		}

		// Uit beeld en met opzet zonder ook maar een pixel te sturen - vandaar
		// makeNonDrawable en nadrukkelijk niet hide(). Die zou hier of niets doen
		// (op het moment van verbergen staat alles al op niet-tekenbaar) of, als
		// de volgende pagina al is getekend, die pagina schermvullend opnieuw
		// laten tekenen. Wissen hoeft sowieso niet: elke pagina begint met een
		// schermvullend paneel dat wist wat de vorige achterlaat.
		void hidePage(IWidget& page)
		{
			page.disable(true);
			page.makeNonDrawable(true);
		}

		static Vec2 ballSizePix()   { return Vec2(BallSizePix, BallSizePix); }
		static Vec2 paddleSizePix() { return Vec2(PaddleWidthPix, PaddleHeightPix); }


		// --- FreeRTOS-hulpfuncties ---
		static void staticMain(void* pvParameters)
		{
			static_cast<DisplayControl*>(pvParameters)->main();
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

#ifdef LOAD_GFXFF							  // Don't select font indices 0 and 1 - they are reserved.
			freeFonts.setFreeFont(10, FSB9);  // Free serif 9 pix font.
			freeFonts.setFreeFont(12, FSB12); // Free serif 12 pix font.
#endif

			PageRoot<20> pageRoot("rootSceneDisplay",display);

			// De root blijft tekenbaar - dat is hij bij constructie al, en zo moet
			// hij blijven: redrawRegion() breekt af op een niet-tekenbare widget,
			// en moveTo() laat juist via de root de vrijgekomen strook herstellen
			// en de bal opnieuw tekenen. Maak hem dus nooit niet-tekenbaar.
			//
			// Wat hier evenmin mag: show(true) op de root. Onder deze ene root
			// hangen alle drie de pagina's, en show() maakt de hele boom tekenbaar
			// - dus zouden ze alle drie over elkaar heen komen. redraw(true) mag
			// wel: die raakt geen vlag aan en kapt elke niet-tekenbare subboom af,
			// dus daar komt alleen de pagina uit die op dat moment aan staat. Per
			// pagina blijft duidelijker; daar is showPage voor.

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
			startPagePanel.addChildWidget(startPageTitle);
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

				// Alles waar deze taak op reageert in een keer: wie hier niet in
				// staat wordt niet gewekt, en dan blijft zijn wens hangen.
				firedBits = xEventGroupWaitBits(eventGroup, eventBitShowStartPage | eventBitHideStartPage | eventBitShowGamePage | eventBitHideGamePage | eventBitShowGameOverPage | eventBitHideGameOverPage | eventBitShowPaddle | eventBitHidePaddle | eventBitBallPositionUpdated | eventBitPaddlePositionUpdated, pdFALSE, pdFALSE, portMAX_DELAY);

				if(hasFired(eventBitShowGamePage))
				{
					initBallAndPaddlePosition();
					showPage(gamePanel);
				}
				else if(hasFired(eventBitHideGamePage))
				{
					hidePage(gamePanel);
					// Een peddelwens van het vorige spel mag niet blijven staan:
					// PaddleControl zet flagHidePaddle op het moment dat het spel
					// eindigt, en die zou anders in het volgende spel de peddel
					// meteen weer laten verdwijnen.
					xEventGroupClearBits(eventGroup, eventBitShowPaddle);
					xEventGroupClearBits(eventGroup, eventBitHidePaddle);
				}
				else if(hasFired(eventBitShowStartPage))
				{
					showPage(startPagePanel);
				}
				else if(hasFired(eventBitHideStartPage))
				{
					hidePage(startPagePanel);
				}
				else if(hasFired(eventBitShowGameOverPage))
				{
					showPage(gameOverPanel);
				}
				else if(hasFired(eventBitHideGameOverPage))
				{
					hidePage(gameOverPanel);
				}
				else if(hasFired(eventBitShowPaddle))
				{
					// Alleen als de spelpagina er ook werkelijk staat. show() is
					// makeDrawable + redraw, dus zonder deze test heft een
					// achtergebleven peddelwens de vlag op die hidePage(gamePanel)
					// net zette, en tekent de peddel dwars over de startpagina.
					// Vroeger kon dat niet: show() zette alleen een vlag en het
					// tekenen liep dood op !isDrawable().
					//
					// De twee zusterapps hebben deze test niet nodig - daar zit de
					// hele tak in een case STATE_SHOWN. Deze taak bedient drie
					// pagina's onder een root en heeft die toestand niet.
					if (gamePanel.isDrawable())
					{
						// De enable() staat er expliciet bij omdat show() niet meer
						// enablet. Dubbelop met de page.enable(true) van showPage(),
						// maar hij houdt deze tak op zichzelf kloppend, en het is de
						// plek waar TouchscreenButton::enable() een verouderde
						// bPressed weggooit.
						paddlePanel.show(true);
						paddlePanel.enable(true);
						buttonLeft.show(true);
						buttonLeft.enable(true);
						buttonRight.show(true);
						buttonRight.enable(true);
					}
				}
				else if(hasFired(eventBitHidePaddle))
				{
					// Hier wel hide() en geen makeNonDrawable: de gamepagina staat
					// op dit moment nog in beeld, dus peddel en knoppen moeten er
					// echt af. De boom tekent terug wat eronder zat.
					//
					// Eerst uitschakelen en dan pas weghalen: dat tekenwerk loopt
					// via de commandobuffer van AsyncDisplay, en in het venster
					// daartussen komt de touch-taak langs. Een knop die al van het
					// scherm is mag geen druk meer melden.
					paddlePanel.disable(true);
					paddlePanel.hide(true);
					buttonLeft.disable(true);
					buttonLeft.hide(true);
					buttonRight.disable(true);
					buttonRight.hide(true);
				}
				else if(hasFired(eventBitPaddlePositionUpdated))
				{
					int paddlePosition;
					xQueuePeek(paddlePositionQueue, &paddlePosition, 0);
					// moveTo() en niet setLocPos(): die laatste verzet alleen de
					// coordinaten en tekent niets, zodat de peddel op het scherm
					// blijft staan waar hij stond. moveTo() herstelt bovendien
					// alleen de strook die vrijkomt - dat is wat dit betaalbaar
					// maakt bij 25 beelden per seconde.
					//
					// Staat de gamepagina niet in beeld, dan verzet dit alleen de
					// coordinaten: moveTo() keert vroeg terug op een
					// niet-tekenbare widget.
					paddlePanel.moveTo(centreToTopLeftPix(paddlePosition, PaddleHeightPix/2, paddleSizePix()), CoordType::Pixels);
				}
				else if(hasFired(eventBitBallPositionUpdated))
				{
					TwoNumbers ballPosition;
					xQueuePeek(ballPositionQueue, &ballPosition, 0);
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

};// end namespace pong_simple_frt
