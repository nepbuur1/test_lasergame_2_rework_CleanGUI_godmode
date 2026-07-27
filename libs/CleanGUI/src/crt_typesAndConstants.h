// by Marius Versteegen, 2024

#pragma once
#include <stdint.h>
#include "crt_Vec2.h"

namespace crt
{
	enum class CoordType : int {Pixels=0,Promillage=1};
	// Raised: schaduw rondom, glans binnen langs boven en links - een opstaande
	// toets. Sunken: dezelfde schaduw rondom, maar de glans langs onder en
	// rechts, want daar valt het licht op de binnenwand van een uitsparing.
	enum class BevelStyle : int {Raised=0, Sunken=1};

	// Elke kleur in CleanGUI is een uint32_t in ARGB, en de bovenste byte is
	// overal nul: alles staat als 0x00RRGGBB genoteerd. Die byte is dus vrij om
	// er iets anders in te herkennen - een index in het schermpalet.
	//
	// Daarmee kan dezelfde fillRect() zowel "maak dit vlak lichtblauw" als "maak
	// dit vlak kleur 7" betekenen, zonder de interface te verdubbelen. Wie een
	// palet heeft ontworpen wijst er precies in aan; wie dat niet heeft, geeft
	// gewoon een kleur op en de adapter zoekt de dichtstbijzijnde index erbij.
	//
	// Gebruik altijd deze helper en nooit de ruwe waarde: 0xFFFFFFFF is anders
	// zomaar index 255 in plaats van wit.
	constexpr uint32_t paletteIndex(uint8_t index)
	{
		return 0x01000000u | (uint32_t)index;
	}

	constexpr bool isPaletteIndex(uint32_t colour)
	{
		return (colour & 0xFF000000u) == 0x01000000u;
	}

	constexpr uint8_t toPaletteIndex(uint32_t colour)
	{
		return (uint8_t)(colour & 0x000000FFu);
	}

	// Overlappen twee as-parallelle rechthoeken elkaar? Randen die elkaar net
	// raken tellen niet mee: een rechthoek beslaat [topLeft, topLeft+size).
	inline bool rectsIntersect(const Vec2& topLeftA, const Vec2& sizeA,
	                           const Vec2& topLeftB, const Vec2& sizeB)
	{
		return (topLeftA.x < topLeftB.x + sizeB.x) && (topLeftB.x < topLeftA.x + sizeA.x) &&
		       (topLeftA.y < topLeftB.y + sizeB.y) && (topLeftB.y < topLeftA.y + sizeA.y);
	}

	enum class Alignment : int {TopLeft=0, TopMid=1, TopRight=2, MidRight=3,     // Clockwise by 45 degrees per step.
		                        BottomRight=4, BottomMid=5, BottomLeft=6, MidLeft=7, MidMid=8};
};
