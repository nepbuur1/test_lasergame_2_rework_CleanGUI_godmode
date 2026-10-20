// Met de hand geschreven - dit bestand wordt NIET gegenereerd.
//
// De kleuren hier komen letterlijk, en in deze volgorde, vooraan in het
// gegenereerde _palette.h terecht. Zie de toelichting boven het gelijknamige
// bestand in libs/CleanGUI_for_TFT_eSPI/examples/Panel/images/_pal8.
//
// Waarom deze app er een heeft: bijna alles op het scherm komt uit een plaatje,
// maar niet alles. De achtergrondpanelen, de twee peddelknoppen en hun tekst
// worden nog gewoon getekend, met de kleur die in de C++-code staat. Zonder dit
// bestand zoekt TFT_eSPI_DisplayAdapter_PAL8::indexForColour() daar de
// dichtstbijzijnde palet-ingang bij, en dat is wat de median-cut over de
// plaatjes toevallig heeft opgeleverd - een rood dat net niet het rood is dat je
// opgaf. Wat hier staat zit er gegarandeerd exact in, en de zoektocht vindt hem
// dan op afstand 0.

#pragma once
#include <stdint.h>

namespace crt
{
	static constexpr uint32_t arUserPalette[] =
	{
		// De kleuren die de code zelf nog noemt.
		0x00000000,		// 0  achtergrond van alle drie de pagina's
		0x00FFFFFF,		// 1  tekst op de peddelknoppen
		0x00FF0000,		// 2  de peddelknoppen
		0x0000FFFF,		// 3  cyaan - wanden en peddel, ook waar de code ze noemt
		0x00FFFF00,		// 4  geel - de bal
		0x00404040,		// 5  grijs - de doellijn
		0x00202020,		// 6  het kader achter GAME OVER
		0x00FF4040,		// 7  de tekst GAME OVER

		// De basiskleuren uit de plaatjes. Ze zouden ook uit de median-cut
		// komen, maar dan op een index die verschuift zodra er een plaatje
		// bijkomt - en juist deze wil je kunnen herkennen in _palette.h.
		0x0000B4C8,		// 8  cyaan, middentoon
		0x00005070,		// 9  cyaan, schaduw
		0x00FFF040,		// 10 bal
		0x00FFFFC0,		// 11 bal, hoogsel
		0x00A07000,		// 12 bal, schaduw
		0x00E02020,		// 13 knop
		0x00FF8070,		// 14 knop, glans
		0x00700000,		// 15 knop, schaduw
		0x00181820,		// 16 kader achter GAME OVER
	};
}
