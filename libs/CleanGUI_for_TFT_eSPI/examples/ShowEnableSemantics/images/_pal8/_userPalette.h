// Met de hand geschreven - dit bestand wordt NIET gegenereerd.
//
// De kleuren die hier staan komen letterlijk, en in deze volgorde, vooraan in
// het gegenereerde _palette.h terecht. arUserPalette[i] is dus palet-index i,
// en dat blijft zo als er plaatjes bijkomen of verdwijnen: de converter vult
// alleen wat erachter komt. Daarmee is paletteIndex(i) bruikbaar in de code.
//
// Kleuren staan in ARGB (0x00RRGGBB), net als overal elders in CleanGUI, zodat
// je ze rechtstreeks uit een widget kunt overnemen. De converter rekent ze om
// naar RGB565.
//
// Wat hier *niet* in hoeft: een kleurenkubus of een grijstrap om de rest mee te
// vullen. De converter doet dat zelf met elke ingang die overblijft.
//
// Er passen er hoogstens 255; index 255 is de transparante.

#pragma once
#include <stdint.h>

namespace crt
{
	static constexpr uint32_t arUserPalette[] =
	{
		// basis
		0x00000000,		// 0
		0x00FFFFFF,		// 1

		// bevelgrijzen uit de widgets (crt_Button, crt_Inputfield, ...)
		0x000C0C0C,		// 2
		0x00303030,		// 3
		0x00313131,		// 4
		0x003A3A3A,		// 5
		0x00505050,		// 6
		0x00B8B8B8,		// 7

		// grijzen uit de voorbeelden zelf
		0x00222222,		// 8
		0x00666666,		// 9
		0x00888888,		// 10

		// primairen en de mengkleuren die de voorbeelden gebruiken
		0x00FF0000,		// 11
		0x0000FF00,		// 12
		0x000000FF,		// 13
		0x0000FFFF,		// 14
		0x00008800,		// 15
		0x00808000,		// 16
		0x00006020,		// 17
		0x00000080,		// 18
	};
}
