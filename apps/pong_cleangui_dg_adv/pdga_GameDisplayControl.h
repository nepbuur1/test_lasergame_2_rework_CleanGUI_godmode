// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include "images/_pal8/_allImages.h"

// De DisplayControl van de spelpagina: het speelveld, de bal, de peddel en de
// twee peddelknoppen, met een eigen PageRoot en een eigen taak.
//
// Zie README.txt: elke pagina heeft hier zijn eigen DisplayControl. Dat mag
// alleen omdat de PageRoots elkaar niet overlappen in de tijd - er is er steeds
// maar een getoond. Zie de opmerking boven crt_PageRoot.h, en hidePage()
// hieronder voor hoe dat wordt afgedwongen.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// Zie pdga_BallControl.h: logger komt uit namespace crt, niet uit deze.

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
	// header toch al voor IGameDisplayControl.
	const int FrameIntervalMs      = 40;	// 25 beelden per seconde.
	const int WallThicknessPix     = 10;
	const int BallSizePix          = 12;
	const int PaddleWidthPix       = 50;
	const int PaddleHeightPix      = 12;
	const int BallSpeedPixPerFrame = 4;		// 100 pixels per seconde.
	const int PaddleStepPix        = 6;		// 150 pixels per seconde zolang de knop ingedrukt blijft.

	// De maten hierboven staan ook in images/make_pong_images.py: daar bepalen ze
	// hoe groot de PNG's worden. Verander je er een, dan hoort dat script opnieuw
	// te draaien, gevolgd door tools/img2rgb565.py --pal8. Vergeet je dat, dan
	// zegt setImageChecked() hieronder het bij het opstarten - dat is precies
	// waarvoor die functie er is.
	//
	// Een plaatje op een panel zetten en meteen nagaan of het past. Te klein laat
	// Panel::needsBackground() de achtergrondkleur er alsnog onder schilderen; te
	// groot loopt over de buren heen. Allebei zie je op het bord alleen als "hij
	// tekent net niet goed", en niet als fout.
	// Een template omdat een TouchscreenButton ook een Panel is, maar er een met
	// een ander aantal kinderen - hij houdt zijn label als kind-widget.
	template <typename PANEL>
	inline void setImageChecked(PANEL& panel, const Image& image, const Vec2& sizePix, const char* strName)
	{
		if ((image.width != sizePix.x) || (image.height != sizePix.y))
		{
			ESP_LOGE("pong", "%s: plaatje is %dx%d maar het panel %dx%d - draai "
				"images/make_pong_images.py en tools/img2rgb565.py --pal8 opnieuw.",
				strName, (int)image.width, (int)image.height, (int)sizePix.x, (int)sizePix.y);
		}
		panel.setImage(&image);
	}

	class IGameDisplayControl
	{
		public:
			virtual int getWidth() = 0;
			virtual int getHeight() = 0;
			virtual IButton* addLeftButtonListener(IButtonListener* pButtonListener) = 0;
			virtual IButton* addRightButtonListener(IButtonListener* pButtonListener) = 0;
			virtual void updateBallPosition(int x, int y) = 0;
			virtual void updatePaddlePosition(int x) = 0;
			virtual void showGamePage() = 0;
			virtual void hideGamePage() = 0;
			virtual void showPaddle() = 0;
			virtual void hidePaddle() = 0;
	};

	template <int WIDTH, int HEIGHT>
	class GameDisplayControl : public Task, public IGameDisplayControl
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

		// Tekent deze display in een backbuffer? Bepaalt hoe een beeld tot stand
		// komt - zie updateBallPosition-afhandeling in main(). Pas in main() te
		// beantwoorden: isBuffered() maakt de buffer zo nodig aan, en dat hoort
		// niet in een constructor die voor het opstarten van de andere taken uit
		// loopt.
		bool bDisplayIsBuffered;

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
			state (STATE_HIDDEN),
			bDisplayIsBuffered(false)
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

			// Bal, peddel, wanden en doellijn komen alle zes uit een PAL8-plaatje
			// (zie images/). De cornerRadius is daarom weg: een afgeronde hoek
			// tekent de PAL8-adapter toch vierkant, en de ronding zit nu in het
			// plaatje - inclusief het verloop en het hoogsel, die geen enkele
			// fillRect kan maken.
			//
			// De colFg-kleuren blijven staan. Ze worden bij deze adapter niet
			// gebruikt (needsBackground() is onwaar zodra er een plaatje op zit),
			// maar zodra iemand de typedef in de ino terugzet naar RGB565 en de
			// setImage-regels hieronder weghaalt, staat het spel er weer zoals het
			// was.
			ballPanel.setPanelProps("ballPanel", /*locPos*/centreToTopLeftPix(width/2, height/2, ballSizePix()), CoordType::Pixels,
				/*size*/ballSizePix(), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colFg*/0x00FFFF00, /*colBg*/0x00000000);
			setImageChecked(ballPanel, imageBall, ballSizePix(), "ballPanel");

			paddlePanel.setPanelProps("paddlePanel", /*locPos*/centreToTopLeftPix(width/2, PaddleHeightPix/2, paddleSizePix()), CoordType::Pixels,
				/*size*/paddleSizePix(), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colFg*/0x0000FFff, /*colBg*/0x00000000);
			setImageChecked(paddlePanel, imagePaddle, paddleSizePix(), "paddlePanel");

			// De wanden sluiten het speelveld links, rechts en boven af. Hun dikte
			// is waar BallControl tegenaan kaatst.
			leftBorderPanel.setPanelProps("leftBorderPanel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(WallThicknessPix, height), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x0000FFff, /*colBg*/0x00000000);
			setImageChecked(leftBorderPanel, imageBorderLeft, Vec2(WallThicknessPix, height), "leftBorderPanel");

			rightBorderPanel.setPanelProps("rightBorderPanel", /*locPos*/Vec2(width - WallThicknessPix, 0), CoordType::Pixels,
				/*size*/Vec2(WallThicknessPix, height), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x0000FFff, /*colBg*/0x00000000);
			setImageChecked(rightBorderPanel, imageBorderRight, Vec2(WallThicknessPix, height), "rightBorderPanel");

			topBorderPanel.setPanelProps("topBorderPanel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(width, WallThicknessPix), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x0000FFff, /*colBg*/0x00000000);
			setImageChecked(topBorderPanel, imageBorderTop, Vec2(width, WallThicknessPix), "topBorderPanel");

			// Onderaan zit geen wand maar de doellijn: daar gaat de bal eruit.
			bottomLinePanel.setPanelProps("bottomLinePanel", /*locPos*/Vec2(0, height - 2), CoordType::Pixels,
				/*size*/Vec2(width, 2), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00404040, /*colBg*/0x00000000);
			setImageChecked(bottomLinePanel, imageGoalLine, Vec2(width, 2), "bottomLinePanel");

			// De peddelknoppen staan in de onderste kwart, onder de doellijn.
			// Let op: locPos is de LINKERBOVENHOEK van de widget. De Alignment
			// hieronder geldt alleen voor het plaatje binnen de knop, niet voor de
			// plek van de knop zelf.
			//
			// In pixels en niet meer in promillage, om dezelfde reden als bij de
			// PLAY-knop - zie initPlayButton() in pdga_PlayButton.h: 190 promille
			// van 240 is 45,6 pixel, en een panel dat daardoor een pixel groter
			// uitvalt dan het plaatje krijgt van Panel::needsBackground() de vlakke
			// rode kleur er alsnog onder geschilderd. Het label is leeg omdat de
			// pijl in het plaatje zit; de tekst "<-" die er stond was op deze maat
			// niet meer dan twee streepjes.
			Vec2 paddleButtonSizePix(imageButtonPaddleLeft.width, imageButtonPaddleLeft.height);
			const int paddleButtonTopPix = screenHeight - paddleButtonSizePix.y - 4;
			const int paddleButtonMarginPix = 6;

			buttonLeft.setTouchScreenButtonProps
				("left",/*locPos*/Vec2(paddleButtonMarginPix, paddleButtonTopPix), CoordType::Pixels,
				/*size*/paddleButtonSizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 1, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);
			setImageChecked(buttonLeft, imageButtonPaddleLeft, paddleButtonSizePix, "buttonLeft");

			buttonRight.setTouchScreenButtonProps
				("right",/*locPos*/Vec2(screenWidth - paddleButtonSizePix.x - paddleButtonMarginPix, paddleButtonTopPix), CoordType::Pixels,
				/*size*/paddleButtonSizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 1, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);
			setImageChecked(buttonRight, imageButtonPaddleRight, paddleButtonSizePix, "buttonRight");
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
		// moet zelfs: redrawRegion() breekt af op een onzichtbare widget, en
		// moveTo() laat de bal juist via de root opnieuw tekenen. Dat de root
		// meteen ook alle kinderen toont is precies de bedoeling, want deze root
		// draagt maar een pagina. (In een opzet met drie pagina's onder een
		// gedeelde root zou show(true) ze alle drie over elkaar heen tekenen.)
		void showPage(PageRoot<20>& page)
		{
			initBallAndPaddlePosition();
			page.setDrawable(true, true);
			page.show(true);
			page.enable(true);
		}

		// Uit beeld, en met opzet ZONDER iets te tekenen: setDrawable(false) eerst,
		// zodat Panel::hide() zijn wisvlak overslaat.
		//
		// Dat is wat deze opzet met meerdere tekentaken werkbaar houdt. Zou het
		// verbergen wel tekenen, dan zet deze taak een zwart vlak in de
		// commandobuffer terwijl de taak van de volgende pagina daar al zijn
		// tekenwerk in schuift - en welke van de twee er als laatste uitkomt ligt
		// niet vast. Dan wist het ene beeld het andere.
		//
		// Het mag omdat elke pagina begint met een schermvullend backgroundPanel:
		// de volgende pagina wist wat hier blijft staan. Blijft die eis overeind,
		// dan is de wissel bovendien mooier - geen zwarte flits tussen twee
		// pagina's in.
		void hidePage(PageRoot<20>& page)
		{
			page.setDrawable(false, true);
			page.hide(true);
			page.disable(true);

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

			// Eenmalig vragen wat voor display dit is; het antwoord bepaalt hoe
			// hieronder een beeld tot stand komt.
			//
			// Rechtstreeks op het paneel is elke pixel bustijd, dus verzet je
			// alleen wat er beweegt: moveTo() tekent de bal op zijn nieuwe plek en
			// herstelt de strook die vrijkomt.
			//
			// Met een backbuffer is dat juist averechts. Tekenen in geheugen is
			// bijna gratis, maar het uitsturen kost altijd het hele scherm - ~31
			// ms, ongeacht hoeveel er veranderde. Dan is de hele boom opnieuw
			// opbouwen en er een present() overheen doen goedkoper dan het slim
			// proberen te doen, en het is bovendien flikkervrij: er komt pas iets
			// op het scherm als het beeld af is. Zelfde afweging als in
			// CleanGUI_for_TFT_eSPI/examples/MultiPanel.
			bDisplayIsBuffered = display.isBuffered();
			ESP_LOGI(taskName, "display %s een backbuffer; %s",
				bDisplayIsBuffered ? "heeft" : "heeft geen",
				bDisplayIsBuffered ? "de hele pagina wordt per beeld hertekend"
				                   : "bal en peddel bewegen met moveTo");

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
							paddlePanel.setDrawable(true, true);
							buttonLeft.setDrawable(true, true);
							buttonRight.setDrawable(true, true);

							paddlePanel.show(true);
							paddlePanel.enable(true);
							buttonLeft.show(true);
							buttonLeft.enable(true);
							buttonRight.show(true);
							buttonRight.enable(true);
						}
						else if(hasFired(flagHidePaddle))
						{
							if (bDisplayIsBuffered)
							{
								// setDrawable(false) en niet hide(): het eerstvolgende
								// beeld bouwt de hele boom opnieuw op en zou hem dan
								// gewoon terugzetten. Wissen hoeft er niet bij - wat
								// niet meer wordt getekend, staat er in het volgende
								// beeld ook niet meer.
								paddlePanel.setDrawable(false, true);
								buttonLeft.setDrawable(false, true);
								buttonRight.setDrawable(false, true);
							}
							else
							{
								paddlePanel.hide(true);
								buttonLeft.hide(true);
								buttonRight.hide(true);
							}

							paddlePanel.disable(true);
							buttonLeft.disable(true);
							buttonRight.disable(true);
						}
						else if(hasFired(flagPaddlePositionUpdated))
						{
							int paddlePosition;
							poolPaddlePosition.read(paddlePosition);
							Vec2 paddleTopLeftPix = centreToTopLeftPix(paddlePosition, PaddleHeightPix/2, paddleSizePix());

							if (bDisplayIsBuffered)
							{
								// Alleen verzetten, niet tekenen. Het beeld dat de bal
								// zo meteen laat maken neemt de peddel mee; er nu al
								// een eigen beeld aan wijden zou het scherm twee keer
								// uitsturen voor een beeld, en dat halveert de
								// beeldsnelheid.
								paddlePanel.setLocPos(paddleTopLeftPix, CoordType::Pixels, Alignment::MidMid);
							}
							else
							{
								// moveTo() en niet setLocPos(): die laatste verzet alleen de
								// coordinaten en tekent niets, zodat de peddel op het scherm
								// blijft staan waar hij stond. moveTo() herstelt bovendien
								// alleen de strook die vrijkomt - dat is wat dit betaalbaar
								// maakt bij 25 beelden per seconde.
								paddlePanel.moveTo(paddleTopLeftPix, CoordType::Pixels);
							}
						}
						else if(hasFired(flagBallPositionUpdated))
						{
							TwoNumbers ballPosition;
							poolBallPosition.read(ballPosition);
							Vec2 ballTopLeftPix = centreToTopLeftPix(ballPosition.number1, ballPosition.number2, ballSizePix());

							if (bDisplayIsBuffered)
							{
								ballPanel.setLocPos(ballTopLeftPix, CoordType::Pixels, Alignment::MidMid);

								// De peddel er meteen bij. De twee vlaggen worden in
								// deze if/else-keten om beurten afgehandeld, dus zonder
								// dit zou de peddel steeds een beeld achterlopen op wat
								// PaddleControl al doorgaf.
								int paddlePosition;
								poolPaddlePosition.read(paddlePosition);
								paddlePanel.setLocPos(centreToTopLeftPix(paddlePosition, PaddleHeightPix/2, paddleSizePix()),
													  CoordType::Pixels, Alignment::MidMid);

								// Dit is het beeld. AsyncDisplay stuurt de buffer uit
								// zodra zijn commandoketen leeg is, dus een present()
								// hoeft hier niet - zie de auto-present onderaan
								// AsyncDisplay::main().
								pageGame.show(true);
							}
							else
							{
								ballPanel.moveTo(ballTopLeftPix, CoordType::Pixels);
							}

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

};// end namespace pong_cleangui_dg_adv
