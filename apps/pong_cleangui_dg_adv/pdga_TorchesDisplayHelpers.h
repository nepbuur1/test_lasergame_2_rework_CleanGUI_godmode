// by Marius Versteegen, 2026

#pragma once
#include <stdlib.h>
#include <crt_CleanGUI.h>

#include "images/_pal8/_allImages.h"

// De twee brandende fakkels. Ze staan op de startpagina en op de game-over-
// pagina, links en rechts van de knop, en horen daar hetzelfde te branden: een
// vlam die op de ene pagina anders flakkert dan op de andere valt op zodra je
// wisselt, en dat is nu juist wat je niet ziet zolang je de twee pagina's
// ieder apart bekijkt. Vandaar deze ene plek in plaats van twee keer dezelfde
// regels.
//
// De knoppen waar ze naast staan zitten wel elk in hun eigen DisplayControl:
// die schelen in plaatje en in hoogte, dus daar valt niets te delen. Wat de
// fakkels van de knop moeten weten komt daarom als parameter binnen.
//
// Een klasse met alleen statics en geen exemplaren: de fakkelpanelen horen bij
// de DisplayControl die ze toont, en de staat die erbij hoort - welke cel elke
// vlam nu laat zien - staat daar dan ook. Wat hier staat is het gedeelde
// gedrag, niet een gedeeld object.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class TorchesDisplayHelpers
	{
	public:
		// De fakkels naast de knoppen. Ze staan met hun midden op dezelfde hoogte
		// als het midden van de knop, zodat ze meebewegen als die verschuift.
		static constexpr int TorchWidthPix  = 36;
		static constexpr int TorchHeightPix = 72;
		static constexpr int TorchMarginPix = 14;		// vanaf de schermrand

		// Hoe snel de vlam flakkert - en met opzet niet met een vaste stap.
		//
		// Een periodieke timer geeft precies wat je bij vuur niet wilt: een cyclus
		// die je na een paar rondes herkent. Door de wachttijd per stap te loten
		// blijft de gemiddelde snelheid gelijk, maar hoort er geen ritme meer in.
		// Vandaar een eenmalige timer die na elke tik opnieuw wordt gezet.
		//
		// De ondergrens is er omdat elke stap een volledige hertekening is: bij een
		// backbuffer kost die ~31 ms, en sneller aftikken dan dat levert alleen
		// maar werk op dat het scherm niet bijhoudt.
		static constexpr int TorchFrameIntervalMinMs = 45;
		static constexpr int TorchFrameIntervalMaxMs = 130;

		static uint64_t nextTorchIntervalUs()
		{
			int spanMs = TorchFrameIntervalMaxMs - TorchFrameIntervalMinMs + 1;
			return (uint64_t)(TorchFrameIntervalMinMs + (rand() % spanMs)) * 1000ULL;
		}

		// Twee fakkels, links en rechts, met hun midden op de hoogte van het midden
		// van de knop ernaast.
		static void initTorches(Panel<0>* arTorchPanels, int screenWidth,
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
		// Een dronkemanswandeling, en geen vrije loting. De tien cellen zijn
		// standen van dezelfde vlam over een cyclus, dus cel 0 en cel 5 schelen
		// flink in tonghoogte - vrij loten leest daardoor eerder als flikkeren dan
		// als branden. Een stap van hooguit twee houdt de vlam samenhangend,
		// terwijl de richting die elke keer kan omslaan het ritme er net zo goed
		// uit haalt.
		//
		// Een stap van nul komt niet voor: twee keer dezelfde cel leest als een
		// haperende vlam, en dat valt meer op dan welke sprong ook. Elke fakkel
		// wandelt apart, zodat ze niet als tweeling flakkeren - waar een vaste
		// volgorde daar nog een halve cyclus verschil voor nodig had.
		static void stepTorches(Panel<0>* arTorchPanels, int32_t* arIndexTorchFrame)
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

	private:
		// Er valt niets te maken: alles hierboven is static.
		TorchesDisplayHelpers() = delete;
	}; // end class TorchesDisplayHelpers

};// end namespace pong_cleangui_dg_adv
