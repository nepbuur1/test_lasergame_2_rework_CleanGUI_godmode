// by Marius Versteegen, 2026

#pragma once
#include <stdlib.h>
#include <crt_CleanGUI.h>
#include <crt_TouchscreenButton.h>

#include "images/_pal8/_allImages.h"

// De Play-knop. Hij staat op de startpagina en op de game-over-pagina, en die
// twee horen precies dezelfde knop te tonen: hij verspringt anders bij het
// wisselen van pagina, en dat is nu juist wat je niet ziet zolang je de twee
// pagina's ieder apart bekijkt. Vandaar deze ene plek in plaats van twee keer
// dezelfde regels.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	typedef TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> PlayButton;

	// Gecentreerd tussen het regeltje onder de titel (dat op y=124 eindigt) en de
	// onderkant van het scherm: (124+240)/2 = 182, en de knop is 54 hoog.
	const int PlayButtonTopPix = 155;

	// De fakkels naast de knoppen. Ze staan met hun midden op dezelfde hoogte als
	// het midden van de knop, zodat ze meebewegen als die verschuift.
	const int TorchWidthPix  = 36;
	const int TorchHeightPix = 72;
	const int TorchMarginPix = 14;		// vanaf de schermrand

	// Hoe snel de vlam flakkert - en met opzet niet met een vaste stap.
	//
	// Een periodieke timer geeft precies wat je bij vuur niet wilt: een cyclus
	// die je na een paar rondes herkent. Door de wachttijd per stap te loten
	// blijft de gemiddelde snelheid gelijk, maar hoort er geen ritme meer in.
	// Vandaar een eenmalige timer die na elke tik opnieuw wordt gezet.
	//
	// De ondergrens is er omdat elke stap een volledige hertekening is: bij een
	// backbuffer kost die ~31 ms, en sneller aftikken dan dat levert alleen maar
	// werk op dat het scherm niet bijhoudt.
	const int TorchFrameIntervalMinMs = 45;
	const int TorchFrameIntervalMaxMs = 130;

	inline uint64_t nextTorchIntervalUs()
	{
		int spanMs = TorchFrameIntervalMaxMs - TorchFrameIntervalMinMs + 1;
		return (uint64_t)(TorchFrameIntervalMinMs + (rand() % spanMs)) * 1000ULL;
	}

	// Waar de "Play Again"-knop van de game-over-pagina staat: onder de hall of
	// fame. Een eigen hoogte, want die pagina heeft er meer boven staan.
	const int PlayAgainButtonTopPix = 176;

	inline void initPlayButton(PlayButton& button, int screenWidth)
	{
		// Alles in pixels en niet in promillage. Bij een knop die uit een plaatje
		// bestaat is dat geen smaak: promillage wordt naar hele pixels afgerond,
		// en een panel dat daardoor een pixel groter uitvalt dan het plaatje laat
		// Panel::needsBackground() de vlakke colPanel-kleur er alsnog onder
		// schilderen. Die rand van een pixel is precies zichtbaar genoeg om lelijk
		// te zijn.
		Vec2 sizePix(imageButtonPlay.width, imageButtonPlay.height);

		// Het label is leeg omdat "PLAY" in het plaatje zit - met de schuine kant
		// en het verloop eromheen, die geen enkele fillRect kan maken. colPanel en
		// colFont blijven wel ingevuld: het plaatje is transparant rond zijn
		// afgeronde hoeken, dus zodra iemand de setImage() hieronder weghaalt is
		// er weer een gewone rode knop met tekst.
		button.setTouchScreenButtonProps
			("play",/*locPos*/Vec2((screenWidth - sizePix.x)/2, PlayButtonTopPix), CoordType::Pixels,
			/*size*/sizePix, /*cornerRadius*/0, CoordType::Pixels,
			Alignment::MidMid, /*colPanel*/0x00FF0000, /*colBg*/0x00000000,
			/*bInvertedArea*/false, "", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF,
			/*fontScale*/ 1, Alignment::MidMid,
			/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);

		// fontScale 1 en niet 0: bij 0 zoekt TouchscreenButton de grootste schaal
		// die past, en dat is werk zonder betekenis voor een leeg label.
		button.setImage(&imageButtonPlay);
	}

	// Dezelfde knop, maar met "PLAY AGAIN" erop en lager op het scherm - UC03
	// stap 2a. Een eigen plaatje omdat het woord langer is; de letters blijven
	// even groot als op de startpagina.
	inline void initPlayAgainButton(PlayButton& button, int screenWidth)
	{
		Vec2 sizePix(imageButtonPlayAgain.width, imageButtonPlayAgain.height);

		button.setTouchScreenButtonProps
			("playAgain",/*locPos*/Vec2((screenWidth - sizePix.x)/2, PlayAgainButtonTopPix), CoordType::Pixels,
			/*size*/sizePix, /*cornerRadius*/0, CoordType::Pixels,
			Alignment::MidMid, /*colPanel*/0x00FF0000, /*colBg*/0x00000000,
			/*bInvertedArea*/false, "", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF,
			/*fontScale*/ 1, Alignment::MidMid,
			/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);

		button.setImage(&imageButtonPlayAgain);
	}

	// Twee fakkels, links en rechts, met hun midden op de hoogte van het midden
	// van de knop ernaast.
	inline void initTorches(Panel<0>* arTorchPanels, int screenWidth,
	                        int buttonTopPix, int buttonHeightPix)
	{
		Vec2 sizePix(TorchWidthPix, TorchHeightPix);
		int topPix = buttonTopPix + (buttonHeightPix - TorchHeightPix)/2;

		int arLeftPix[2] = { TorchMarginPix, screenWidth - TorchWidthPix - TorchMarginPix };

		for (int i = 0; i < 2; i++)
		{
			arTorchPanels[i].setPanelProps("torch", /*locPos*/Vec2(arLeftPix[i], topPix),
				CoordType::Pixels, /*size*/sizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colFg*/0x00FFA020, /*colBg*/0x00000000);
			arTorchPanels[i].setImage(&imageTorch);

			// Zonder een gekozen frame tekent Panel de HELE sheet - vijf fakkels
			// naast elkaar.
			arTorchPanels[i].setImageFrame(0, 0);
		}
	}

	// Een stap in de vlam: elke fakkel schuift een of twee cellen op of neer.
	//
	// Een dronkemanswandeling, en geen vrije loting. De tien cellen zijn standen
	// van dezelfde vlam over een cyclus, dus cel 0 en cel 5 schelen flink in
	// tonghoogte - vrij loten leest daardoor eerder als flikkeren dan als
	// branden. Een stap van hooguit twee houdt de vlam samenhangend, terwijl de
	// richting die elke keer kan omslaan het ritme er net zo goed uit haalt.
	//
	// Een stap van nul komt niet voor: twee keer dezelfde cel leest als een
	// haperende vlam, en dat valt meer op dan welke sprong ook. Elke fakkel
	// wandelt apart, zodat ze niet als tweeling flakkeren - waar een vaste
	// volgorde daar nog een halve cyclus verschil voor nodig had.
	inline void stepTorches(Panel<0>* arTorchPanels, int32_t* arIndexTorchFrame)
	{
		int32_t nofCells = imageTorch.nofCols;

		for (int i = 0; i < 2; i++)
		{
			// rand()%4 geeft 0..3; dat wordt -2, -1, +1, +2.
			int32_t step = (int32_t)(rand() % 4) - 2;
			if (step >= 0)
			{
				step++;
			}

			// Er eerst nofCells bij optellen: % levert in C++ een negatief
			// resultaat bij een negatief linkerlid, en dat is geen celindex.
			int32_t indexNext = (arIndexTorchFrame[i] + step + nofCells) % nofCells;

			arIndexTorchFrame[i] = indexNext;
			arTorchPanels[i].setImageFrame(0, (int)indexNext);
		}
	}
};// end namespace pong_cleangui_dg_adv
