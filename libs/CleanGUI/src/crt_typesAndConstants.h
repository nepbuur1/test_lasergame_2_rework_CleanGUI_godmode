// by Marius Versteegen, 2024

#pragma once
#include "crt_Vec2.h"

namespace crt
{
	enum class CoordType : int {Pixels=0,Promillage=1};
	// Raised: schaduw rondom, glans binnen langs boven en links - een opstaande
	// toets. Sunken: dezelfde schaduw rondom, maar de glans langs onder en
	// rechts, want daar valt het licht op de binnenwand van een uitsparing.
	enum class BevelStyle : int {Raised=0, Sunken=1};

	enum class Alignment : int {TopLeft=0, TopMid=1, TopRight=2, MidRight=3,     // Clockwise by 45 degrees per step.
		                        BottomRight=4, BottomMid=5, BottomLeft=6, MidLeft=7, MidMid=8};
};
