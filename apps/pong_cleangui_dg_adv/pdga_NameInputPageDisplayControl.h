// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>
// Hier en niet in crt_TouchscreenKeyboard.h, waar hij hoort: die header noemt
// IKeyboardListener wel maar includet hem niet, dus hij moet ervoor staan.
#include <crt_IKeyboardListener.h>
#include <crt_TouchscreenKeyboard.h>
#include <crt_InputField.h>
#include <string.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include "pdga_HallOfFame.h"		// alleen voor MaxNofNameChars.
#include "pdga_INameInputPageListener.h"

// De DisplayControl van de naaminvoerpagina (UC03 stap 1), met een eigen PageRoot
// en een eigen taak. Zie pdga_GameDisplayControl.h voor waarom het verbergen niets
// tekent.
//
// De pagina toont een invoerveld met "TYPE UW NAAM" erboven, en een toetsenbord
// dat van onderaf in beeld schuift. Wat de toetsen betekenen beslist ze niet: dat
// is NameInputControl, net zoals GameControl beslist wat een tik op het GAME
// OVER-label betekent.
//
// Dit stond eerder als tweede fase in GameOverDisplayControl, op dezelfde PageRoot
// als de hall of fame. Dat kon, maar het kostte wel wat: twee fasen die elk de
// widgets van de ander tekenbaar en aanraakbaar uit moesten zetten, een
// showNameInput() naast een showHallOfFame() op een pagina die niet allebei
// tegelijk kon zijn, en een lijst met vlaggen waarvan per toestand maar een deel
// werd afgewacht - met clears om te voorkomen dat de rest een ronde later alsnog
// vuurde. Twee pagina's die elk maar een ding tonen hebben dat niet: hier is
// tekenbaar zijn hetzelfde als in beeld zijn.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// Zie pdga_BallControl.h: logger komt uit namespace crt, niet uit deze.

	// Hoeveel er in het invoerveld past. Ruimer dan de naam die eruit komt; wie
	// doortypt raakt de rest kwijt, en dat is nog altijd duidelijker dan een veld
	// dat halverwege niet meer reageert.
	const int MaxNofNameFieldChars = MaxNofNameChars + 4;

	// Wat NameInputControl van deze pagina mag vragen, staat in het publieke blok
	// na de constructor. Zie de toelichting boven
	// pdga_GameDisplayControl.h::GameDisplayControl voor waarom daar geen
	// interface meer boven staat.
	//
	// IInputfieldListener blijft wel: dat is een CleanGUI-luisteraar, en die heeft
	// wel degelijk meerdere implementaties - deze pagina luistert mee om te
	// hertekenen, NameInputControl om te weten wat er getypt is.
	class NameInputPageDisplayControl : public Task, public IInputfieldListener
	{
		enum State
		{
			STATE_HIDDEN,		// pagina uit beeld
			STATE_SHOWN			// in beeld: het invoerveld met het toetsenbord ervoor
		};

		// Vier kinderen (achtergrond, kopregel, veld, toetsenbord); de rest is lucht
		// voor wie er een regel bij zet.
		static const size_t MaxNofPageChildren = 8;
		using PAGE = PageRoot<MaxNofPageChildren>;

	private:
		IDisplay& display;
		TFT_eSPI_IFreeFonts& freeFonts;

		int screenWidth;
		int screenHeight;

		// Schermvullend, net als bij de andere pagina's. Daar leunt de paginawissel
		// op: wie in beeld komt wist wat de vorige achterliet.
		Panel<0> backgroundPanel;

		Text<0> typeYourNameText;

		using TOUCHSCREENKEYBOARD = TouchscreenKeyboard<0 /*MaxNofChildren*/, 1 /*MaxNofKeyboardListeners*/>;
		TOUCHSCREENKEYBOARD touchscreenKeyboard;

		// Twee luisteraars: deze pagina zelf, die op elke aanslag opnieuw tekent, en
		// NameInputControl, die beslist wat de aanslagen betekenen.
		using INPUTFIELD = Inputfield<0 /*MaxNofChildren*/, 2 /*MaxNofInputfieldListeners*/, MaxNofNameFieldChars>;
		INPUTFIELD inputfield;

		Flag flagShowNameInputPage;
		Flag flagHideNameInputPage;
		Flag flagNewKey;
		Pool<const char*> poolNewKey;

		// Wie er te horen krijgt dat de pagina echt weg is; zie
		// pdga_INameInputPageListener.h.
		INameInputPageListener* pNameInputPageListener;

		State state;

		// Waar het toetsenbord staat als het uit beeld is, en als het in beeld is.
		int32_t yKeyboardDownPix;
		int32_t yKeyboardUpPix;
		bool bKeyboardIsUp;

		// Eenmalig gevraagd in main(); bepaalt hoe het toetsenbord schuift en
		// wat een toetsaanslag hertekent.
		bool bDisplayIsBuffered;

	public:
		NameInputPageDisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber,
							IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
			display(display),
			freeFonts(freeFonts),
			screenWidth(0),
			screenHeight(0),

			touchscreenKeyboard("nameInputKeyboard", /*locPos*/Vec2(0, 500), CoordType::Promillage,
				/*size*/Vec2(1000, 500), CoordType::Promillage, Alignment::TopLeft,
				/*colFg*/(uint32_t)0x00222222, /*colBg*/0x00000000, TouchscreenKeyboardMode::LowercaseAlphabet),

			inputfield(&touchscreenKeyboard, InputfieldType::TextField, "", /*locPos*/Vec2(100, 250), CoordType::Promillage,
				/*size*/Vec2(800, 150), CoordType::Promillage, Alignment::TopLeft,
				/*colPanel*/(uint32_t)0x00303030, /*colBg*/0x00000000, /*colFont*/0x0000FFFF, InputfieldType::TextField),

			flagShowNameInputPage(this),
			flagHideNameInputPage(this),
			flagNewKey(this),
			pNameInputPageListener(nullptr),
			state (STATE_HIDDEN),
			yKeyboardDownPix(0),
			yKeyboardUpPix(0),
			bKeyboardIsUp(false),
			bDisplayIsBuffered(false)
		{
			Vec2 screenSize = display.getScreenSize();
			screenWidth  = screenSize.x;
			screenHeight = screenSize.y;

			display.addTouchListener(&inputfield);
			display.addTouchListener(&touchscreenKeyboard);

			// Deze pagina luistert zelf mee om op elke aanslag te hertekenen. Mag hier
			// al: inputNewKey zet alleen een Pool en een Flag, en de event group van de
			// taak bestaat vanaf de Task-constructor - dus ruim voor start().
			inputfield.addInputfieldListener(this);

			start();
		}

	// Net als addGameOverLabelListener bij GameDisplayControl: de widget is van de
	// pagina, de betekenis van zijn gebeurtenissen niet. Het veld zelf komt terug
	// omdat daar de ingetypte tekst in staat.
	IInputfield* addNameInputfieldListener(IInputfieldListener* pInputfieldListener)
	{
		inputfield.addInputfieldListener(pInputfieldListener);
		return &inputfield;
	}

	// De volgende twee wissen elkaars vlag: alleen de laatste wens telt.
	void showNameInputPage()
	{
		flagHideNameInputPage.clear();
		flagShowNameInputPage.set();
	}

	// Deze aanroep zet het verbergen alleen in gang. Wanneer het klaar is meldt de
	// pagina zelf, via nameInputPageHidden() op de luisteraar hieronder - want het
	// toetsenbord schuift eerst weer uit beeld, en dat is tekenwerk dat pas een
	// paar honderd milliseconden na deze aanroep af is. Wie ondertussen de
	// volgende pagina laat tekenen, staat er dwars doorheen, en dat is precies wat
	// de regel "een PageRoot tegelijk in beeld" verbiedt (zie de kop van
	// crt_PageRoot.h).
	void hideNameInputPage()
	{
		flagShowNameInputPage.clear();
		flagHideNameInputPage.set();
	}

	// Wie op die melding wacht. Net als BallControl::setBallListener(): de pagina
	// weet niet wie er luistert en wat de melding daar betekent.
	void setNameInputPageListener(INameInputPageListener* pNameInputPageListener)
	{
		this->pNameInputPageListener = pNameInputPageListener;
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
		// Het toetsenbord staat hier al omhoog zodra de pagina in beeld komt; een
		// tik op het veld hoeft dus niets te doen.
	}

	/* override */ void outsideInputfieldPressed(IInputfield* pInputfield)
	{
		// Waarschijnlijk het toetsenbord zelf.
	}

	private:
		void initPanels()
		{
			backgroundPanel.setPanelProps("nameInputBackground", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(screenWidth, screenHeight), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00000000, /*colBg*/0x00000000);

			// TopMid: de oorsprong is het midden van de bovenrand van de pagina,
			// dus met locPos (0, y) staat de regel horizontaal gecentreerd op
			// hoogte y. Zie Text::calcOriginPix.
			typeYourNameText.setTextProps
				(/*locPos*/Vec2(0, 25), CoordType::Pixels, Alignment::TopMid,
				"TYPE UW NAAM", /*font*/2, /*color*/0x0000FFFF, /*fontScale*/1);
		}

		void initKeyboardPos()
		{
			// Het begint net onder het scherm, zodat het van onderaf in beeld kan
			// schuiven.
			yKeyboardDownPix = screenHeight;
			yKeyboardUpPix   = screenHeight - touchscreenKeyboard.getSizePix().y;
			touchscreenKeyboard.setLocPos(Vec2(0, yKeyboardDownPix), CoordType::Pixels, Alignment::TopLeft);
		}

		// Schuift het toetsenbord van yFromPix naar yToPix. Overgenomen uit het
		// Inputfield-voorbeeld: een deadline per stap in plaats van een vaste
		// pauze, zodat de duur niet afhangt van de adapter - een present() van de
		// backbuffer kost al ~31 ms per stap.
		static const int64_t SlideDurationUs = 400000;

		void doSlideKeyboard(PAGE& page, int32_t yFromPix, int32_t yToPix)
		{
			const int32_t stepPix = 16;
			int32_t step = (yToPix > yFromPix) ? stepPix : -stepPix;

			// Zolang het schuift mag er geen aanslag binnenkomen. Het hertekenen
			// per stap raakt geen enkele vlag aan, dus deze disable houdt het hele
			// eind vol - tot de enable onderaan.
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

				if (bDisplayIsBuffered)
				{
					// Met een backbuffer verzet moveTo() alleen de coordinaten van
					// een deel van de boom; de hele pagina moet daarna opnieuw.
					// Zonder dat blijft het toetsenbord op zijn oude plek in de
					// buffer staan.
					touchscreenKeyboard.setLocPos(Vec2(0, y), CoordType::Pixels, Alignment::TopLeft);
					page.redraw(true);
				}
				else
				{
					// Zonder buffer is diezelfde page.redraw(true) per stap een
					// zichtbare wis van het hele scherm. moveTo() tekent het
					// toetsenbord op zijn nieuwe plek en laat de boom alleen de
					// vrijgekomen strook herstellen - zoals in het
					// Inputfield-voorbeeld.
					touchscreenKeyboard.moveTo(Vec2(0, y), CoordType::Pixels);
				}

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
				// Na een schuifbeweging omhoog is dit wat de veertig toetsen terugzet.
				touchscreenKeyboard.enable(true);
			}
		}

		// Zie de toelichting bij GameDisplayControl::showPage/hidePage - dit is
		// dezelfde afspraak, en die moet voor alle pagina's gelden.
		void showPage(PAGE& page)
		{
			page.makeDrawable(true);
			page.enable(true);

			// ... op het toetsenbord na: dat staat nog onder het scherm en blijft
			// doof tot het er ligt. De schuifbeweging zet het aan.
			touchscreenKeyboard.disable(true);

			page.redraw(true);
		}

		void hidePage(PAGE& page)
		{
			// Zonder ook maar een tekencommando, want een andere pagina schrijft al
			// in dezelfde commandobuffer (zie de kop van dit bestand). Daarom geen
			// hide(): die zou vanaf de wortel over dit vlak heen gaan.
			// De disable staat er los bij; niet-tekenbaar maakt niet doof.
			page.makeNonDrawable(true);
			page.disable(true);
		}

		void enterHidden(PAGE& page)
		{
			// Eerst het toetsenbord weer naar beneden, en dat is nog gewoon tekenwerk
			// op een pagina die in beeld staat. Pas als het eronder ligt, verdwijnt
			// ze - en pas daarna mag de volgende pagina beginnen. Vandaar de melding
			// helemaal onderaan.
			if (bKeyboardIsUp)
			{
				doSlideKeyboard(page, yKeyboardUpPix, yKeyboardDownPix);
			}

			hidePage(page);
			signalPageHidden();
		}

		// De melding waar de aanroeper van hideNameInputPage() op staat te wachten.
		// Hij moet uit elke tak komen die een hide-wens verwerkt: blijft hij een keer
		// uit, dan is dat geen gemiste hertekening maar een taak die nooit meer
		// verdergaat.
		//
		// Dit draait in de taak van deze pagina, niet in die van de luisteraar. Wat
		// die ermee doet is aan hem; NameInputControl zet er zijn eigen vlag mee.
		void signalPageHidden()
		{
			if (pNameInputPageListener != nullptr)
			{
				pNameInputPageListener->nameInputPageHidden();
			}
		}

		void defineDisplayHierarchy(IWidget& pageRoot)
		{
			pageRoot.addChildWidget(backgroundPanel);		// eerst, want alles hierna tekent erop.
			pageRoot.addChildWidget(typeYourNameText);

			// Het toetsenbord als laatste: het schuift over de rest heen.
			pageRoot.addChildWidget(inputfield);
			pageRoot.addChildWidget(touchscreenKeyboard);
		}

		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

			initPanels();
			bDisplayIsBuffered = display.isBuffered();

			PAGE pageNameInput("rootNameInputPageDisplay",display);
			defineDisplayHierarchy(pageNameInput);

			initKeyboardPos();
			hidePage(pageNameInput);	// bij het eerste opstarten is er nog niets verloren.

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				switch(state)
				{
					case STATE_HIDDEN:
						// Ook op de hide-wens wachten, en dat is hier geen netheid: wie
						// verbergt staat op de melding te wachten, dus een wens die
						// niemand opmerkt is een taak die nooit meer verdergaat. Dat de
						// pagina al uit beeld staat maakt daarvoor niets uit.
						waitAny(flagShowNameInputPage + flagHideNameInputPage);

						if(hasFired(flagHideNameInputPage))
						{
							signalPageHidden();
						}
						else if(hasFired(flagShowNameInputPage))
						{
							// Het veld leegmaken voordat het in beeld komt. clearInput()
							// en niet een lus met "DEL": die stuurde bij elke stap een
							// inputNewKey naar alle luisteraars, voor toetsen die niemand
							// heeft ingedrukt. Zie crt_InputField.h.
							inputfield.clearInput();

							// En de vlag erachteraan, want tussen de ENTER van de vorige
							// ronde en het verbergen kan er nog een toets zijn gevallen.
							// Die hoort niet bij deze naam. clearInput() zelf meldt niets:
							// het is juist de lus die dat wel deed.
							flagNewKey.clear();

							showPage(pageNameInput);
							doSlideKeyboard(pageNameInput, yKeyboardDownPix, yKeyboardUpPix);
							state = STATE_SHOWN;
						}
						break;

					case STATE_SHOWN:
						waitAny(flagHideNameInputPage + flagShowNameInputPage + flagNewKey);

						if(hasFired(flagHideNameInputPage))
						{
							enterHidden(pageNameInput);
							state = STATE_HIDDEN;
						}
						else if(hasFired(flagShowNameInputPage))
						{
							// al in beeld: niets te doen. doQueryName() staat tot ENTER
							// stil, dus een tweede vraag kan er sowieso niet zijn.
						}
						else if(hasFired(flagNewKey))
						{
							const char* strKey;
							poolNewKey.read(strKey);

							// Wat ENTER betekent gaat deze pagina niet aan - dat is UC03
							// stap 1b, en die zit in NameInputControl. Hier telt alleen
							// dat Inputfield::keyPressed er niets mee doet, dus dat er ook
							// niets te hertekenen valt. Elke andere toets verandert het
							// veld wel, en dan moet met een backbuffer de hele pagina
							// opnieuw.
							//
							// Zonder backbuffer niet: Inputfield::keyPressed heeft het
							// label dan al zelf bijgewerkt (updateButtonLabel), en een
							// hertekening van de hele pagina is daar alleen een wis.
							if (bDisplayIsBuffered && (strncmp(strKey, "ENTER", 5) != 0))
							{
								pageNameInput.redraw(true);
							}
						}
						break;

					default:
						// De wachtoproep zit binnen de switch, dus een toestand die hier
						// niet staat laat de lus vrij rondtollen - en assert is in een
						// release-build leeg. Terug naar een toestand die wel blokkeert.
						assert(false);
						state = STATE_HIDDEN;
						break;
				}
			}
		}
	}; // end class NameInputPageDisplayControl

};// end namespace pong_cleangui_dg_adv
