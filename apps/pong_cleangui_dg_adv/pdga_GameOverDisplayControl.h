// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>
// Hier en niet in crt_TouchscreenKeyboard.h, waar hij hoort: die header noemt
// IKeyboardListener wel maar includet hem niet, dus hij moet ervoor staan.
#include <crt_IKeyboardListener.h>
#include <crt_TouchscreenKeyboard.h>
#include <crt_InputField.h>
#include <stdio.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include "images/_pal8/_allImages.h"
#include "pdga_TorchesDisplayHelpers.h"
#include "pdga_HallOfFame.h"

// De DisplayControl van de game-over-pagina, met een eigen PageRoot en een eigen
// taak. Zie pdga_GameDisplayControl.h voor waarom het verbergen niets tekent.
//
// UC03_GameOverHandling kent twee fasen, en die delen deze ene pagina:
//
//   NAAM   een invoerveld met "TYPE UW NAAM" erboven en het toetsenbord dat van
//          onderaf in beeld schuift. Alleen als de score een top-5 plek haalt.
//   LIJST  de hall of fame met daaronder de knop "Play Again".
//
// Twee fasen op een PageRoot en niet twee PageRoots: er mag er maar een tegelijk
// in beeld staan (zie de opmerking boven crt_PageRoot.h), en die eis geldt binnen
// deze taak net zo goed als ertussen. Wisselen gaat met setDrawable().

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// Zie pdga_BallControl.h: logger komt uit namespace crt, niet uit deze.

	typedef TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> PlayAgainButton;

	// Waar de "Play Again"-knop staat: onder de hall of fame. Een eigen hoogte en
	// niet die van de startpagina, want deze pagina heeft er meer boven staan.
	const int PlayAgainButtonTopPix = 176;

	// De verticale indeling van de hall of fame, uitgerekend in plaats van
	// opgeschreven: de titel bovenaan, de knop onderaan, en de lijst precies
	// midden in wat daartussen overblijft. Verschuift de knop of wordt de titel
	// een andere maat, dan schuift de lijst vanzelf mee.
	const int HallOfFameTitleTopPix   = 10;
	const int HallOfFameLineHeightPix = 18;

	const int HallOfFameListHeightPix = NofHallOfFameEntries * HallOfFameLineHeightPix;
	const int HallOfFameSpaceTopPix   = HallOfFameTitleTopPix + imageTitleHallOfFame.height;
	const int HallOfFameTopPix        = HallOfFameSpaceTopPix +
		(PlayAgainButtonTopPix - HallOfFameSpaceTopPix - HallOfFameListHeightPix)/2;

	// Hoeveel er in het invoerveld past. Ruimer dan de naam die eruit komt; wie
	// doortypt raakt de rest kwijt, en dat is nog altijd duidelijker dan een veld
	// dat halverwege niet meer reageert.
	const int MaxNofNameFieldChars = MaxNofNameChars + 4;

	class IGameOverDisplayControl
	{
		public:
			virtual IButton* addGameOverPageStartButtonListener(IButtonListener* pButtonListener) = 0;

			// De naaminvoer van UC03 stap 1. Deze pagina toont het veld en schuift
			// het toetsenbord in beeld; wat de toetsen betekenen beslist de
			// luisteraar, net als bij het GAME OVER-label van GameDisplayControl.
			// Vandaar dat het veld zelf wordt teruggegeven: daar staat de ingetypte
			// tekst in.
			virtual IInputfield* addNameInputfieldListener(IInputfieldListener* pInputfieldListener) = 0;
			virtual void showNameInput() = 0;

			// UC03 stap 2: de lijst, met de knop eronder.
			virtual void showHallOfFame(const HallOfFameData& hallOfFameData) = 0;

			virtual void showGameOverPage() = 0;
			virtual void hideGameOverPage() = 0;
	};

	class GameOverDisplayControl : public Task, public IGameOverDisplayControl, public IInputfieldListener
	{
		enum State
		{
			STATE_HIDDEN,
			STATE_SHOWN
		};

		enum Phase
		{
			PHASE_NAME_INPUT,		// invoerveld en toetsenbord
			PHASE_HALLOFFAME		// hall of fame en de knop
		};

		// De naam met zijn plaatsnummer ("1  marius") en de score staan in twee
		// aparte kolommen, elk met een eigen Text.
		//
		// Een regel met spaties ertussen werkt niet: font 2 is proportioneel, dus
		// een naam van tien smalle letters is smaller dan een van tien brede, en
		// dan zwabbert de scorekolom. Twee Texts, waarvan de ene links begint en de
		// andere rechts eindigt, staan wel recht onder elkaar.
		static const int MaxNofNameLineChars  = MaxNofNameChars + 6;
		static const int MaxNofScoreLineChars = 8;
		static const int HallOfFameFont       = 2;

		// Wat er tussen de twee kolommen zit.
		static const int HallOfFameGapPix = 20;

	private:
		IDisplay& display;
		TFT_eSPI_IFreeFonts& freeFonts;

		int screenWidth;
		int screenHeight;

		// Schermvullend, net als bij de andere twee pagina's. Daar leunt de
		// paginawissel op: wie in beeld komt wist wat de vorige achterliet.
		Panel<0> backgroundPanel;

		// "HALL OF FAME" boven de lijst (UC03 stap 2). Het GAME OVER-vignet hoort
		// niet meer op deze pagina: dat toont GameControl over het spelscherm heen,
		// zoals de uitzondering in UC04 zegt.
		Panel<0> hallOfFameTitlePanel;

		// --- fase NAAM ---
		Text<0> typeYourNameText;

		using TOUCHSCREENKEYBOARD = TouchscreenKeyboard<0 /*MaxNofChildren*/, 1 /*MaxNofKeyboardListeners*/>;
		TOUCHSCREENKEYBOARD touchscreenKeyboard;

		// Twee luisteraars: deze pagina zelf, die op elke aanslag opnieuw tekent, en
		// NameInputControl, die beslist wat de aanslagen betekenen.
		using INPUTFIELD = Inputfield<0 /*MaxNofChildren*/, 2 /*MaxNofInputfieldListeners*/, MaxNofNameFieldChars>;
		INPUTFIELD inputfield;

		// --- fase LIJST ---
		Text<0> arHallOfFameNames[NofHallOfFameEntries];
		Text<0> arHallOfFameScores[NofHallOfFameEntries];
		char arNameText[NofHallOfFameEntries][MaxNofNameLineChars + 1];
		char arScoreText[NofHallOfFameEntries][MaxNofScoreLineChars + 1];

		PlayAgainButton buttonStart;

		// Fakkels naast de "Play Again"-knop, net als op de startpagina. Ze horen
		// bij de lijstfase; tijdens het intypen staan ze uit.
		Panel<0> arTorchPanels[2];
		int32_t arIndexTorchFrame[2];

		Flag flagShowGameOverPage;
		Flag flagDeshowGameOverPage;
		Flag flagShowNameInput;
		Flag flagShowHallOfFame;
		Flag flagNewKey;
		Timer timerTorch;
		Pool<const char*> poolNewKey;

		// De lijst die getoond moet worden. Ook via een Pool: showHallOfFame()
		// wordt vanuit MainControl aangeroepen en de opbouw gebeurt hier.
		Pool<HallOfFameData> poolHallOfFame;

		State state;
		Phase phase;

		// Waar het toetsenbord staat als het uit beeld is, en als het in beeld is.
		int32_t yKeyboardDownPix;
		int32_t yKeyboardUpPix;
		bool bKeyboardIsUp;

	public:
		GameOverDisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber,
							IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
			display(display),
			freeFonts(freeFonts),
			screenWidth(0),
			screenHeight(0),

			touchscreenKeyboard("gameOverKeyboard", /*locPos*/Vec2(0, 500), CoordType::Promillage,
				/*size*/Vec2(1000, 500), CoordType::Promillage, Alignment::TopLeft,
				/*colFg*/(uint32_t)0x00222222, /*colBg*/0x00000000, TouchscreenKeyboardMode::LowercaseAlphabet),

			inputfield(&touchscreenKeyboard, InputfieldType::TextField, "", /*locPos*/Vec2(100, 250), CoordType::Promillage,
				/*size*/Vec2(800, 150), CoordType::Promillage, Alignment::TopLeft,
				/*colPanel*/(uint32_t)0x00303030, /*colBg*/0x00000000, /*colFont*/0x0000FFFF, InputfieldType::TextField),

			arIndexTorchFrame{0, 5},
			flagShowGameOverPage(this),
			flagDeshowGameOverPage(this),
			flagShowNameInput(this),
			flagShowHallOfFame(this),
			flagNewKey(this),
			timerTorch(this),
			state (STATE_HIDDEN),
			phase (PHASE_HALLOFFAME),
			yKeyboardDownPix(0),
			yKeyboardUpPix(0),
			bKeyboardIsUp(false)
		{
			Vec2 screenSize = display.getScreenSize();
			screenWidth  = screenSize.x;
			screenHeight = screenSize.y;

			display.addTouchListener(&buttonStart);
			display.addTouchListener(&inputfield);
			display.addTouchListener(&touchscreenKeyboard);

			// Deze pagina luistert zelf mee om op elke aanslag te hertekenen. Mag hier
			// al: inputNewKey zet alleen een Pool en een Flag, en de event group van de
			// taak bestaat vanaf de Task-constructor - dus ruim voor start().
			inputfield.addInputfieldListener(this);

			start();
		}

	IButton* addGameOverPageStartButtonListener(IButtonListener* pButtonListener)
	{
		buttonStart.addButtonListener(pButtonListener);
		return &buttonStart;
	}

	// Net als addGameOverLabelListener bij GameDisplayControl: de widget is van de
	// pagina, de betekenis van zijn gebeurtenissen niet.
	IInputfield* addNameInputfieldListener(IInputfieldListener* pInputfieldListener)
	{
		inputfield.addInputfieldListener(pInputfieldListener);
		return &inputfield;
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

	void showNameInput()
	{
		flagShowNameInput.set();
	}

	void showHallOfFame(const HallOfFameData& hallOfFameData)
	{
		poolHallOfFame.write(hallOfFameData);
		flagShowHallOfFame.set();
	}

	// --- IInputfieldListener ---
	// Deze pagina luistert alleen mee om te weten dat er iets veranderd is; wat er
	// getypt wordt gaat langs haar heen naar NameInputControl.
	/* override */ void inputNewKey(IInputfield* pInputfield, const char* strKey)
	{
		poolNewKey.write(strKey);
		flagNewKey.set();
	}

	/* override */ void inputfieldPressed(IInputfield* pInputfield)
	{
		// Het toetsenbord staat hier al omhoog zodra de fase begint; een tik op
		// het veld hoeft dus niets te doen.
	}

	/* override */ void outsideInputfieldPressed(IInputfield* pInputfield)
	{
		// Waarschijnlijk het toetsenbord zelf.
	}

	private:
		void initPanels()
		{
			backgroundPanel.setPanelProps("gameOverBackground", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(screenWidth, screenHeight), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00000000, /*colBg*/0x00000000);

			Vec2 titleSizePix(imageTitleHallOfFame.width, imageTitleHallOfFame.height);
			hallOfFameTitlePanel.setPanelProps
				("hallOfFameTitle",/*locPos*/Vec2((screenWidth - titleSizePix.x)/2, HallOfFameTitleTopPix),
				CoordType::Pixels,
				/*size*/titleSizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid,	/*colFg*/0x0000FFFF, /*colBg*/0x00000000);
			hallOfFameTitlePanel.setImage(&imageTitleHallOfFame);

			// TopMid: de oorsprong is het midden van de bovenrand van de pagina,
			// dus met locPos (0, y) staat de regel horizontaal gecentreerd op
			// hoogte y. Zie Text::calcOriginPix.
			typeYourNameText.setTextProps
				(/*locPos*/Vec2(0, 25), CoordType::Pixels, Alignment::TopMid,
				"TYPE UW NAAM", /*font*/2, /*color*/0x0000FFFF, /*fontScale*/1);

			// De plekken worden pas in fillHallOfFameLines() bepaald: daar zijn de
			// tekstbreedtes bekend, en daarmee waar het blok gecentreerd moet staan.
			for (int i = 0; i < NofHallOfFameEntries; i++)
			{
				arNameText[i][0] = '\0';
				arScoreText[i][0] = '\0';

				int32_t yPix = HallOfFameTopPix + i*HallOfFameLineHeightPix;

				arHallOfFameNames[i].setTextProps
					(/*locPos*/Vec2(0, yPix), CoordType::Pixels, Alignment::TopLeft,
					arNameText[i], HallOfFameFont, /*color*/0x0000FFFF, /*fontScale*/1);

				arHallOfFameScores[i].setTextProps
					(/*locPos*/Vec2(0, yPix), CoordType::Pixels, Alignment::TopLeft,
					arScoreText[i], HallOfFameFont, /*color*/0x0000FFFF, /*fontScale*/1);
			}

			initPlayAgainButton();
			TorchesDisplayHelpers::initTorches(arTorchPanels, screenWidth, PlayAgainButtonTopPix, imageButtonPlayAgain.height);
		}

		// Dezelfde knop als op de startpagina, maar met "PLAY AGAIN" erop en lager
		// op het scherm - UC03 stap 2a. Een eigen plaatje omdat het woord langer
		// is; de letters blijven even groot als op de startpagina.
		void initPlayAgainButton()
		{
			// Alles in pixels en niet in promillage. Bij een knop die uit een plaatje
			// bestaat is dat geen smaak: promillage wordt naar hele pixels afgerond,
			// en een panel dat daardoor een pixel groter uitvalt dan het plaatje laat
			// Panel::needsBackground() de vlakke colPanel-kleur er alsnog onder
			// schilderen. Die rand van een pixel is precies zichtbaar genoeg om lelijk
			// te zijn.
			Vec2 sizePix(imageButtonPlayAgain.width, imageButtonPlayAgain.height);

			// Het label is leeg omdat de tekst in het plaatje zit - met de schuine
			// kant en het verloop eromheen, die geen enkele fillRect kan maken.
			// colPanel en colFont blijven wel ingevuld: het plaatje is transparant
			// rond zijn afgeronde hoeken, dus zodra iemand de setImage() hieronder
			// weghaalt is er weer een gewone rode knop met tekst.
			buttonStart.setTouchScreenButtonProps
				("playAgain",/*locPos*/Vec2((screenWidth - sizePix.x)/2, PlayAgainButtonTopPix), CoordType::Pixels,
				/*size*/sizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF,
				/*fontScale*/ 1, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);

			// fontScale 1 en niet 0: bij 0 zoekt TouchscreenButton de grootste schaal
			// die past, en dat is werk zonder betekenis voor een leeg label.
			buttonStart.setImage(&imageButtonPlayAgain);
		}

		// Alleen de widgets van de gevraagde fase tekenbaar maken. setDrawable en
		// niet hide(): hide() zou een wisvlak insturen, en dat is bij een pagina
		// die als geheel opnieuw wordt getekend zowel overbodig als riskant - zie
		// GameDisplayControl::hidePage().
		void drawAndEnableWidgetsForPhase(Phase newPhase)
		{
			phase = newPhase;

			bool bName = (phase == PHASE_NAME_INPUT);

			// De titel hoort bij de lijst: in de naamfase staat het invoerveld op
			// die hoogte.
			hallOfFameTitlePanel.setDrawable(!bName, true);

			typeYourNameText.setDrawable(bName, true);
			inputfield.setDrawable(bName, true);
			touchscreenKeyboard.setDrawable(bName, true);

			for (int i = 0; i < NofHallOfFameEntries; i++)
			{
				arHallOfFameNames[i].setDrawable(!bName, true);
				arHallOfFameScores[i].setDrawable(!bName, true);
			}
			buttonStart.setDrawable(!bName, true);
			arTorchPanels[0].setDrawable(!bName, true);
			arTorchPanels[1].setDrawable(!bName, true);

			// De klok loopt alleen als de fakkels te zien zijn: elke tik is een
			// volledige hertekening, en tijdens het intypen valt er niets te
			// flakkeren.
			if (bName)
			{
				timerTorch.stop();
			}
			else
			{
				timerTorch.start(TorchesDisplayHelpers::nextTorchIntervalUs());
			}

			inputfield.enable(bName);
			touchscreenKeyboard.enable(bName && bKeyboardIsUp);
			buttonStart.enable(!bName);
		}

		void fillHallOfFameLines()
		{
			HallOfFameData snapshot;
			poolHallOfFame.read(snapshot);

			// Eerst de teksten, en meteen meten hoe breed de twee kolommen worden.
			int32_t maxNameWidthPix = 0;
			int32_t maxScoreWidthPix = 0;

			for (int i = 0; i < NofHallOfFameEntries; i++)
			{
				const HallOfFameEntry& entry = snapshot.arEntry[i];
				bool bEmpty = (entry.strName[0] == '\0');

				snprintf(arNameText[i], sizeof(arNameText[i]), "%d  %s",
					i + 1, bEmpty ? "-" : entry.strName);
				snprintf(arScoreText[i], sizeof(arScoreText[i]), "%s",
					bEmpty ? "-" : "");
				if (!bEmpty)
				{
					snprintf(arScoreText[i], sizeof(arScoreText[i]), "%d", (int)entry.score);
				}

				int32_t nameWidthPix = display.getTextWidth(arNameText[i], HallOfFameFont);
				if (nameWidthPix > maxNameWidthPix) { maxNameWidthPix = nameWidthPix; }

				int32_t scoreWidthPix = display.getTextWidth(arScoreText[i], HallOfFameFont);
				if (scoreWidthPix > maxScoreWidthPix) { maxScoreWidthPix = scoreWidthPix; }
			}

			// Het blok als geheel in het midden: de namen beginnen allemaal op
			// dezelfde x, de scores eindigen allemaal op dezelfde x, en die twee
			// kolommen samen staan gecentreerd. De breedte volgt uit de langste
			// naam, dus hij verschuift mee met wat er in de lijst staat.
			int32_t blockWidthPix = maxNameWidthPix + HallOfFameGapPix + maxScoreWidthPix;
			int32_t blockLeftPix  = (screenWidth - blockWidthPix)/2;
			if (blockLeftPix < 0) { blockLeftPix = 0; }
			int32_t scoreRightPix = blockLeftPix + blockWidthPix;

			for (int i = 0; i < NofHallOfFameEntries; i++)
			{
				int32_t yPix = HallOfFameTopPix + i*HallOfFameLineHeightPix;

				arHallOfFameNames[i].setText(arNameText[i]);
				arHallOfFameNames[i].setLocPos(Vec2(blockLeftPix, yPix), CoordType::Pixels, Alignment::TopLeft);

				// Rechts uitlijnen door per regel zijn eigen breedte af te trekken.
				// Dat is nauwkeuriger dan Alignment::TopRight, dat op de rand van de
				// ouder uitlijnt en niet op de kolom.
				int32_t scoreWidthPix = display.getTextWidth(arScoreText[i], HallOfFameFont);
				arHallOfFameScores[i].setText(arScoreText[i]);
				arHallOfFameScores[i].setLocPos(Vec2(scoreRightPix - scoreWidthPix, yPix),
												CoordType::Pixels, Alignment::TopLeft);
			}
		}

		// Schuift het toetsenbord van yFromPix naar yToPix. Overgenomen uit het
		// Inputfield-voorbeeld: een deadline per stap in plaats van een vaste
		// pauze, zodat de duur niet afhangt van de adapter - een present() van de
		// backbuffer kost al ~31 ms per stap.
		static const int64_t SlideDurationUs = 400000;

		void slideKeyboard(PageRoot<22 + 2*NofHallOfFameEntries>& page, int32_t yFromPix, int32_t yToPix)
		{
			const int32_t stepPix = 16;
			int32_t step = (yToPix > yFromPix) ? stepPix : -stepPix;

			touchscreenKeyboard.disable(true);

			int32_t distancePix = (yToPix > yFromPix) ? (yToPix - yFromPix) : (yFromPix - yToPix);
			int32_t nofSteps = (distancePix + stepPix - 1) / stepPix;
			if (nofSteps < 1) { nofSteps = 1; }

			int64_t timeStartUs = esp_timer_get_time();

			int32_t y = yFromPix;
			int32_t indexStep = 0;
			while (y != yToPix)
			{
				y += step;
				if (((step > 0) && (y > yToPix)) || ((step < 0) && (y < yToPix)))
				{
					y = yToPix;
				}

				// Met een backbuffer verzet moveTo() alleen de coordinaten van een
				// deel van de boom; de hele pagina moet daarna opnieuw. Zonder dat
				// blijft het toetsenbord op zijn oude plek in de buffer staan.
				touchscreenKeyboard.setLocPos(Vec2(0, y), CoordType::Pixels, Alignment::TopLeft);
				page.show(true);

				indexStep++;
				int64_t timeTargetUs = timeStartUs + (SlideDurationUs * indexStep) / nofSteps;
				int64_t timeLeftUs = timeTargetUs - esp_timer_get_time();
				if (timeLeftUs > 1000)
				{
					vTaskDelay((timeLeftUs / 1000) / portTICK_PERIOD_MS);
				}
			}

			bKeyboardIsUp = (yToPix < yFromPix);
			if (bKeyboardIsUp)
			{
				touchscreenKeyboard.enable(true);
			}
		}

		// Zie de toelichting bij GameDisplayControl::showPage/hidePage - dit is
		// dezelfde afspraak, en die moet voor alle drie de pagina's gelden.
		void showPage(PageRoot<22 + 2*NofHallOfFameEntries>& page)
		{
			page.setDrawable(true, true);
			drawAndEnableWidgetsForPhase(phase);		// zet meteen weer uit wat niet bij deze fase hoort
			page.show(true);
			page.enable(true);
		}

		void hidePage(PageRoot<22 + 2*NofHallOfFameEntries>& page)
		{
			page.setDrawable(false, true);
			page.hide(true);
			page.disable(true);
		}

		void initKeyboardPos()
		{
			// Het begint net onder het scherm, zodat het van onderaf in beeld kan
			// schuiven.
			yKeyboardDownPix = screenHeight;
			yKeyboardUpPix   = screenHeight - touchscreenKeyboard.getSizePix().y;
			touchscreenKeyboard.setLocPos(Vec2(0, yKeyboardDownPix), CoordType::Pixels, Alignment::TopLeft);
		}

		void defineDisplayHierarchy(IWidget& pageRoot)
		{
			pageRoot.addChildWidget(backgroundPanel);		// eerst, want alles hierna tekent erop.
			pageRoot.addChildWidget(hallOfFameTitlePanel);
			pageRoot.addChildWidget(typeYourNameText);

			for (int i = 0; i < NofHallOfFameEntries; i++)
			{
				pageRoot.addChildWidget(arHallOfFameNames[i]);
				pageRoot.addChildWidget(arHallOfFameScores[i]);
			}

			pageRoot.addChildWidget(buttonStart);
			pageRoot.addChildWidget(arTorchPanels[0]);
			pageRoot.addChildWidget(arTorchPanels[1]);

			// Het toetsenbord als laatste: het schuift over de rest heen.
			pageRoot.addChildWidget(inputfield);
			pageRoot.addChildWidget(touchscreenKeyboard);
		}

		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

			initPanels();

			PageRoot<22 + 2*NofHallOfFameEntries> pageGameOver("rootGameOverDisplay",display);
			defineDisplayHierarchy(pageGameOver);

			initKeyboardPos();

			drawAndEnableWidgetsForPhase(PHASE_HALLOFFAME);
			hidePage(pageGameOver);		// bij het eerste opstarten is er nog niets verloren.

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				waitAny(flagShowGameOverPage + flagDeshowGameOverPage +
				        flagShowNameInput + flagShowHallOfFame + flagNewKey + timerTorch);

				if (hasFired(timerTorch))
				{
					TorchesDisplayHelpers::stepTorches(arTorchPanels, arIndexTorchFrame);
					pageGameOver.show(true);
					timerTorch.start(TorchesDisplayHelpers::nextTorchIntervalUs());
				}

				if(hasFired(flagDeshowGameOverPage))
				{
					if (bKeyboardIsUp)
					{
						touchscreenKeyboard.setLocPos(Vec2(0, yKeyboardDownPix), CoordType::Pixels, Alignment::TopLeft);
						bKeyboardIsUp = false;
					}
					timerTorch.stop();
					hidePage(pageGameOver);
					state = STATE_HIDDEN;
				}
				else if(hasFired(flagShowGameOverPage))
				{
					if (state == STATE_HIDDEN)
					{
						showPage(pageGameOver);
						state = STATE_SHOWN;
					}
				}
				else if(hasFired(flagShowNameInput))
				{
					// Het veld leegmaken. Inputfield kent geen setTextValue, dus via
					// dezelfde weg als het toetsenbord: DEL tot er niets meer staat.
					// Dat zet onze eigen flagNewKey, en die halen we er meteen weer
					// af - anders tekent de lus hieronder de pagina nog een keer.
					for (int i = 0; i < MaxNofNameFieldChars; i++)
					{
						inputfield.keyPressed("DEL");
					}
					flagNewKey.clear();

					drawAndEnableWidgetsForPhase(PHASE_NAME_INPUT);
					pageGameOver.show(true);

					if (!bKeyboardIsUp)
					{
						slideKeyboard(pageGameOver, yKeyboardDownPix, yKeyboardUpPix);
					}
				}
				else if(hasFired(flagShowHallOfFame))
				{
					if (bKeyboardIsUp)
					{
						slideKeyboard(pageGameOver, yKeyboardUpPix, yKeyboardDownPix);
					}

					fillHallOfFameLines();
					drawAndEnableWidgetsForPhase(PHASE_HALLOFFAME);
					pageGameOver.show(true);
				}
				else if(hasFired(flagNewKey))
				{
					const char* strKey;
					poolNewKey.read(strKey);

					// Wat ENTER betekent gaat deze pagina niet aan - dat is UC03
					// stap 1b, en die zit in NameInputControl. Hier telt alleen dat
					// Inputfield::keyPressed er niets mee doet, dus dat er ook niets
					// te hertekenen valt. Elke andere toets verandert het veld wel,
					// en dan moet met een backbuffer de hele pagina opnieuw.
					if (strncmp(strKey, "ENTER", 5) != 0)
					{
						pageGameOver.show(true);
					}
				}
			}
		}
	}; // end class GameOverDisplayControl

};// end namespace pong_cleangui_dg_adv
