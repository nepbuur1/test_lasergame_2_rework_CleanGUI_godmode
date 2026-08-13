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

// Meetgereedschap, normaal uit. Aan: elke seconde de spreiding in de beeldtijd
// en het aantal overgeslagen spelstanden, en per verbrande bal zijn valsnelheid;
// plus een regel bij elk gespawnd of geopend object. Daarmee zijn de vragen
// "loopt de klok goed", "houdt het tekenen het bij" en "wat gebeurt er in het
// veld" af te lezen in plaats van te beredeneren.
//
// Uit, omdat het geld kost waar het gemeten wordt: een logregel is bij 115200
// baud zo'n 7 ms, en de spawn-regels lopen in de taak waarin ook de beeldklok
// zit - een kwart van een beeld.
//#define PONG_MEASURE

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// Zie pdga_BallControl.h: logger komt uit namespace crt, niet uit deze.

	// Hoeveel ballen er tegelijk in de lucht kunnen zijn. Zie UC07: een present
	// crate spawnt er tien ineens. Ze liggen geprealloceerd in een Depot (zie
	// pdga_Depot.h) en hebben elk een panel in de array hieronder, op dezelfde
	// index.
	const int MaxNofBalls = 50;

	// Alles wat het tekenen van EEN bal nodig heeft. Het brandende bit reist met
	// de positie mee en niet als losse vlag: de twee horen bij elkaar, en apart
	// verstuurd kunnen ze in de verkeerde volgorde aankomen - dan vlamt de bal een
	// beeld te vroeg of te laat.
	struct BallState
	{
		BallState():posX(0),posY(0),bActive(false),bBurning(false),indexBurnAnimationStep(0){}
		BallState(int posX, int posY, bool bBurning, int32_t indexBurnAnimationStep)
			:posX(posX),posY(posY),bActive(true),bBurning(bBurning),indexBurnAnimationStep(indexBurnAnimationStep){}

		int posX;
		int posY;

		// Ligt dit slot in het spel? Een slot dat niet in gebruik is levert een
		// BallState met bActive false op, en daar zet de display het panel op
		// niet-tekenbaar. Zo hoeft de bal die zichzelf vrijgeeft geen apart
		// bericht te sturen om zijn panel te laten verdwijnen.
		bool bActive;

		bool bBurning;

		// Hoeveel stappen deze bal al brandt. De display maakt er een cel van de
		// spritesheet van; hoeveel cellen die heeft blijft kennis van de display.
		int32_t indexBurnAnimationStep;
	};

	// Hoeveel present crates er tegelijk in het veld kunnen staan, hoe groot ze
	// zijn en hoeveel treffers ze verdragen. Zie UC04 en UC07: elke drie seconden
	// dat er geen bal de onderlijn passeert komt er een bij, en na NofCrateHits
	// balaanrakingen gaat hij open en levert hij NofBallsPerCrate ballen op.
	const int MaxNofCrates     = 5;
	const int CrateSizePix     = 28;
	const int NofCrateHits     = 5;
	const int NofBallsPerCrate = 10;

	// Wat het tekenen van EEN present crate nodig heeft.
	struct CrateState
	{
		CrateState():posX(0),posY(0),bActive(false),nofHitsLeft(0){}
		CrateState(int posX, int posY, int nofHitsLeft)
			:posX(posX),posY(posY),bActive(true),nofHitsLeft(nofHitsLeft){}

		int posX;
		int posY;
		bool bActive;

		// Hoeveel treffers er nog nodig zijn. De display kiest daarmee de cel van
		// de spritesheet: cel 0 is gaaf en toont NofCrateHits, elke treffer schuift
		// een cel op. Zo staat het aftellen in het plaatje zelf.
		int nofHitsLeft;
	};

	struct CrateStates
	{
		CrateState arCrate[MaxNofCrates];
	};

	// De vuurkorf (UC08). Hij staat FireBinLifetimeMs in het veld en verdwijnt dan
	// weer; een bal die hem raakt brandt op in BallBurnUpMs (UC05 stap 1d).
	//
	// De kans dat het beslismoment van UC04 een vuurkorf oplevert in plaats van
	// een present crate is evenredig met het aantal ballen in het veld: bij
	// NofBallsForCertainFireBin of meer is het altijd een vuurkorf.
	const int MaxNofFireBins            = 5;
	const int FireBinSizePix            = 28;
	const int FireBinLifetimeMs         = 3000;
	const int BallBurnUpMs              = 4000;
	const int NofBallsForCertainFireBin = 40;

	struct FireBinState
	{
		FireBinState():posX(0),posY(0),bActive(false),indexAnimationStep(0){}
		FireBinState(int posX, int posY, int32_t indexAnimationStep)
			:posX(posX),posY(posY),bActive(true),indexAnimationStep(indexAnimationStep){}

		int posX;
		int posY;
		bool bActive;

		// Hoe lang de korf al brandt. Anders dan bij de crate is dit geen teller
		// maar een animatiefase: de display maakt er cyclisch een cel van, zodat
		// het vuur doorbrandt zolang de korf er staat.
		int32_t indexAnimationStep;
	};

	struct FireBinStates
	{
		FireBinState arFireBin[MaxNofFireBins];
	};

	// De score staat tussen de twee peddelknoppen, als vier losse panels die elk
	// een cel uit de cijfersheet tonen. Geen gerenderde tekst: dat scheelt
	// tekentijd, en vaste celbreedtes houden de score stil staan - met een
	// proportioneel font wiebelt hij zodra er een 1 in komt.
	const int NofScoreDigits      = 4;
	const int ScoreDigitWidthPix  = 28;
	const int ScoreDigitHeightPix = 36;

	// Alle ballen in een bericht.
	//
	// Een array en niet vijftig losse berichten, om twee redenen. Pool::write()
	// neemt zijn argument op waarde aan, dus vijftig schrijvers zouden vijftig
	// keer de hele boel kopieren. En een Flag per bal kan niet: crt_Task.h doet
	// assert(nofWaitables < 24), dus een taak kan er hooguit 24 bezitten.
	struct BallStates
	{
		BallStates():indexFrame(0){}

		BallState arBall[MaxNofBalls];

		// Doorlopend nummer van het beeld, om te kunnen zien of er een stand niet
		// is getekend. Zie nofSkippedSteps in GameDisplayControl.
		int32_t indexFrame;
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
	const int PaddleStepPix        = 8;		// 150 pixels per seconde zolang de knop ingedrukt blijft.

	// De levensduren uit UC08 en UC05 in beeldstappen, want daarin rekenen de
	// spelobjecten. Hier en niet bij de constanten hierboven, omdat ze
	// FrameIntervalMs nodig hebben.
	const int NofFireBinSteps    = FireBinLifetimeMs / FrameIntervalMs;
	const int NofBallBurnUpSteps = BallBurnUpMs / FrameIntervalMs;

	// De brandende bal krimpt terwijl hij opbrandt: een seconde op volle grootte,
	// een seconde half, een seconde een kwart. Elk stadium heeft een eigen cyclus
	// van NofFramesPerBurnStage cellen in de spritesheet, dus die telt
	// 1 + NofBurnStages * NofFramesPerBurnStage cellen - de eerste is de gewone
	// bal.
	//
	// Een bal die onder de doellijn door valt is binnen 700 ms weg en komt dus
	// nooit verder dan het eerste stadium. Dat hoeft nergens apart geregeld te
	// worden; het volgt uit de tijd.
	const int NofBurnStages         = 3;
	const int NofFramesPerBurnStage = 5;
	const int NofStepsPerBurnStage  = (BallBurnUpMs / NofBurnStages) / FrameIntervalMs;

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
		// De cel telt, niet het hele plaatje: bij een spritesheet is het panel zo
		// groot als een frame.
		int32_t frameWidth  = image.width  / ((image.nofCols > 0) ? image.nofCols : 1);
		int32_t frameHeight = image.height / ((image.nofRows > 0) ? image.nofRows : 1);

		if ((frameWidth != sizePix.x) || (frameHeight != sizePix.y))
		{
			ESP_LOGE("pong", "%s: plaatje is %dx%d maar het panel %dx%d - draai "
				"images/make_pong_images.py en tools/img2rgb565.py --pal8 opnieuw.",
				strName, (int)frameWidth, (int)frameHeight, (int)sizePix.x, (int)sizePix.y);
		}
		panel.setImage(&image);
	}

	// Wat er vast op de spelpagina staat: achtergrond, doellijn, drie wanden,
	// peddel, twee peddelknoppen en het GAME OVER-vignet. Daar komen de balpanels,
	// de crates, de vuurkorven en de vier scorecijfers bij.
	//
	// Deze telling moet kloppen, en dat merk je NIET vanzelf: een te krappe
	// PageRoot laat addChildWidget() het laatste kind weigeren met een ESP_LOGI en
	// verder gewoon doorlopen. Er crasht niets, er ontbreekt alleen stilzwijgend
	// een widget - en omdat de pagina bij het opstarten wordt gebouwd, is die
	// melding allang van het scherm als je het merkt. Het vignet is er precies zo
	// een keer uitgevallen.
	const int NofFixedGameWidgets = 9;

	typedef PageRoot<NofFixedGameWidgets + MaxNofBalls + MaxNofCrates + MaxNofFireBins + NofScoreDigits> GamePageRoot;

	class IGameDisplayControl
	{
		public:
			virtual int getWidth() = 0;
			virtual int getHeight() = 0;
			virtual IButton* addLeftButtonListener(IButtonListener* pButtonListener) = 0;
			virtual IButton* addRightButtonListener(IButtonListener* pButtonListener) = 0;
			// Een beeld: alle ballen en alle crates in een keer, met een enkele
			// vlag erachter. Twee losse berichten zouden twee hertekeningen kunnen
			// opleveren, en een present kost het hele scherm.
			virtual void updateGameObjects(const BallStates& ballStates, const CrateStates& crateStates,
			                               const FireBinStates& fireBinStates, int32_t score) = 0;

			// De laagste speelveld-y waarop nog iets van de bal te zien is. Onder
			// deze waarde is hij het scherm uit. Deze klasse beantwoordt dat en
			// niet BallControl, omdat het antwoord van de spritemaat afhangt - een
			// brandende bal draagt zijn vlammen mee - en die kent alleen de
			// display.
			virtual int getBallOffScreenY() = 0;
			virtual void updatePaddlePosition(int x) = 0;
			virtual void showGamePage() = 0;
			virtual void hideGamePage() = 0;
			virtual void showPaddle() = 0;
			virtual void hidePaddle() = 0;

			// De uitzondering van UC04: "Game Over" over het spelscherm heen, en
			// aantikbaar. Een knop en geen panel, want er moet op geklikt kunnen
			// worden.
			virtual IButton* addGameOverLabelListener(IButtonListener* pButtonListener) = 0;
			virtual void showGameOverLabel() = 0;
			virtual void hideGameOverLabel() = 0;
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
		// Een panel per balslot, op dezelfde index als in het Depot. Dat de index
		// beide aanwijst is het hele idee: de bal hoeft zijn panel niet te kennen
		// en het panel de bal niet.
		Panel<0> arBallPanels[MaxNofBalls];
		Panel<0> arCratePanels[MaxNofCrates];
		Panel<0> arFireBinPanels[MaxNofFireBins];
		Panel<0> arScoreDigitPanels[NofScoreDigits];
		Panel<0> bottomLinePanel;
		Panel<0> leftBorderPanel;
		Panel<0> rightBorderPanel;
		Panel<0> topBorderPanel;
		Panel<0> paddlePanel;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonLeft;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonRight;

		// Het GAME OVER-vignet, midden over het spelscherm (UC04, uitzondering).
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonGameOverLabel;

		Pool<BallStates> poolBallStates;
		Pool<CrateStates> poolCrateStates;
		Pool<FireBinStates> poolFireBinStates;
		Pool<int32_t> poolScore;
		Pool<int> poolPaddlePosition;
		Flag flagShowGamePage;
		Flag flagDeshowGamePage;
		Flag flagShowPaddle;
		Flag flagHidePaddle;
		Flag flagBallPositionUpdated;
		Flag flagPaddlePositionUpdated;
		Flag flagShowGameOverLabel;
		Flag flagHideGameOverLabel;

		State state;

		// Tekent deze display in een backbuffer? Bepaalt hoe een beeld tot stand
		// komt - zie updateBallPosition-afhandeling in main(). Pas in main() te
		// beantwoorden: isBuffered() maakt de buffer zo nodig aan, en dat hoort
		// niet in een constructor die voor het opstarten van de andere taken uit
		// loopt.
		bool bDisplayIsBuffered;

#ifdef PONG_MEASURE
		// Het beeldnummer dat als laatste is getekend, en hoeveel er
		// sindsdien zijn overgeslagen. De Pool bewaart alleen de laatste stand:
		// loopt de spelklok voor op het tekenen, dan wordt een positie
		// overschreven voordat hij ooit op het scherm heeft gestaan en verspringt
		// de bal een dubbele stap. Dat is precies wat je als schokkerig ziet, en
		// zonder deze telling niet te onderscheiden van een bal die snel gaat.
		int32_t indexLastDrawnStep;
		int32_t nofSkippedSteps;
#endif

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
			flagShowGameOverLabel(this),
			flagHideGameOverLabel(this),
			state (STATE_HIDDEN),
			bDisplayIsBuffered(false)
#ifdef PONG_MEASURE
			, indexLastDrawnStep(-1)
			, nofSkippedSteps(0)
#endif
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

			// Nog geen enkele bal in het spel; het depot geeft er pas een uit als
			// GameControl daarom vraagt.
			poolBallStates.write(BallStates());
			poolPaddlePosition.write(width/2);

			// De knoppen van deze pagina moeten hun aanrakingen van het display
			// krijgen. Een uitgeschakelde knop negeert een aanraking zelf (zie
			// TouchscreenButton::touchPressed), dus ze mogen meteen mee.
			display.addTouchListener(&buttonLeft);
			display.addTouchListener(&buttonRight);
			display.addTouchListener(&buttonGameOverLabel);

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

	void updateGameObjects(const BallStates& ballStates, const CrateStates& crateStates,
	                       const FireBinStates& fireBinStates, int32_t score)
	{
		poolBallStates.write(ballStates);
		poolCrateStates.write(crateStates);
		poolFireBinStates.write(fireBinStates);
		poolScore.write(score);
		flagBallPositionUpdated.set();
	}

	// De bal is uit beeld zodra de bovenkant van zijn sprite onder de onderrand
	// van het scherm ligt. In speelveldcoordinaten (oorsprong linksonder) telt y
	// omhoog, dus dat is een negatief getal.
	//
	// De vlammen horen bij de sprite en dus bij deze grens: zou hier de kale
	// BallSizePix staan, dan knipt het vuur weg terwijl het nog zichtbaar was.
	int getBallOffScreenY()
	{
		return (height - 1) - screenHeight - ballPanelSizePix().y/2;
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

	IButton* addGameOverLabelListener(IButtonListener* pButtonListener)
	{
		buttonGameOverLabel.addButtonListener(pButtonListener);
		return &buttonGameOverLabel;
	}

	void showGameOverLabel()
	{
		flagHideGameOverLabel.clear();
		flagShowGameOverLabel.set();
	}

	void hideGameOverLabel()
	{
		flagShowGameOverLabel.clear();
		flagHideGameOverLabel.set();
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
			// De sheet moet evenveel cellen hebben als de brandcyclus vraagt: een
			// gewone bal plus een stadium per seconde. Klopt dat niet, dan wordt er
			// straks een cel buiten de sheet gekozen, en dat zie je pas als de bal
			// brandt.
			{
				const int nofExpectedCells = 1 + NofBurnStages * NofFramesPerBurnStage;
				if (imageBall.nofCols != nofExpectedCells)
				{
					ESP_LOGE("pong", "ball-sheet heeft %d cellen, verwacht %d - draai "
						"images/make_pong_images.py en tools/img2rgb565.py --pal8 opnieuw.",
						(int)imageBall.nofCols, nofExpectedCells);
				}
			}

			// Elk panel is zo groot als een cel van de spritesheet en dus ruimer
			// dan de bal zelf: de brandende bal draagt zijn vlammen mee. Voor het
			// spel verandert er niets - BallControl rekent met BallSizePix.
			//
			// Alle MaxNofBalls panels worden hier volledig opgezet en daarna
			// uitgezet. Een bal in het spel zetten is dan niet meer dan een positie
			// en makeDrawable(true) - zie MultiPanel, dat het net zo doet.
			for (int i = 0; i < MaxNofBalls; i++)
			{
				Panel<0>& panel = arBallPanels[i];

				panel.setPanelProps("ballPanel", /*locPos*/centreToTopLeftPix(width/2, height/2, ballPanelSizePix()), CoordType::Pixels,
					/*size*/ballPanelSizePix(), /*cornerRadius*/0, CoordType::Pixels,
					Alignment::MidMid, /*colFg*/0x00FFFF00, /*colBg*/0x00000000);
				setImageChecked(panel, imageBall, ballPanelSizePix(), "ballPanel");

				// Zonder deze regel staat er geen frame gekozen (_frameRow/_frameCol
				// zijn -1) en tekent Panel de HELE sheet - zestien ballen naast elkaar.
				panel.setImageFrame(0, 0);
				panel.makeNonDrawable(true);
			}

			// En hetzelfde voor de present crates. Cel 0 is een gaaf kistje; welke
			// cel er getoond wordt volgt uit de teller - zie applyCrateStates().
			for (int i = 0; i < MaxNofCrates; i++)
			{
				Panel<0>& panel = arCratePanels[i];

				panel.setPanelProps("cratePanel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
					/*size*/crateSizePix(), /*cornerRadius*/0, CoordType::Pixels,
					Alignment::MidMid, /*colFg*/0x00A87038, /*colBg*/0x00000000);
				setImageChecked(panel, imageCrate, crateSizePix(), "cratePanel");
				panel.setImageFrame(0, 0);
				panel.makeNonDrawable(true);
			}

			// En de vuurkorven. Hun cel loopt cyclisch door zolang ze er staan.
			for (int i = 0; i < MaxNofFireBins; i++)
			{
				Panel<0>& panel = arFireBinPanels[i];

				panel.setPanelProps("fireBinPanel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
					/*size*/fireBinSizePix(), /*cornerRadius*/0, CoordType::Pixels,
					Alignment::MidMid, /*colFg*/0x00FFA020, /*colBg*/0x00000000);
				setImageChecked(panel, imageFireBin, fireBinSizePix(), "fireBinPanel");
				panel.setImageFrame(0, 0);
				panel.makeNonDrawable(true);
			}

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
			// PLAY-knop - zie initPlayButton() in pdga_StartPageDisplayControl.h:
			// 190 promille
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

			// Het GAME OVER-vignet, midden in het SPEELVELD en niet midden op het
			// scherm: verticaal tussen de onderkant van de bovenwand en de
			// bovenkant van de doellijn. Het scherm loopt daaronder nog door met de
			// peddelknoppen, dus op het schermmidden zou het vignet zichtbaar te
			// laag hangen.
			//
			// Uitgerekend uit de wanddikte en de doellijn zelf, zodat het meeschuift
			// als een van die twee verandert.
			Vec2 gameOverSizePix(imageGameOver.width, imageGameOver.height);

			int32_t fieldTopPix    = WallThicknessPix;
			int32_t fieldBottomPix = height - imageGoalLine.height;
			int32_t gameOverTopPix = fieldTopPix + (fieldBottomPix - fieldTopPix - gameOverSizePix.y)/2;

			buttonGameOverLabel.setTouchScreenButtonProps
				("gameOverLabel",/*locPos*/Vec2((screenWidth - gameOverSizePix.x)/2, gameOverTopPix),
				CoordType::Pixels, /*size*/gameOverSizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colPanel*/0x00202020, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "", /*buttonFont*/ 2, /*colFont*/ 0x00FF4040,
				/*fontScale*/ 1, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);
			setImageChecked(buttonGameOverLabel, imageGameOver, gameOverSizePix, "gameOverLabel");
			buttonGameOverLabel.makeNonDrawable(true);

			// De score, midden tussen de peddelknoppen. Die staan links en rechts
			// met paddleButtonMarginPix marge, dus het midden van het scherm is ook
			// het midden van de tussenruimte.
			{
				Vec2 digitSizePix(ScoreDigitWidthPix, ScoreDigitHeightPix);
				int scoreWidthPix = NofScoreDigits * ScoreDigitWidthPix;
				int scoreLeftPix  = (screenWidth - scoreWidthPix)/2;
				int scoreTopPix   = paddleButtonTopPix + (paddleButtonSizePix.y - ScoreDigitHeightPix)/2;

				for (int i = 0; i < NofScoreDigits; i++)
				{
					Panel<0>& panel = arScoreDigitPanels[i];

					panel.setPanelProps("scoreDigit", /*locPos*/Vec2(scoreLeftPix + i*ScoreDigitWidthPix, scoreTopPix),
						CoordType::Pixels, /*size*/digitSizePix, /*cornerRadius*/0, CoordType::Pixels,
						Alignment::MidMid, /*colFg*/0x0000FFFF, /*colBg*/0x00000000);
					setImageChecked(panel, imageScoreDigits, digitSizePix, "scoreDigit");
					panel.setImageFrame(0, 0);
				}
			}
		}

		void initBallAndPaddlePosition()
		{
			// Bal en peddel eerst op hun nieuwe plek zetten, en pas daarna tekenen.
			// Anders tekent redraw(true) ze nog even op de plek van het vorige spel -
			// en voor de bal is dat onder de doellijn.
			//
			// setLocPos en niet moveTo: er staat op dit moment nog niets van deze
			// pagina op het scherm, dus er valt niets te herstellen.
			BallStates ballStates;
			poolBallStates.read(ballStates);
			applyBallStates(ballStates);

			CrateStates crateStates;
			poolCrateStates.read(crateStates);
			applyCrateStates(crateStates);

			FireBinStates fireBinStates;
			poolFireBinStates.read(fireBinStates);
			applyFireBinStates(fireBinStates);

			int32_t score;
			poolScore.read(score);
			applyScore(score);

			int paddlePosition;
			poolPaddlePosition.read(paddlePosition);
			paddlePanel.setLocPos(centreToTopLeftPix(paddlePosition, PaddleHeightPix/2, paddleSizePix()),
									CoordType::Pixels, Alignment::MidMid);

			flagBallPositionUpdated.clear();		// zojuist verwerkt; redraw(true) tekent ze.
			flagPaddlePositionUpdated.clear();
		}

		// De hele pagina in beeld. Tekenbaar maken en tekenen mag hier op de
		// PageRoot zelf - en moet zelfs: redrawRegion() breekt af op een
		// niet-tekenbare widget, en moveTo() laat de bal juist via de root
		// opnieuw tekenen. Dat de root meteen de hele boom meeneemt is precies de
		// bedoeling, want deze root draagt maar een pagina. (In een opzet met drie
		// pagina's onder een gedeelde root zou dat ze alle drie over elkaar heen
		// tekenen.)
		void showPage(GamePageRoot& page)
		{
			// Eerst de vlag terug die hidePage() over de hele boom uitzette, en PAS
			// DAARNA de standen toepassen: applyBallStates() en de twee andere zetten
			// juist de slots uit die niet in het spel zijn. In de omgekeerde volgorde
			// gaan alle vijftig balpanels op het veldmidden en alle crates in de hoek
			// weer aan, en dan worden ze ook getekend.
			page.makeDrawable(true);
			initBallAndPaddlePosition();

			// makeDrawable(true) hierboven zet ook het vignet aan; een nieuw spel
			// begint zonder.
			buttonGameOverLabel.makeNonDrawable(true);

			// redraw en niet show: show zou de hele boom weer tekenbaar maken en
			// daarmee het vignet en de zojuist uitgezette slots terugzetten.
			page.redraw(true);

			page.enable(true);

			// Na page.enable(true), niet ervoor: die zet de hele boom aan. Het
			// vignet mag tijdens het spel geen tik aannemen - hij ligt er
			// onzichtbaar overheen.
			buttonGameOverLabel.disable(true);
		}

		// Uit beeld, en met opzet ZONDER iets te tekenen. Daarom makeNonDrawable()
		// en nadrukkelijk NIET hide(): die laat de boom vanaf de wortel over het
		// vrijgekomen vlak gaan, en dat is hier precies wat niet mag.
		//
		// Dat is wat deze opzet met meerdere tekentaken werkbaar houdt. Zou het
		// verbergen wel tekenen, dan zet deze taak een vlak in de commandobuffer
		// terwijl de taak van de volgende pagina daar al zijn tekenwerk in schuift
		// - en welke van de twee er als laatste uitkomt ligt niet vast. Dan wist
		// het ene beeld het andere.
		//
		// Het mag omdat elke pagina begint met een schermvullend backgroundPanel:
		// de volgende pagina wist wat hier blijft staan. Blijft die eis overeind,
		// dan is de wissel bovendien mooier - geen zwarte flits tussen twee
		// pagina's in.
		//
		// De disable() hieronder is de enige uitschakeling van deze pagina: sinds
		// tekenbaarheid en aanraakbaarheid losse assen zijn, disabelt verbergen
		// niet meer mee. Wie hem opruimt laat de peddelknoppen en het vignet
		// doorwerken terwijl de volgende pagina in beeld staat.
		void hidePage(GamePageRoot& page)
		{
			page.makeNonDrawable(true);
			page.disable(true);

			// Een peddelwens van het vorige spel mag niet blijven staan:
			// PaddleControl zet flagHidePaddle op het moment dat het spel eindigt,
			// en die zou anders in het volgende spel de peddel meteen weer laten
			// verdwijnen.
			flagShowPaddle.clear();
			flagHidePaddle.clear();
		}

		// De maat van een cel uit de spritesheet, dus inclusief de ruimte voor de
		// vlammen. Uit het plaatje afgeleid en niet als constante opgeschreven:
		// dan kan het er niet naast komen te zitten.
		static Vec2 ballPanelSizePix()
		{
			return Vec2(imageBall.width / imageBall.nofCols, imageBall.height / imageBall.nofRows);
		}

		// Welke cel een brandende bal toont, gegeven hoe lang hij al brandt.
		//
		// Het stadium volgt uit de verstreken tijd en blijft op het laatste staan
		// als de bal langer brandt dan verwacht - dat kan niet gebeuren zolang
		// BallBurnUpMs en NofBurnStages bij elkaar passen, maar een index buiten
		// de sheet is een fout die je pas op het scherm ziet.
		static int burningFrameIndex(int32_t indexBurnAnimationStep)
		{
			int32_t stage = indexBurnAnimationStep / NofStepsPerBurnStage;
			if (stage >= NofBurnStages)
			{
				stage = NofBurnStages - 1;
			}

			int32_t indexInStage = indexBurnAnimationStep % NofFramesPerBurnStage;
			return 1 + (int)(stage * NofFramesPerBurnStage + indexInStage);
		}

		// De score over de vier cijferpanels verdelen, met voorloopnullen - dan
		// blijft hij even breed en springt hij niet heen en weer.
		void applyScore(int32_t score)
		{
			if (score < 0) { score = 0; }

			int32_t maxScore = 1;
			for (int i = 0; i < NofScoreDigits; i++) { maxScore *= 10; }
			if (score >= maxScore) { score = maxScore - 1; }

			for (int i = NofScoreDigits - 1; i >= 0; i--)
			{
				arScoreDigitPanels[i].setImageFrame(0, (int)(score % 10));
				score /= 10;
			}
		}

		static Vec2 crateSizePix()   { return Vec2(CrateSizePix, CrateSizePix); }
		static Vec2 fireBinSizePix() { return Vec2(FireBinSizePix, FireBinSizePix); }

		// Alle vuurkorfpanels op de zojuist ontvangen standen. Zelfde opzet als bij
		// de ballen en de crates; alleen is de cel hier een animatiefase en geen
		// teller, dus hij loopt cyclisch rond.
		void applyFireBinStates(const FireBinStates& fireBinStates)
		{
			for (int i = 0; i < MaxNofFireBins; i++)
			{
				const FireBinState& fireBinState = fireBinStates.arFireBin[i];
				Panel<0>& panel = arFireBinPanels[i];

				if (!fireBinState.bActive)
				{
					panel.makeNonDrawable(true);
					continue;
				}

				panel.makeDrawable(true);
				panel.setLocPos(centreToTopLeftPix(fireBinState.posX, fireBinState.posY, fireBinSizePix()),
								CoordType::Pixels, Alignment::MidMid);
				panel.setImageFrame(0, (int)(fireBinState.indexAnimationStep % imageFireBin.nofCols));
			}
		}

		// Alle cratepanels op de zojuist ontvangen standen zetten. Zelfde opzet
		// als applyBallStates(): een slot dat niet in het spel is wordt
		// niet-tekenbaar, en zo verdwijnt een geopende crate vanzelf.
		void applyCrateStates(const CrateStates& crateStates)
		{
			for (int i = 0; i < MaxNofCrates; i++)
			{
				const CrateState& crateState = crateStates.arCrate[i];
				Panel<0>& panel = arCratePanels[i];

				if (!crateState.bActive)
				{
					panel.makeNonDrawable(true);
					continue;
				}

				panel.makeDrawable(true);
				panel.setLocPos(centreToTopLeftPix(crateState.posX, crateState.posY, crateSizePix()),
								CoordType::Pixels, Alignment::MidMid);

				// Cel 0 is gaaf en toont NofCrateHits; elke treffer schuift een cel
				// op. Het aftellen zit dus in het plaatje en niet in losse tekst.
				int indexFrame = NofCrateHits - crateState.nofHitsLeft;
				if (indexFrame < 0) { indexFrame = 0; }
				if (indexFrame >= imageCrate.nofCols) { indexFrame = (int)imageCrate.nofCols - 1; }
				panel.setImageFrame(0, indexFrame);
			}
		}

		// Alle balpanels op de zojuist ontvangen standen zetten. Een slot dat niet
		// in het spel is, wordt niet-tekenbaar - dat is hoe een bal die zichzelf
		// aan het depot teruggaf van het scherm verdwijnt, zonder dat hij daar een
		// apart bericht voor hoeft te sturen.
		//
		// Alleen de coordinaten; het tekenen gebeurt daarna in een keer.
		void applyBallStates(const BallStates& ballStates)
		{
			for (int i = 0; i < MaxNofBalls; i++)
			{
				const BallState& ballState = ballStates.arBall[i];
				Panel<0>& panel = arBallPanels[i];

				if (!ballState.bActive)
				{
					panel.makeNonDrawable(true);
					continue;
				}

				panel.makeDrawable(true);
				panel.setLocPos(centreToTopLeftPix(ballState.posX, ballState.posY, ballPanelSizePix()),
								CoordType::Pixels, Alignment::MidMid);

				// Cel 0 is de gewone bal, 1..N de brandcyclus. Hoeveel cellen de
				// sheet heeft is kennis van deze klasse; de bal telt alleen hoe
				// lang hij al brandt.
				panel.setImageFrame(0, ballState.bBurning
					? burningFrameIndex(ballState.indexBurnAnimationStep)
					: 0);
			}
		}
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

			GamePageRoot pageGame("rootGameDisplay",display);
			pageGame.addChildWidget(backgroundPanel);	// eerst, want alles hierna tekent erop.
			pageGame.addChildWidget(bottomLinePanel);
			pageGame.addChildWidget(leftBorderPanel);
			pageGame.addChildWidget(rightBorderPanel);
			pageGame.addChildWidget(topBorderPanel);
			pageGame.addChildWidget(paddlePanel);
			pageGame.addChildWidget(buttonLeft);
			pageGame.addChildWidget(buttonRight);

			// De crates vóór de ballen, zodat een bal er overheen gaat en niet
			// erachter verdwijnt op het moment dat hij hem raakt.
			for (int i = 0; i < MaxNofCrates; i++)
			{
				pageGame.addChildWidget(arCratePanels[i]);
			}

			for (int i = 0; i < MaxNofFireBins; i++)
			{
				pageGame.addChildWidget(arFireBinPanels[i]);
			}

			for (int i = 0; i < NofScoreDigits; i++)
			{
				pageGame.addChildWidget(arScoreDigitPanels[i]);
			}

			// De ballen als laatste, dus bovenop. Dat is niet alleen mooier bij een
			// kaatsing tegen de peddel: een brandende bal valt onder de doellijn
			// door en komt daarbij over de peddelknoppen heen. Stonden ze eerder in
			// de rij, dan verdween hij vlak na de doellijn achter een knop in
			// plaats van door te vliegen tot buiten het scherm.
			for (int i = 0; i < MaxNofBalls; i++)
			{
				pageGame.addChildWidget(arBallPanels[i]);
			}

			// En het GAME OVER-vignet daar nog overheen.
			pageGame.addChildWidget(buttonGameOverLabel);

			hidePage(pageGame);		// het spel begint op de startpagina.

			// Beeldtelling, om te kunnen nagaan of de 25 per seconde ook echt
			// gehaald worden in plaats van dat aan te nemen.
			int32_t nofFrames = 0;
			int64_t timeOfLastReportUs = esp_timer_get_time();
#ifdef PONG_MEASURE
			int64_t timeOfLastFrameUs = 0;
			int32_t minFrameIntervalUs = 0x7FFFFFFF;
			int32_t maxFrameIntervalUs = 0;
#endif

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.
				switch(state)
				{
					case STATE_SHOWN:
					{
						waitAny(flagShowGamePage + flagDeshowGamePage +
							    flagShowPaddle + flagHidePaddle +
							    flagShowGameOverLabel + flagHideGameOverLabel +
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
							// De enable() achter elke show() is dragend en geen
							// dubbeling: tonen maakt niet aanraakbaar. De twee knoppen
							// worden verder alleen nog door page.enable(true) in
							// showPage() aangezet. Beide plekken lopen door
							// TouchscreenButton::enable(), die bij een echte overgang
							// van uit naar aan een druk weggooit die is blijven hangen
							// doordat de knop tijdens het loslaten uit stond.
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
								// makeNonDrawable() en niet hide(): wissen hoeft er
								// niet bij - wat niet meer wordt getekend, staat er in
								// het volgende beeld ook niet meer. hide() zou er nog
								// een herstel van het vrijgekomen vlak overheen doen,
								// en dat beeld gaat toch al in zijn geheel opnieuw.
								paddlePanel.makeNonDrawable(true);
								buttonLeft.makeNonDrawable(true);
								buttonRight.makeNonDrawable(true);
							}
							else
							{
								// Hier moet er wel gewist worden, want er komt geen
								// volledige hertekening meer overheen. hide() laat de
								// boom vanaf de wortel over het vrijgekomen vlak gaan,
								// dus daar komt de achtergrond terug in plaats van een
								// vulling met de eigen colBg.
								paddlePanel.hide(true);
								buttonLeft.hide(true);
								buttonRight.hide(true);
							}

							// Buiten de if/else, en dat moet ook: geen van beide takken
							// schakelt iets uit. De gebufferde tak zet alleen vlaggen,
							// en verbergen disabelt sinds de splitsing van de assen
							// niet meer mee.
							paddlePanel.disable(true);
							buttonLeft.disable(true);
							buttonRight.disable(true);
						}
						else if(hasFired(flagShowGameOverLabel))
						{
							// Er komen geen balbeelden meer, dus deze pagina wordt
							// nergens anders meer hertekend. Zelf doen dus - maar met
							// redraw en niet met show: de peddel en de peddelknoppen
							// zijn hier net verborgen, en show zou ze samen met alle
							// ongebruikte slots weer aanzetten.
							buttonGameOverLabel.makeDrawable(true);
							buttonGameOverLabel.enable(true);
							pageGame.redraw(true);
						}
						else if(hasFired(flagHideGameOverLabel))
						{
							// Geen hide(): hierna volgt meteen hideGamePage(), en die
							// mag met opzet niets tekenen zolang de volgende pagina in
							// dezelfde commandobuffer bezig is - zie hidePage().
							buttonGameOverLabel.makeNonDrawable(true);
							buttonGameOverLabel.disable(true);
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
							BallStates ballStates;
							poolBallStates.read(ballStates);

							CrateStates crateStates;
							poolCrateStates.read(crateStates);

							FireBinStates fireBinStates;
							poolFireBinStates.read(fireBinStates);

							// Alle panels op hun nieuwe stand, inclusief de cel van
							// de brandcyclus, de cel van de crate-teller, en het
							// uitzetten van slots die niet meer in het spel zijn.
							// Nog niet tekenen.
							int32_t score;
							poolScore.read(score);

							applyBallStates(ballStates);
							applyCrateStates(crateStates);
							applyFireBinStates(fireBinStates);
							applyScore(score);

#ifdef PONG_MEASURE
							// Is er een beeld overgeslagen? Zie nofSkippedSteps.
							if (indexLastDrawnStep >= 0)
							{
								nofSkippedSteps += (ballStates.indexFrame - indexLastDrawnStep - 1);
							}
							indexLastDrawnStep = ballStates.indexFrame;
#endif

							if (bDisplayIsBuffered)
							{
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
								//
								// redraw en niet show: deze boom zit vol widgets die
								// met opzet uit staan (de ongebruikte bal-, crate- en
								// vuurkorfslots, en na hidePaddle ook de peddel). show
								// zou die vijfentwintig keer per seconde weer aanzetten
								// en het veld in een beeld laten vollopen.
								pageGame.redraw(true);
							}
							else
							{
								// Zonder backbuffer verzet elke bal zich apart, zodat
								// alleen de strook die vrijkomt wordt hersteld.
								for (int i = 0; i < MaxNofBalls; i++)
								{
									if (ballStates.arBall[i].bActive)
									{
										arBallPanels[i].moveTo(
											centreToTopLeftPix(ballStates.arBall[i].posX, ballStates.arBall[i].posY, ballPanelSizePix()),
											CoordType::Pixels);
									}
								}
							}

							nofFrames++;
							int64_t nowUs = esp_timer_get_time();

#ifdef PONG_MEASURE
							// Niet alleen hoeveel beelden er per seconde zijn, maar
							// ook hoe gelijkmatig ze vallen. Een gemiddelde van 25
							// fps kan net zo goed 40+40+40 zijn als 20+70+30, en
							// alleen het tweede ziet er schokkerig uit. Zonder dit
							// onderscheid is "minder vloeiend" niet na te gaan.
							if (timeOfLastFrameUs > 0)
							{
								int32_t frameIntervalUs = (int32_t)(nowUs - timeOfLastFrameUs);
								if (frameIntervalUs < minFrameIntervalUs) { minFrameIntervalUs = frameIntervalUs; }
								if (frameIntervalUs > maxFrameIntervalUs) { maxFrameIntervalUs = frameIntervalUs; }
							}
							timeOfLastFrameUs = nowUs;
#endif

							if (nowUs - timeOfLastReportUs >= 1000000)
							{
#ifdef PONG_MEASURE
								// Een seconde met maar een beeld erin - vlak na een
								// herstart - heeft geen enkel interval opgeleverd. Dan
								// staat de teller nog op zijn beginwaarde, en die als
								// "beeldtijd 2147483..0 ms" afdrukken leest als een
								// meting terwijl er niets gemeten is.
								if (maxFrameIntervalUs > 0)
								{
									ESP_LOGI("pong", "%d fps, beeldtijd %d..%d ms (doel %d), %d overgeslagen",
										(int)nofFrames, (int)(minFrameIntervalUs/1000),
										(int)(maxFrameIntervalUs/1000), FrameIntervalMs,
										(int)nofSkippedSteps);
								}
								else
								{
									ESP_LOGI("pong", "%d fps, geen beeldtijd gemeten (doel %d), %d overgeslagen",
										(int)nofFrames, FrameIntervalMs, (int)nofSkippedSteps);
								}
								nofSkippedSteps = 0;
								minFrameIntervalUs = 0x7FFFFFFF;
								maxFrameIntervalUs = 0;
#else
								ESP_LOGI("pong", "%d fps", (int)nofFrames);
#endif
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

#ifdef PONG_MEASURE
							// Een nieuw spel telt zijn stappen vanaf nul, dus de
							// vorige stand zegt niets meer.
							indexLastDrawnStep = -1;

							// De beeldtijdmeting begint hier opnieuw. Tussen twee
							// spellen staat de game-over-pagina in beeld en tekent
							// deze pagina niets; zonder deze regel telt die hele
							// wachttijd mee als uitschieter, en lijkt het spel te
							// haperen terwijl er alleen niet gespeeld werd.
							timeOfLastFrameUs = 0;
#endif

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
