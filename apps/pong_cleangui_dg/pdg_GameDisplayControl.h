// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

// De DisplayControl van de spelpagina: het speelveld, de bal, de peddel en de
// twee peddelknoppen, met een eigen PageRoot en een eigen taak.
//
// Zie README.txt: elke pagina heeft hier zijn eigen DisplayControl. Dat mag
// alleen omdat de PageRoots elkaar niet overlappen in de tijd - er is er steeds
// maar een getoond. Zie de opmerking boven crt_PageRoot.h, en hidePage()
// hieronder voor hoe dat wordt afgedwongen.

using namespace crt;
namespace pong_cleangui_dg
{
	// Zie pdg_BallControl.h: logger komt uit namespace crt, niet uit deze.

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
	// loopt omhoog. GameDisplayControl is de enige die dat naar schermpixels
	// omrekent (het scherm telt van boven).
	//
	// Ze staan in deze header en niet in een eigen bestand omdat ze de spelpagina
	// beschrijven: BallControl en PaddleControl rekenen ermee en includeren deze
	// header toch al voor GameDisplayControl.
	const int FrameIntervalMs      = 40;	// 25 beelden per seconde.
	const int WallThicknessPix     = 10;
	const int BallSizePix          = 12;
	const int PaddleWidthPix       = 50;
	const int PaddleHeightPix      = 12;
	const int BallSpeedPixPerFrame = 4;		// 100 pixels per seconde.
	const int PaddleStepPix        = 6;		// 150 pixels per seconde zolang de knop ingedrukt blijft.

	// Wat de control-objecten van deze pagina mogen vragen, staat in het publieke
	// blok na de constructor. Zie pdg_StartPageDisplayControl.h voor waarom daar
	// geen interface meer boven staat.
	//
	// De template-parameters die hier stonden (<int WIDTH, int HEIGHT>) zijn ook
	// weg: ze werden nergens in de klasse gebruikt. Ze komen uit de console-versie
	// (apps/pong/pong/crt_SceneDisplayControl.h), waar ze de maat van een
	// CharMatrix bepalen; hier tekenen widgets het beeld en volgt elke maat uit de
	// constanten hierboven.
	class GameDisplayControl : public Task
	{
		enum State
		{
			STATE_HIDDEN,
			STATE_SHOWN
		};

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

		// De panels krijgen hun maat en plek pas in main(), zodra de veldmaat
		// bekend is. Ze hier al invullen zou betekenen dat de initialisatielijst
		// leden leest die nog niet bestaan.
		//
		// backgroundPanel beslaat het HELE scherm, niet alleen het speelveld. Twee
		// redenen: de peddelknoppen zitten in de onderste kwart en horen bij deze
		// pagina, en moveTo() laat de vrijgekomen strook door de boom opnieuw
		// tekenen - zonder iets dat daar wel wat tekent blijft de oude bal staan.
		// Dat laatste geldt ook onder de doellijn, waar de bal aan het eind van
		// een spel verdwijnt.
		Panel<0> backgroundPanel;
		Panel<0> ballPanel;
		Panel<0> bottomLinePanel;
		Panel<0> leftBorderPanel;
		Panel<0> rightBorderPanel;
		Panel<0> topBorderPanel;
		Panel<0> paddlePanel;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonLeft;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonRight;

		Pool<TwoNumbers> poolBallPosition;
		Pool<int> poolPaddlePosition;
		Flag flagShowGamePage;
		Flag flagDeshowGamePage;
		Flag flagShowPaddle;
		Flag flagHidePaddle;
		Flag flagBallPositionUpdated;
		Flag flagPaddlePositionUpdated;

		State state;

	public:
		GameDisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, int frameDelayMs,
							IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
			display(display),
			freeFonts(freeFonts),
			screenWidth(0),
			screenHeight(0),
			width(0),
			height(0),
			frameDelayMs(frameDelayMs),
			flagShowGamePage(this),
			flagDeshowGamePage(this),
			flagShowPaddle(this),
			flagHidePaddle(this),
			flagBallPositionUpdated(this),
			flagPaddlePositionUpdated(this),
			state (STATE_HIDDEN)
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

			// De knoppen van deze pagina moeten hun aanrakingen van het display
			// krijgen. Een uitgeschakelde knop negeert een aanraking zelf (zie
			// TouchscreenButton::touchPressed), dus ze mogen meteen mee.
			display.addTouchListener(&buttonLeft);
			display.addTouchListener(&buttonRight);

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

	void showGamePage()
	{
		flagDeshowGamePage.clear();	// alleen de laatste wens telt.
		flagShowGamePage.set();
	}

	void hideGamePage()
	{
		flagShowGamePage.clear();
		flagDeshowGamePage.set();
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
		void initPanels()
		{
			// Het hele scherm: het speelveld erboven, de peddelknoppen eronder.
			backgroundPanel.setPanelProps("gameBackground", /*locPos*/Vec2(0, 0), CoordType::Pixels,
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

		void initBallAndPaddlePosition()
		{
			// Bal en peddel eerst op hun nieuwe plek zetten, en pas daarna tonen.
			// Anders tekent show(true) ze nog even op de plek van het vorige spel -
			// en voor de bal is dat onder de doellijn.
			//
			// setLocPos en niet moveTo: er staat op dit moment nog niets van deze
			// pagina op het scherm, dus er valt niets te herstellen.
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

		// De hele pagina in beeld. show(true) op de PageRoot zelf mag hier - en
		// moet zelfs: hidePage() laat de hele boom niet-tekenbaar achter, en
		// zowel redrawRegion() als moveTo() breken af op een niet-tekenbare
		// widget. Alleen redraw(true) zou hier dus niets doen, en daarna zou ook
		// de bal nergens meer verschijnen. Dat show(true) alle kinderen meeneemt
		// is hier precies de bedoeling: deze root draagt maar een pagina en er
		// staat niets met opzet uit. (In een opzet met drie pagina's onder een
		// gedeelde root zou show(true) ze alle drie over elkaar heen tekenen.)
		void showPage(PageRoot<20>& page)
		{
			initBallAndPaddlePosition();
			page.show(true);
			page.enable(true);		// show() enablet niet mee; zonder deze regel blijft de pagina doof.
		}

		// Uit beeld, en met opzet ZONDER iets te tekenen: makeNonDrawable() zet
		// alleen de tekenvlag om. Een hide() hoort hier juist NIET: die laat de
		// boom vanaf de wortel over het vrijgekomen vlak gaan, en dat tekenwerk
		// is precies wat hier niet mag ontstaan.
		//
		// Dat is wat deze opzet met meerdere tekentaken werkbaar houdt. Zou het
		// verbergen wel tekenen, dan schuift deze taak dat tekenwerk in de
		// commandobuffer terwijl de taak van de volgende pagina daar al zijn
		// eigen beeld in zet - en welke van de twee er als laatste uitkomt ligt
		// niet vast. Dan wist het ene beeld het andere.
		//
		// Het mag omdat elke pagina begint met een schermvullend backgroundPanel:
		// de volgende pagina wist wat hier blijft staan. Blijft die eis overeind,
		// dan is de wissel bovendien mooier - geen zwarte flits tussen twee
		// pagina's in.
		void hidePage(PageRoot<20>& page)
		{
			page.disable(true);		// een niet-tekenbare knop meldt nog gewoon drukken.
			page.makeNonDrawable(true);

			// Een peddelwens van het vorige spel mag niet blijven staan:
			// PaddleControl zet flagHidePaddle op het moment dat het spel eindigt,
			// en die zou anders in het volgende spel de peddel meteen weer laten
			// verdwijnen.
			flagShowPaddle.clear();
			flagHidePaddle.clear();
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
			initPanels();

			PageRoot<20> pageGame("rootGameDisplay",display);
			pageGame.addChildWidget(backgroundPanel);	// eerst, want alles hierna tekent erop.
			pageGame.addChildWidget(bottomLinePanel);
			pageGame.addChildWidget(leftBorderPanel);
			pageGame.addChildWidget(rightBorderPanel);
			pageGame.addChildWidget(topBorderPanel);
			pageGame.addChildWidget(ballPanel);
			pageGame.addChildWidget(paddlePanel);
			pageGame.addChildWidget(buttonLeft);
			pageGame.addChildWidget(buttonRight);

			hidePage(pageGame);		// het spel begint op de startpagina.

			// Beeldtelling, om te kunnen nagaan of de 25 per seconde ook echt
			// gehaald worden in plaats van dat aan te nemen.
			int32_t nofFrames = 0;
			int64_t timeOfLastReportUs = esp_timer_get_time();

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.
				switch(state)
				{
					case STATE_SHOWN:
					{
						waitAny(flagShowGamePage + flagDeshowGamePage +
							    flagShowPaddle + flagHidePaddle +
							    flagBallPositionUpdated + flagPaddlePositionUpdated);

						if(hasFired(flagDeshowGamePage))
						{
							hidePage(pageGame);
							state = STATE_HIDDEN;
						}
						else if(hasFired(flagShowGamePage))
						{
							// al in beeld: niets te doen.
						}
						else if(hasFired(flagShowPaddle))
						{
							// De enable() bij de twee knoppen is dragend: tekenen en
							// aanraakbaarheid zijn losse assen, dus show() zet ze alleen
							// terug op het scherm. Zonder die twee blijft de peddel staan,
							// want hij beweegt uitsluitend op knopdruk en
							// TouchscreenButton::touchPressed keert terug bij !isEnabled().
							// Bij paddlePanel staat hij er voor de symmetrie; een Panel
							// leest zijn _bEnabled nergens.
							paddlePanel.show(true);
							paddlePanel.enable(true);
							buttonLeft.show(true);
							buttonLeft.enable(true);
							buttonRight.show(true);
							buttonRight.enable(true);
						}
						else if(hasFired(flagHidePaddle))
						{
							// Disable voor hide, niet erna: hide() tekent, en dat loopt via
							// de commandobuffer van AsyncDisplay. Staat de disable erachter,
							// dan kan de touch-taak in dat gaatje nog een druk melden op een
							// knop die al van het scherm is. TouchscreenButton::disable()
							// gooit meteen een half afgemaakte druk weg.
							paddlePanel.disable(true);
							paddlePanel.hide(true);
							buttonLeft.disable(true);
							buttonLeft.hide(true);
							buttonRight.disable(true);
							buttonRight.hide(true);
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
						break;
					}

					case STATE_HIDDEN:
					{
						// Ook hier op beide vlaggen wachten: wie er niet in staat
						// wordt niet gewekt, en dan blijft een wens onopgemerkt
						// hangen.
						waitAny(flagShowGamePage + flagDeshowGamePage);

						if(hasFired(flagShowGamePage))
						{
							showPage(pageGame);
							state = STATE_SHOWN;
						}
						else if(hasFired(flagDeshowGamePage))
						{
							// al uit beeld: niets te doen.
						}
						break;
					}
					default:
						assert(false);
				}
			}
		}
	}; // end class GameDisplayControl

};// end namespace pong_cleangui_dg
