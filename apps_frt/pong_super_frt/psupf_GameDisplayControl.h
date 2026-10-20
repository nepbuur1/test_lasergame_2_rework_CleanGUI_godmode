// by Marius Versteegen, 2023

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>
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
// Uit, omdat het tijd kost waar het gemeten wordt: een logregel is bij 115200
// baud zo'n 7 ms, en de spawn-regels lopen in de taak waarin ook de beeldklok
// zit - een kwart van een beeld.
//#define PONG_MEASURE

using namespace crt;
namespace pong_super_frt
{
	// Zie psupf_BallControl.h: logger komt uit namespace crt, niet uit deze.

	// Hoeveel ballen er tegelijk in de lucht kunnen zijn. Een present
	// crate spawnt er tien ineens. Ze liggen geprealloceerd in een Depot (zie
	// psupf_Depot.h) en hebben elk een panel in de array hieronder, op dezelfde
	// index.
	const int MaxNofBalls = 50;

	// Alles wat het tekenen van EEN bal nodig heeft. 
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
	// zijn en hoeveel treffers ze verdragen. Elke drie seconden
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
	// weer; een bal die hem raakt brandt op in BallBurnUpMs.
	//
	// De kans dat het beslismoment een vuurkorf oplevert in plaats van
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
	// Alles hieronder is in pixels, en de bal legt er
	// een paar per frame af.
	//
	// De oorsprong van het speelveld ligt LINKSONDER: y=0 is de peddellijn en y
	// loopt omhoog. GameDisplayControl is de enige die dat naar schermpixels
	// omrekent (het scherm telt van boven).
	const int FrameIntervalMs      = 40;	// 25 beelden per seconde.
	const int WallThicknessPix     = 10;
	const int BallSizePix          = 12;
	const int PaddleWidthPix       = 50;
	const int PaddleHeightPix      = 12;
	const int BallSpeedPixPerFrame = 4;		// 100 pixels per seconde.
	const int PaddleStepPix        = 8;		// 200 pixels per seconde zolang de knop ingedrukt blijft.

	// De levensduren in frame steps.
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
	// worden. Het volgt uit de tijd.
	const int NofBurnStages         = 3;
	const int NofFramesPerBurnStage = 5;
	const int NofStepsPerBurnStage  = (BallBurnUpMs / NofBurnStages) / FrameIntervalMs;

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
	// een widget.
	const int NofFixedGameWidgets = 9;

	typedef PageRoot<NofFixedGameWidgets + MaxNofBalls + MaxNofCrates + MaxNofFireBins + NofScoreDigits> GamePageRoot;

	class GameDisplayControl
	{

		// --- FreeRTOS (was CleanRTOS) ---
		static constexpr EventBits_t eventBitShowGamePage = (1 << 0);
		static constexpr EventBits_t eventBitDeshowGamePage = (1 << 1);
		static constexpr EventBits_t eventBitShowPaddle = (1 << 2);
		static constexpr EventBits_t eventBitHidePaddle = (1 << 3);
		static constexpr EventBits_t eventBitBallPositionUpdated = (1 << 4);
		static constexpr EventBits_t eventBitPaddlePositionUpdated = (1 << 5);
		static constexpr EventBits_t eventBitShowGameOverLabel = (1 << 6);
		static constexpr EventBits_t eventBitHideGameOverLabel = (1 << 7);
		EventGroupHandle_t eventGroup;
		EventBits_t firedBits;		// wat de laatste wachtoproep op meer tegelijk opleverde; zie hasFired()
		TaskHandle_t taskHandle;

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
		// backgroundPanel beslaat het HELE scherm, niet alleen het speelveld.
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

		QueueHandle_t ballStatesQueue;		// lengte 1: was Pool<BallStates> poolBallStates
		QueueHandle_t crateStatesQueue;		// lengte 1: was Pool<CrateStates> poolCrateStates
		QueueHandle_t fireBinStatesQueue;		// lengte 1: was Pool<FireBinStates> poolFireBinStates
		QueueHandle_t scoreQueue;		// lengte 1: was Pool<int32_t> poolScore
		QueueHandle_t paddlePositionQueue;		// lengte 1: was Pool<int> poolPaddlePosition

		State state;

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
			eventGroup(xEventGroupCreate()),
			firedBits(0),
			taskHandle(nullptr),
			display(display),
			freeFonts(freeFonts),
			screenWidth(0),
			screenHeight(0),
			width(0),
			height(0),
			frameDelayMs(frameDelayMs),
			state (STATE_HIDDEN)
#ifdef PONG_MEASURE
			, indexLastDrawnStep(-1)
			, nofSkippedSteps(0)
#endif
		{
			ballStatesQueue = xQueueCreate(1, sizeof(BallStates));
			{ BallStates initValue{}; xQueueOverwrite(ballStatesQueue, &initValue); }
			crateStatesQueue = xQueueCreate(1, sizeof(CrateStates));
			{ CrateStates initValue{}; xQueueOverwrite(crateStatesQueue, &initValue); }
			fireBinStatesQueue = xQueueCreate(1, sizeof(FireBinStates));
			{ FireBinStates initValue{}; xQueueOverwrite(fireBinStatesQueue, &initValue); }
			scoreQueue = xQueueCreate(1, sizeof(int32_t));
			{ int32_t initValue{}; xQueueOverwrite(scoreQueue, &initValue); }
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

			// Nog geen enkele bal in het spel; het depot geeft er pas een uit als
			// GameControl daarom vraagt.
			{ BallStates poolValue = BallStates(); xQueueOverwrite(ballStatesQueue, &poolValue); }
			{ int poolValue = width/2; xQueueOverwrite(paddlePositionQueue, &poolValue); }

			// De knoppen van deze pagina moeten hun aanrakingen van het display
			// krijgen. Een uitgeschakelde knop negeert een aanraking zelf (zie
			// TouchscreenButton::touchPressed), dus ze mogen meteen mee.
			display.addTouchListener(&buttonLeft);
			display.addTouchListener(&buttonRight);
			display.addTouchListener(&buttonGameOverLabel);

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

	// Een beeld: alle ballen en alle crates in een keer, met een enkele vlag
	// erachter. Twee losse berichten zouden twee hertekeningen kunnen opleveren,
	// en een present kost het hele scherm.
	void updateGameObjects(const BallStates& ballStates, const CrateStates& crateStates,
	                       const FireBinStates& fireBinStates, int32_t score)
	{
		{ BallStates poolValue = ballStates; xQueueOverwrite(ballStatesQueue, &poolValue); }
		{ CrateStates poolValue = crateStates; xQueueOverwrite(crateStatesQueue, &poolValue); }
		{ FireBinStates poolValue = fireBinStates; xQueueOverwrite(fireBinStatesQueue, &poolValue); }
		{ int32_t poolValue = score; xQueueOverwrite(scoreQueue, &poolValue); }
		xEventGroupSetBits(eventGroup, eventBitBallPositionUpdated);
	}

	// De laagste speelveld-y waarop nog iets van de bal te zien is; daaronder is
	// hij het scherm uit. De bal is uit beeld zodra de bovenkant van zijn sprite
	// onder de onderrand van het scherm ligt. In speelveldcoordinaten (oorsprong
	// linksonder) telt y omhoog, dus dat is een negatief getal.
	//
	// Deze klasse beantwoordt dat en niet BallControl, omdat het antwoord van de
	// spritemaat afhangt - een brandende bal draagt zijn vlammen mee - en die kent
	// alleen de display. Die vlammen horen dus bij deze grens: zou hier de kale
	// BallSizePix staan, dan knipt het vuur weg terwijl het nog zichtbaar was.
	int getBallOffScreenY()
	{
		// deze drie variabelen: geen aanpassing na initialisatie/constructie 
		// -> geen taaksynchronisatie nodig.
		return (height - 1) - screenHeight - ballPanelSizePix().y/2;
	}

	void updatePaddlePosition(int x)
	{
		{ int poolValue = x; xQueueOverwrite(paddlePositionQueue, &poolValue); }
		xEventGroupSetBits(eventGroup, eventBitPaddlePositionUpdated);
	}

	void showGamePage()
	{
		xEventGroupClearBits(eventGroup, eventBitDeshowGamePage);	// alleen de laatste wens telt.
		xEventGroupSetBits(eventGroup, eventBitShowGamePage);
	}

	void hideGamePage()
	{
		xEventGroupClearBits(eventGroup, eventBitShowGamePage);
		xEventGroupSetBits(eventGroup, eventBitDeshowGamePage);
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

	// De uitzondering van UC04: "Game Over" over het spelscherm heen, en
	// aantikbaar. Een knop en geen panel, want er moet op geklikt kunnen worden.
	IButton* addGameOverLabelListener(IButtonListener* pButtonListener)
	{
		buttonGameOverLabel.addButtonListener(pButtonListener);
		return &buttonGameOverLabel;
	}

	void showGameOverLabel()
	{
		xEventGroupClearBits(eventGroup, eventBitHideGameOverLabel);
		xEventGroupSetBits(eventGroup, eventBitShowGameOverLabel);
	}

	void hideGameOverLabel()
	{
		xEventGroupClearBits(eventGroup, eventBitShowGameOverLabel);
		xEventGroupSetBits(eventGroup, eventBitHideGameOverLabel);
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
			// De colFg-kleuren blijven staan. Ze worden bij de PAL8-adapter niet
			// gebruikt (needsBackground() is onwaar zodra er een plaatje op zit),
			// maar zonder de setImage-regels hieronder staat het spel er weer
			// zoals het was.
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
			// PLAY-knop - zie initPlayButton() in psupf_StartPageDisplayControl.h:
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
			xQueuePeek(ballStatesQueue, &ballStates, 0);
			applyBallStates(ballStates);

			CrateStates crateStates;
			xQueuePeek(crateStatesQueue, &crateStates, 0);
			applyCrateStates(crateStates);

			FireBinStates fireBinStates;
			xQueuePeek(fireBinStatesQueue, &fireBinStates, 0);
			applyFireBinStates(fireBinStates);

			int32_t score;
			xQueuePeek(scoreQueue, &score, 0);
			applyScore(score);

			int paddlePosition;
			xQueuePeek(paddlePositionQueue, &paddlePosition, 0);
			paddlePanel.setLocPos(centreToTopLeftPix(paddlePosition, PaddleHeightPix/2, paddleSizePix()),
									CoordType::Pixels, Alignment::MidMid);

			xEventGroupClearBits(eventGroup, eventBitBallPositionUpdated);		// zojuist verwerkt; redraw(true) tekent ze.
			xEventGroupClearBits(eventGroup, eventBitPaddlePositionUpdated);
		}

		// De hele pagina in beeld. Tekenbaar maken en tekenen mag hier op de
		// PageRoot zelf.
		void showPage(GamePageRoot& page)
		{
			page.makeDrawable(true);
			initBallAndPaddlePosition();

			// makeDrawable(true) hierboven zet ook het vignet aan; een nieuw spel
			// begint zonder.
			buttonGameOverLabel.makeNonDrawable(true);

			// redraw en niet show: show zou de hele boom weer tekenbaar maken en
			// daarmee het vignet en de zojuist uitgezette slots terugzetten.
			page.redraw(true);

			page.enable(true);

			// Na page.enable(true), die zet immers de touchbuttons van de hele boom aan. 
			// Het vignet mag tijdens het spel geen touch detecteren - hij ligt er
			// onzichtbaar overheen.
			buttonGameOverLabel.disable(true);
		}

		void hidePage(GamePageRoot& page)
		{
			page.makeNonDrawable(true);
			page.disable(true);

			xEventGroupClearBits(eventGroup, eventBitShowPaddle);
			xEventGroupClearBits(eventGroup, eventBitHidePaddle);
		}

		// De enable() zijn ook nodig, want alleen tonen zet de
		// touchscreenbuttons niet aan. De twee knoppen worden verder alleen nog
		// door page.enable(true) in showPage() aangezet.
		void showPaddleAndButtons()
		{
			paddlePanel.show(true);
			buttonLeft.show(true);
			buttonLeft.enable(true);
			buttonRight.show(true);
			buttonRight.enable(true);
		}

		void hidePaddleAndButtons()
		{
			// makeNonDrawable() en niet hide(): wissen hoeft er niet bij - wat
			// niet meer wordt getekend, staat er in het volgende beeld ook niet
			// meer. hide() zou er nog een herstel van het vrijgekomen vlak
			// overheen doen, en dat beeld gaat toch al in zijn geheel opnieuw.
			paddlePanel.makeNonDrawable(true);
			buttonLeft.makeNonDrawable(true);
			buttonRight.makeNonDrawable(true);

			// Apart, want verbergen disabelt sinds de splitsing van de assen
			// niet meer mee.
			buttonLeft.disable(true);
			buttonRight.disable(true);
		}

		// Er komen geen balbeelden meer, dus deze pagina wordt nergens anders
		// meer hertekend. Zelf doen dus - maar met redraw en niet met show: de
		// peddel en de peddelknoppen zijn hier net verborgen, en show zou ze
		// samen met alle ongebruikte slots weer aanzetten.
		void doShowGameOverLabel(GamePageRoot& page)
		{
			buttonGameOverLabel.makeDrawable(true);
			buttonGameOverLabel.enable(true);
			page.redraw(true);
		}

		// Geen hide(): hierna volgt meteen hideGamePage(), en die mag met opzet
		// niets tekenen zolang de volgende pagina in dezelfde commandobuffer
		// bezig is - zie hidePage().
		void doHideGameOverLabel()
		{
			buttonGameOverLabel.makeNonDrawable(true);
			buttonGameOverLabel.disable(true);
		}

		// Het peddelpaneel op de plek zetten die PaddleControl zojuist in de
		// pool heeft gelegd.
		void syncPaddlePanelPos()
		{
			int paddlePosition;
			xQueuePeek(paddlePositionQueue, &paddlePosition, 0);
			Vec2 paddleTopLeftPix = centreToTopLeftPix(paddlePosition, PaddleHeightPix/2, paddleSizePix());

			// Alleen verzetten, niet tekenen. Het beeld dat de bal zometeen laat
			// maken neemt de peddel mee; er nu al een eigen beeld aan wijden zou
			// het scherm twee keer uitsturen voor een beeld, en dat halveert de
			// beeldsnelheid.
			paddlePanel.setLocPos(paddleTopLeftPix, CoordType::Pixels, Alignment::MidMid);
		}

		// De stand van het spel uit de pools op alle panels zetten: crates,
		// vuurkorven, score en ballen. Alleen de vlaggen, plekken en cellen;
		// redrawWholePage() tekent daarna alles in een keer.
		void applyGameState()
		{
			BallStates ballStates;
			xQueuePeek(ballStatesQueue, &ballStates, 0);

			CrateStates crateStates;
			xQueuePeek(crateStatesQueue, &crateStates, 0);

			FireBinStates fireBinStates;
			xQueuePeek(fireBinStatesQueue, &fireBinStates, 0);

			int32_t score;
			xQueuePeek(scoreQueue, &score, 0);

			applyCrateStates(crateStates);
			applyFireBinStates(fireBinStates);
			applyScore(score);
			applyBallStates(ballStates);

#ifdef PONG_MEASURE
			// Is er een beeld overgeslagen? Zie nofSkippedSteps.
			if (indexLastDrawnStep >= 0)
			{
				nofSkippedSteps += (ballStates.indexFrame - indexLastDrawnStep - 1);
			}
			indexLastDrawnStep = ballStates.indexFrame;
#endif
		}

		// Het hele beeld in een keer.
		void redrawWholePage(GamePageRoot& page)
		{
			// De peddel er meteen bij. De twee vlaggen worden in
			// deze if/else-keten om beurten afgehandeld, dus zonder
			// dit zou de peddel steeds een beeld achterlopen op wat
			// PaddleControl al doorgaf.
			syncPaddlePanelPos();

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
			page.redraw(true);
		}

		// De maat van een cel uit de spritesheet, dus inclusief de ruimte voor de
		// vlammen. Uit het plaatje afgeleid en niet als constante opgeschreven:
		// dan kan het er niet naast komen te zitten.
		static Vec2 ballPanelSizePix()
		{
			return Vec2(imageBall.width / imageBall.nofCols, imageBall.height / imageBall.nofRows);
		}

		// Welke cel een brandende bal toont, gegeven hoe lang hij al brandt.
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

		// Een spawnable-paneel (bal, crate, vuurkorf) op zijn nieuwe stand zetten:
		// de vlag, de plek en de cel. Er wordt niets getekend; dat doet
		// page.redraw(true) daarna in een keer.
		void applySpawnablePanel(Panel<0>& panel, bool bActive, const Vec2& topLeftPix, int32_t frameCol)
		{
			if (!bActive)
			{
				panel.makeNonDrawable(true);
				return;
			}

			panel.makeDrawable(true);
			panel.setLocPos(topLeftPix, CoordType::Pixels, Alignment::MidMid);
			panel.setImageFrame(0, frameCol);
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

		// Toepassen van de FireBinStates op de bijbehorende panels.
		void applyFireBinStates(const FireBinStates& fireBinStates)
		{
			for (int i = 0; i < MaxNofFireBins; i++)
			{
				const FireBinState& fireBinState = fireBinStates.arFireBin[i];
				applySpawnablePanel(arFireBinPanels[i], fireBinState.bActive,
					centreToTopLeftPix(fireBinState.posX, fireBinState.posY, fireBinSizePix()),
					(int)(fireBinState.indexAnimationStep % imageFireBin.nofCols));
			}
		}

		// Toepassen van de CrateStates op de bijbehorende panels.
		void applyCrateStates(const CrateStates& crateStates)
		{
			for (int i = 0; i < MaxNofCrates; i++)
			{
				const CrateState& crateState = crateStates.arCrate[i];

				// Het aftellen zit in het plaatje en niet in losse tekst.
				int indexFrame = NofCrateHits - crateState.nofHitsLeft;
				if (indexFrame < 0) { indexFrame = 0; }
				if (indexFrame >= imageCrate.nofCols) { indexFrame = (int)imageCrate.nofCols - 1; }

				applySpawnablePanel(arCratePanels[i], crateState.bActive,
					centreToTopLeftPix(crateState.posX, crateState.posY, crateSizePix()),
					indexFrame);
			}
		}

		// Alle balpanels op de zojuist ontvangen standen zetten. Alleen de
		// coordinaten en de cel; het tekenen gebeurt daarna in een keer.
		void applyBallStates(const BallStates& ballStates)
		{
			for (int i = 0; i < MaxNofBalls; i++)
			{
				const BallState& ballState = ballStates.arBall[i];

				// Cel 0 is de gewone bal, 1..N de brandcyclus. Hoeveel cellen de
				// sheet heeft is kennis van deze klasse; de bal telt alleen hoe
				// lang hij al brandt.
				int indexFrame = ballState.bBurning
					? burningFrameIndex(ballState.indexBurnAnimationStep)
					: 0;

				applySpawnablePanel(arBallPanels[i], ballState.bActive,
					centreToTopLeftPix(ballState.posX, ballState.posY, ballPanelSizePix()),
					indexFrame);
			}
		}
		static Vec2 paddleSizePix() { return Vec2(PaddleWidthPix, PaddleHeightPix); }

		void defineDisplayHierarchy(IWidget& pageRoot)
		{
			pageRoot.addChildWidget(backgroundPanel);	// eerst, want alles hierna tekent erop.
			pageRoot.addChildWidget(bottomLinePanel);
			pageRoot.addChildWidget(leftBorderPanel);
			pageRoot.addChildWidget(rightBorderPanel);
			pageRoot.addChildWidget(topBorderPanel);
			pageRoot.addChildWidget(paddlePanel);
			pageRoot.addChildWidget(buttonLeft);
			pageRoot.addChildWidget(buttonRight);

			// De crates vóór de ballen, zodat een bal er overheen gaat en niet
			// erachter verdwijnt op het moment dat hij hem raakt.
			for (int i = 0; i < MaxNofCrates; i++)
			{
				pageRoot.addChildWidget(arCratePanels[i]);
			}

			for (int i = 0; i < MaxNofFireBins; i++)
			{
				pageRoot.addChildWidget(arFireBinPanels[i]);
			}

			for (int i = 0; i < NofScoreDigits; i++)
			{
				pageRoot.addChildWidget(arScoreDigitPanels[i]);
			}

			// De ballen als laatste, dus bovenop. Dat is niet alleen mooier bij een
			// kaatsing tegen de peddel: een brandende bal valt onder de doellijn
			// door en komt daarbij over de peddelknoppen heen. Stonden ze eerder in
			// de rij, dan verdween hij vlak na de doellijn achter een knop in
			// plaats van door te vliegen tot buiten het scherm.
			for (int i = 0; i < MaxNofBalls; i++)
			{
				pageRoot.addChildWidget(arBallPanels[i]);
			}

			// En het GAME OVER-vignet daar nog overheen.
			pageRoot.addChildWidget(buttonGameOverLabel);
		}


		// --- FreeRTOS-hulpfuncties ---
		static void staticMain(void* pvParameters)
		{
			static_cast<GameDisplayControl*>(pvParameters)->main();
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
			initPanels();

			// Elk beeld wordt de hele boom opnieuw opgebouwd en door de backbuffer
			// in een keer uitgestuurd. Tekenen in geheugen is bijna gratis, maar
			// het uitsturen kost altijd het hele scherm - ~31 ms, ongeacht hoeveel
			// er veranderde. Dan is alles opnieuw tekenen goedkoper dan het slim
			// proberen te doen, en flikkervrij: er komt pas iets op het scherm als
			// het beeld af is. Zelfde afweging als in
			// CleanGUI_for_TFT_eSPI/examples/MultiPanel.
			//
			// Rechtstreeks tekenen (TFT_eSPI_DisplayAdapter_RGB565) zou daardoor
			// elk beeld een zichtbare wis van het hele scherm zijn. Die route
			// heeft er wel in gezeten en werkte, met moveTo() voor wat beweegt;
			// hij is op 2026-09-07 bewust weggehaald om de code eenvoudig te
			// houden (zie het logboek; commit 5963268 is de laatste versie die
			// hem nog had). Vandaar de controle: liever meteen stoppen dan een
			// flikkerend spel.
			assert(display.isBuffered());

			GamePageRoot pageGame("rootGameDisplay",display);
			defineDisplayHierarchy(pageGame);

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
				switch(state)
				{
					case STATE_SHOWN:
					{
						firedBits = xEventGroupWaitBits(eventGroup, eventBitShowGamePage | eventBitDeshowGamePage | eventBitShowPaddle | eventBitHidePaddle | eventBitShowGameOverLabel | eventBitHideGameOverLabel | eventBitBallPositionUpdated | eventBitPaddlePositionUpdated, pdFALSE, pdFALSE, portMAX_DELAY);

						if(hasFired(eventBitDeshowGamePage))
						{
							hidePage(pageGame);
							state = STATE_HIDDEN;
						}
						else if(hasFired(eventBitShowGamePage))
						{
							// al in beeld: niets te doen.
						}
						else if(hasFired(eventBitShowPaddle))
						{
							showPaddleAndButtons();
						}
						else if(hasFired(eventBitHidePaddle))
						{
							hidePaddleAndButtons();
						}
						else if(hasFired(eventBitShowGameOverLabel))
						{
							doShowGameOverLabel(pageGame);
						}
						else if(hasFired(eventBitHideGameOverLabel))
						{
							doHideGameOverLabel();
						}
						else if(hasFired(eventBitPaddlePositionUpdated))
						{
							syncPaddlePanelPos();
						}
						else if(hasFired(eventBitBallPositionUpdated))
						{
							applyGameState();
							redrawWholePage(pageGame);

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
						firedBits = xEventGroupWaitBits(eventGroup, eventBitShowGamePage | eventBitDeshowGamePage, pdFALSE, pdFALSE, portMAX_DELAY);

						if(hasFired(eventBitShowGamePage))
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
						else if(hasFired(eventBitDeshowGamePage))
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

};// end namespace pong_super_frt
