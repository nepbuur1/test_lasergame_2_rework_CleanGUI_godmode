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
