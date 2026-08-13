// by Marius Versteegen, 2026

#pragma once
#include <Arduino.h>
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>
#include <crt_Panel.h>
#include <crt_AsyncDisplay.h>
#include <crt_TouchscreenButton.h>
#include <crt_TFT_eSPI_DisplayAdapter.h>
#include <crt_TFT_eSPI_DisplayAdapter_PAL8.h>
#include "images/_pal8/_palette.h"
#include <crt_TFT_eSPI_FreeFonts.h>

// Legt vast wat show, enable en tekenen elk wel en niet doen, en meet dat op
// het bord zelf - zonder dat er iemand naar het scherm hoeft te kijken.
//
// Dat kan om twee redenen:
//
// - Aanrakingen zijn na te maken. ITouchListener is de complete grens tussen de
//   hardware en de widgets, en TouchscreenButton::touchPressed(pos) is publiek.
//   De touch-detector doet letterlijk niets anders dan die aanroep, dus code die
//   hem zelf doet loopt exact hetzelfde pad. De callback komt synchroon terug,
//   in deze taak, en dus is een gewone teller genoeg - geen Flags nodig.
//
// - Pixels zijn terug te lezen. De PAL8-adapter tekent in een schermvullende
//   8-bits buffer in DRAM waarin elke byte een palet-index is;
//   readPaletteIndexAt() haalt er een uit. Na waitUntilDrawn() staat daar wat
//   de widgets werkelijk hebben getekend.
//
// Draai dit voorbeeld en lees de log. De laatste regel is het oordeel.
//
// Dit harnas compileert tegen zowel de oude als de nieuwe API - zie cgRedraw()
// hieronder. Zo is het geschreven voordat de assen uit elkaar waren getrokken,
// en zo is het gedraaid: zeven van de zevenendertig controles faalden, precies
// de zeven die van tevoren waren aangewezen. Diezelfde zeven slagen nu, zonder
// dat er iets aan dit bestand is veranderd behalve het weghalen van hun
// bBrokenBeforeRefactor-vlag.
//
// Die dubbele compileerbaarheid blijft staan. Zonder haar meet dit harnas maar
// een van de twee kanten, en dan is het geen bewijs meer van een verschil.

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

namespace crt
{
	// De oude API kent geen manier om te hertekenen zonder vlaggen aan te
	// raken: show(true) is het enige dat tekent, en dat enablet meteen de hele
	// subboom. Zodra crt_Widget.h die assen uit elkaar heeft
	// (CLEANGUI_HAS_DRAW_ENABLE_SPLIT), wijzen deze helpers naar de echte
	// nieuwe aanroepen.
	//
	// Daarom staat dit hier en niet in de tests zelf: het bestand hoeft tussen
	// stap 2 en stap 7 niet te veranderen. De bibliotheek eronder verandert, en
	// de asserties gaan daardoor van rood naar groen. Dat is precies de meting.
	template <typename W> static void cgRedraw(W& widget)
	{
#ifdef CLEANGUI_HAS_DRAW_ENABLE_SPLIT
		widget.redraw(true);
#else
		widget.show(true);
#endif
	}

	template <typename W> static void cgMakeDrawable(W& widget, bool bIncludeChildren)
	{
#ifdef CLEANGUI_HAS_DRAW_ENABLE_SPLIT
		widget.makeDrawable(bIncludeChildren);
#else
		widget.setDrawable(true, bIncludeChildren);
#endif
	}

	template <typename W> static void cgMakeNonDrawable(W& widget, bool bIncludeChildren)
	{
#ifdef CLEANGUI_HAS_DRAW_ENABLE_SPLIT
		widget.makeNonDrawable(bIncludeChildren);
#else
		widget.setDrawable(false, bIncludeChildren);
#endif
	}

	class TestShowEnableSemantics : public Task, public IButtonListener
	{
		static const int CommandBufferSize = 1000;
		static const int MaxNofClientTasks = 10;
		static const int MaxNofTouchListeners = 20;

		using DISPLAYADAPTER = TFT_eSPI_DisplayAdapter_PAL8<MaxNofTouchListeners>;

		// Alle zeven uit _userPalette.h, dus elk een eigen index. Dat wordt bij
		// het opstarten gecontroleerd: vallen er twee op dezelfde index, dan
		// meet een pixeltest niet meer wie er getekend heeft.
		static const uint32_t COL_BG       = 0x00000000;	// zwart
		static const uint32_t COL_PROBE    = 0x00FF0000;	// rood
		static const uint32_t COL_PROBE_BG = 0x0000FF00;	// groen
		static const uint32_t COL_OUTER    = 0x000000FF;	// blauw
		static const uint32_t COL_INNER    = 0x0000FFFF;	// cyaan
		static const uint32_t COL_BTN      = 0x00808000;	// olijf
		static const uint32_t COL_TEXT     = 0x00FFFFFF;	// wit

	private:
		TFT_eSPI_FreeFonts<20> freeFonts;
		DISPLAYADAPTER tft_eSPI_DisplayAdapter;

		Panel<0> backgroundPanel;

		// Eigen vlak, en met opzet een colBg die van de achtergrond verschilt.
		// Daarmee is te zien wie er wist bij hide(): vult de panel zijn eigen
		// vlak met colBg (groen), of komt terug wat eronder zat (zwart)?
		Panel<0> probePanel;

		// outer -> inner -> knop. Drie niveaus, want twee vragen hebben er drie
		// nodig: knipt een niet-tekenbare ouder de subboom af, en werkt
		// groep-in-groep-gating.
		Panel<1> outerPanel;
		Panel<1> innerPanel;
		TouchscreenButton<0, 4> btnNested;

		TouchscreenButton<0, 4> btnMain;

		AsyncDisplay<CommandBufferSize, MaxNofClientTasks> asyncDisplay;

		int nofPressedMain, nofReleasedMain;
		int nofPressedNested, nofReleasedNested;

		int nofChecks, nofPassed, nofFailedUnexpected, nofFailedExpected, nofPassedUnexpected;

	public:
		TestShowEnableSemantics(const char* taskName, unsigned int taskPriority, unsigned int taskCoreNumber) :
			Task(taskName, taskPriority,
				12000 + sizeof(freeFonts) + sizeof(tft_eSPI_DisplayAdapter) + sizeof(asyncDisplay),
				taskCoreNumber),

			tft_eSPI_DisplayAdapter(freeFonts, "/TouchCalData1", false /*bRepeatCalibration*/),

			backgroundPanel("background", Vec2(0, 0), CoordType::Promillage,
				Vec2(1000, 1000), 0 /*cornerRadius*/, CoordType::Promillage, Alignment::TopLeft,
				COL_BG, COL_BG),

			// Alle maten in pixels en alle hoeken vierkant, zodat een pixeltest
			// op een uitgerekend punt kan kijken in plaats van op iets dat met
			// de schermmaat meeschuift.
			probePanel("probe", Vec2(20, 20), CoordType::Pixels,
				Vec2(80, 40), 0, CoordType::Pixels, Alignment::TopLeft,
				COL_PROBE, COL_PROBE_BG),

			outerPanel("outer", Vec2(20, 80), CoordType::Pixels,
				Vec2(200, 120), 0, CoordType::Pixels, Alignment::TopLeft,
				COL_OUTER, COL_OUTER),

			innerPanel("inner", Vec2(10, 10), CoordType::Pixels,
				Vec2(160, 100), 0, CoordType::Pixels, Alignment::TopLeft,
				COL_INNER, COL_INNER),

			btnNested("bnNested", Vec2(0, 50), CoordType::Pixels,
				Vec2(160, 50), 0, CoordType::Pixels, Alignment::TopLeft,
				COL_BTN, COL_BG, false /*bInvertedArea*/,
				"" /*geen label: dat zou over de pixels heen tekenen waar we naar kijken*/,
				2 /*font*/, COL_TEXT, 1 /*fontScale*/, Alignment::MidMid, 0, 0),

			btnMain("bnMain", Vec2(20, 220), CoordType::Pixels,
				Vec2(200, 50), 0, CoordType::Pixels, Alignment::TopLeft,
				COL_BTN, COL_BG, false, "", 2, COL_TEXT, 1, Alignment::MidMid, 0, 0),

			asyncDisplay("AsyncDisplay", 2 /*priority*/, 8000 /*stackBytes*/, ARDUINO_RUNNING_CORE,
				tft_eSPI_DisplayAdapter, 0 /*rotation*/, 2 /*fontTouchCalibration*/),

			nofPressedMain(0), nofReleasedMain(0),
			nofPressedNested(0), nofReleasedNested(0),
			nofChecks(0), nofPassed(0), nofFailedUnexpected(0), nofFailedExpected(0), nofPassedUnexpected(0)
		{
			start();
		}

	public:
		/* override */ void buttonPressed(IButton* pButton)
		{
			if (pButton == &btnMain)        { nofPressedMain++; }
			else if (pButton == &btnNested) { nofPressedNested++; }
		}

		/* override */ void buttonReleased(IButton* pButton)
		{
			if (pButton == &btnMain)        { nofReleasedMain++; }
			else if (pButton == &btnNested) { nofReleasedNested++; }
		}

	private:
		// --- meetgereedschap ------------------------------------------------

		// De tekencommando's gaan via een queue naar een andere taak. Zonder dit
		// meet je de vorige stand van de buffer.
		int32_t readPixel(int32_t xPix, int32_t yPix)
		{
			asyncDisplay.waitUntilDrawn();
			return tft_eSPI_DisplayAdapter.readPaletteIndexAt(xPix, yPix);
		}

		int32_t indexOf(uint32_t colour)
		{
			return tft_eSPI_DisplayAdapter.paletteIndexForColour(colour);
		}

		// bBrokenBeforeRefactor: deze controle hoort te falen zolang show() nog
		// tekent, enablet en de vlag zet in een keer. Faalt hij daarna nog
		// steeds, dan is de refactoring niet af; slaagt hij hier al, dan klopt
		// de aanname van de test niet en is dat net zo goed nieuws.
		void check(const char* name, bool bOk, bool bBrokenBeforeRefactor = false)
		{
			nofChecks++;
			const char* verdict;
			if (bOk && !bBrokenBeforeRefactor)      { nofPassed++;           verdict = "OK      "; }
			else if (bOk && bBrokenBeforeRefactor)  { nofPassedUnexpected++; verdict = "OK  (!) "; }
			else if (bBrokenBeforeRefactor)         { nofFailedExpected++;   verdict = "FOUT (v)"; }
			else                                    { nofFailedUnexpected++; verdict = "FOUT    "; }

			ESP_LOGI("SES", "%s %s", verdict, name);
		}

		void resetCounters()
		{
			nofPressedMain = nofReleasedMain = 0;
			nofPressedNested = nofReleasedNested = 0;
		}

		// Alles terug naar tekenbaar, ingeschakeld en getekend, zodat elke test
		// van dezelfde stand vertrekt. Bewust met de losse assen en niet met
		// show(true): show is een van de dingen die hier gemeten worden.
		template <typename PAGE> void resetScene(PAGE& page)
		{
			cgMakeDrawable(page, true);
			page.enable(true);
			cgRedraw(page);
			resetCounters();
			asyncDisplay.waitUntilDrawn();
		}

		// --- de tests --------------------------------------------------------

		template <typename PAGE> void testPaletteIsUsable(PAGE& page)
		{
			const uint32_t arColours[] = { COL_BG, COL_PROBE, COL_PROBE_BG, COL_OUTER, COL_INNER, COL_BTN, COL_TEXT };
			const int nofColours = sizeof(arColours) / sizeof(arColours[0]);

			bool bAllDistinct = true;
			for (int i = 0; i < nofColours; i++)
			{
				for (int j = i + 1; j < nofColours; j++)
				{
					if (indexOf(arColours[i]) == indexOf(arColours[j]))
					{
						ESP_LOGE("SES", "kleuren %d en %d vallen op dezelfde palet-index %d",
							i, j, (int)indexOf(arColours[i]));
						bAllDistinct = false;
					}
				}
			}
			check("palet: de zeven testkleuren hebben elk een eigen index", bAllDistinct);

			// Zonder backbuffer meet readPaletteIndexAt niets en is de helft van
			// dit harnas zinloos. Dan liever luid falen dan stil overslaan.
			check("adapter: er is een backbuffer om uit te lezen",
				tft_eSPI_DisplayAdapter.readPaletteIndexAt(0, 0) >= 0);

			check("adapter: buiten het scherm lezen geeft -1",
				tft_eSPI_DisplayAdapter.readPaletteIndexAt(-1, 0) == -1);
		}

		// --- as 1: aanraakbaarheid ------------------------------------------

		template <typename PAGE> void testEnableGating(PAGE& page)
		{
			resetScene(page);

			btnMain.touchPressed(Vec2(120, 245));
			check("enable: een ingeschakelde knop meldt de druk", nofPressedMain == 1);
			btnMain.touchReleased(Vec2(120, 245));
			check("enable: en meldt het loslaten", nofReleasedMain == 1);

			resetCounters();
			btnMain.disable(false);
			btnMain.touchPressed(Vec2(120, 245));
			btnMain.touchReleased(Vec2(120, 245));
			check("enable: een uitgeschakelde knop meldt niets", (nofPressedMain == 0) && (nofReleasedMain == 0));

			resetScene(page);
			btnMain.touchPressed(Vec2(5, 5));	// buiten de knop
			check("enable: een druk buiten de knop meldt niets", nofPressedMain == 0);

			// De knop die halverwege wordt uitgeschakeld: de druk mag niet
			// blijven hangen, want dan is hij na de volgende enable() dood.
			resetScene(page);
			btnMain.touchPressed(Vec2(120, 245));
			btnMain.disable(false);
			btnMain.enable(false);
			btnMain.touchReleased(Vec2(120, 245));
			check("enable: een druk overleeft een uit-en-weer-aan niet", !btnMain.isPressed());
			check("enable: en dat loslaten meldt dus niets", nofReleasedMain == 0);

			// En daarna moet hij het gewoon weer doen. Dit is de controle die
			// bewijst dat de knop niet dood is achtergelaten.
			resetCounters();
			btnMain.touchPressed(Vec2(120, 245));
			check("enable: en daarna doet de knop het weer", nofPressedMain == 1);
			btnMain.touchReleased(Vec2(120, 245));
		}

		template <typename PAGE> void testGroupGating(PAGE& page)
		{
			resetScene(page);
			outerPanel.disable(true);
			btnNested.touchPressed(Vec2(100, 165));
			check("groep: een uitgeschakelde groep dooft de knop erin",
				nofPressedNested == 0);
			btnNested.touchReleased(Vec2(100, 165));

			resetScene(page);
			outerPanel.enable(true);
			btnNested.touchPressed(Vec2(100, 165));
			check("groep: en weer inschakelen brengt hem terug", nofPressedNested == 1);
			btnNested.touchReleased(Vec2(100, 165));

			// Groep in groep: de binnenste uit, de buitenste aan.
			resetScene(page);
			innerPanel.disable(true);
			btnNested.touchPressed(Vec2(100, 165));
			check("groep-in-groep: de binnenste groep dooft hem ook",
				nofPressedNested == 0);
			btnNested.touchReleased(Vec2(100, 165));

			resetScene(page);
			check("groep-in-groep: na resetScene is hij weer ingeschakeld", btnNested.isEnabled());
		}

		// --- as 2: tekenen ---------------------------------------------------

		template <typename PAGE> void testDrawing(PAGE& page)
		{
			resetScene(page);

			check("tekenen: de achtergrond staat er waar niemand anders tekent",
				readPixel(200, 300) == indexOf(COL_BG));
			check("tekenen: de probe-panel staat er", readPixel(60, 40) == indexOf(COL_PROBE));
			check("tekenen: het buitenste paneel staat er", readPixel(205, 150) == indexOf(COL_OUTER));
			check("tekenen: het binnenste paneel staat er", readPixel(100, 110) == indexOf(COL_INNER));
			check("tekenen: de geneste knop staat er", readPixel(100, 165) == indexOf(COL_BTN));

			// makeNonDrawable wist niets - het is een vlag, geen opdracht.
			resetScene(page);
			cgMakeNonDrawable(probePanel, true);
			check("tekenbaar: de vlag uitzetten wist niets",
				readPixel(60, 40) == indexOf(COL_PROBE));

			// ... maar bij de volgende hertekening blijft hij weg. En dan hoort
			// er terug te komen wat eronder zat, niet zijn eigen colBg.
			page.repaintRegion(probePanel.getGlobPosPix(), probePanel.getSizePix());
			check("tekenbaar: en bij de volgende hertekening blijft hij weg",
				readPixel(60, 40) == indexOf(COL_BG));

			resetScene(page);
			check("tekenbaar: weer aanzetten brengt hem terug",
				readPixel(60, 40) == indexOf(COL_PROBE));
		}

		// Een ouder die zelf niet tekenbaar is, hoort de hele subboom af te
		// kappen. Vandaag niet: redrawRegion() kijkt naar _bVisible, en dat
		// staat na een page.show(true) overal op true - dus het kind tekent
		// vrolijk door onder een ouder die er niet meer hoort te zijn.
		template <typename PAGE> void testNonDrawableParentCutsSubtree(PAGE& page)
		{
			resetScene(page);

			cgMakeNonDrawable(outerPanel, false);	// alleen de ouder zelf
			page.repaintRegion(outerPanel.getGlobPosPix(), outerPanel.getSizePix());

			check("tekenbaar: een niet-tekenbare ouder tekent zelf niet",
				readPixel(205, 150) == indexOf(COL_BG));
			check("tekenbaar: en kapt zijn kinderen mee af",
				readPixel(100, 110) == indexOf(COL_BG));
			check("tekenbaar: ook de knop twee niveaus lager",
				readPixel(100, 165) == indexOf(COL_BG));

			resetScene(page);
			check("tekenbaar: en na het terugzetten staat alles er weer",
				(readPixel(205, 150) == indexOf(COL_OUTER)) &&
				(readPixel(100, 110) == indexOf(COL_INNER)));
		}

		// --- as 3: de assen mogen elkaar niet raken --------------------------

		template <typename PAGE> void testAxesAreIndependent(PAGE& page)
		{
			// Hertekenen mag de enable-toestand niet overschrijven. Dit is de
			// kern van de hele refactoring.
			resetScene(page);
			btnMain.disable(false);
			cgRedraw(btnMain);
			check("assen: hertekenen laat uitgeschakeld uitgeschakeld",
				!btnMain.isEnabled());

			resetCounters();
			btnMain.touchPressed(Vec2(120, 245));
			check("assen: en die knop meldt dus nog steeds niets",
				nofPressedMain == 0);
			btnMain.touchReleased(Vec2(120, 245));

			// ... en hij moet wel getekend zijn. Anders is de test hierboven te
			// halen door helemaal niets te doen.
			resetScene(page);
			btnMain.disable(false);
			cgMakeNonDrawable(btnMain, true);
			page.repaintRegion(btnMain.getGlobPosPix(), btnMain.getSizePix());
			cgMakeDrawable(btnMain, true);
			cgRedraw(btnMain);
			check("assen: en hertekenen tekent wel degelijk",
				readPixel(40, 235) == indexOf(COL_BTN));

			// show() mag de enable-as ook niet aanraken.
			resetScene(page);
			page.disable(true);
			page.show(true);
			check("assen: show() laat een uitgeschakelde pagina uitgeschakeld",
				!btnMain.isEnabled());

			// hide() evenmin.
			resetScene(page);
			page.enable(true);
			probePanel.hide(true);
			check("assen: hide() laat een ingeschakelde widget ingeschakeld",
				probePanel.isEnabled());
		}

		// hide() hoort terug te brengen wat eronder zat, niet zijn eigen colBg
		// over zijn vlak te vegen. Het verschil is meetbaar gemaakt: probePanel
		// heeft met opzet een colBg (groen) die van de achtergrond (zwart)
		// verschilt.
		template <typename PAGE> void testHideRestoresBackground(PAGE& page)
		{
			resetScene(page);
			check("hide: vooraf staat de probe er", readPixel(60, 40) == indexOf(COL_PROBE));

			probePanel.hide(true);
			check("hide: daarna is de probe weg", readPixel(60, 40) != indexOf(COL_PROBE));
			check("hide: en er staat wat eronder zat, niet zijn eigen colBg",
				readPixel(60, 40) == indexOf(COL_BG));

			resetScene(page);
			check("hide: en show() brengt hem terug", readPixel(60, 40) == indexOf(COL_PROBE));
		}

		// De aanleiding van de hele refactoring, van begin tot eind: een knop
		// die in zijn eigen indrukbehandeling de pagina wegzet en weer opbouwt,
		// moet daarna nog steeds netjes zijn loslaten melden.
		template <typename PAGE> void testButtonThatRedrawsItsOwnPage(PAGE& page)
		{
			resetScene(page);

			btnMain.touchPressed(Vec2(120, 245));
			check("scenario: de druk komt binnen", nofPressedMain == 1);

			// Dit is wat zo'n listener doet: de pagina opnieuw opbouwen terwijl
			// de vinger er nog op ligt.
			cgRedraw(page);

			btnMain.touchReleased(Vec2(120, 245));
			check("scenario: en het loslaten komt ook binnen, na een hertekening",
				nofReleasedMain == 1);
		}

		void main()
		{
			vTaskDelay(1000);

#ifdef LOAD_GFXFF
			freeFonts.setFreeFont(10, FSB9);
			freeFonts.setFreeFont(12, FSB12);
#endif
			IDisplay& display = asyncDisplay;
			tft_eSPI_DisplayAdapter.setPalette(arPalette);

			PageRoot<10> page("root", display);

			asyncDisplay.addTouchListener(&btnMain);
			asyncDisplay.addTouchListener(&btnNested);
			btnMain.addButtonListener(this);
			btnNested.addButtonListener(this);

			page.addChildWidget(backgroundPanel);	// als eerste, dus onderop
			page.addChildWidget(probePanel);
			page.addChildWidget(outerPanel);
			outerPanel.addChildWidget(innerPanel);
			innerPanel.addChildWidget(btnNested);
			page.addChildWidget(btnMain);
			page.show(true);
			asyncDisplay.waitUntilDrawn();

			ESP_LOGI("SES", "=== ShowEnableSemantics ===");
#ifdef CLEANGUI_HAS_DRAW_ENABLE_SPLIT
			ESP_LOGI("SES", "bibliotheek: NA de refactoring (show/enable/draw zijn los)");
#else
			ESP_LOGI("SES", "bibliotheek: VOOR de refactoring (show tekent, enablet en zet de vlag)");
#endif

			testPaletteIsUsable(page);
			testEnableGating(page);
			testGroupGating(page);
			testDrawing(page);
			testNonDrawableParentCutsSubtree(page);
			testAxesAreIndependent(page);
			testHideRestoresBackground(page);
			testButtonThatRedrawsItsOwnPage(page);

			ESP_LOGI("SES", "---------------------------");
			ESP_LOGI("SES", "%d controles: %d geslaagd, %d gefaald (%d daarvan verwacht), %d onverwacht geslaagd",
				nofChecks, nofPassed + nofPassedUnexpected,
				nofFailedUnexpected + nofFailedExpected, nofFailedExpected, nofPassedUnexpected);

			// De regel waar een script naar kijkt. GROEN betekent: geen enkele
			// verrassing. Voor de refactoring hoort dat ROOD te zijn, met de
			// gefaalde controles allemaal in de verwachte lijst.
			if ((nofFailedUnexpected == 0) && (nofPassedUnexpected == 0) && (nofFailedExpected == 0))
			{
				ESP_LOGI("SES", "EINDOORDEEL: GROEN - alles klopt");
			}
			else if ((nofFailedUnexpected == 0) && (nofPassedUnexpected == 0))
			{
				ESP_LOGI("SES", "EINDOORDEEL: VERWACHT ROOD - %d controles falen zoals voorspeld; dat is de bug",
					nofFailedExpected);
			}
			else
			{
				ESP_LOGI("SES", "EINDOORDEEL: ROOD - %d onverwacht gefaald, %d onverwacht geslaagd",
					nofFailedUnexpected, nofPassedUnexpected);
			}
			ESP_LOGI("SES", "EINDE TEST");

			while (true)
			{
				vTaskDelay(1000);
			}
		}
	};
};
