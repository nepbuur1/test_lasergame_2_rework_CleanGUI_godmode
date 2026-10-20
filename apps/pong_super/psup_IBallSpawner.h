// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

// Ballen in het veld zetten, voor wie er zelf geen bij kan.
//
// Een geopende present crate levert NofBallsPerCrate ballen op de plek waar hij
// stond (UC07 stap 3). Dat is de crate zijn eigen zaak - hij weet wanneer hij
// opengaat en waar hij staat - maar de ballen zijn dat niet: die liggen in een depot
// waar een crate niets van hoort te weten.
//
// Vandaar deze ene functie. Spawnables implementeert hem, want dat bezit het
// baldepot.

using namespace crt;
namespace pong_super
{
	class IBallSpawner
	{
	public:
		// nofBalls ballen op (posX, posY) in het veld zetten. Zijn er niet genoeg
		// vrij, dan komen er minder - een vol depot is geen fout maar een uitkomst,
		// en het depot meldt het zelf.
		virtual void spawnBallsAt(int posX, int posY, int nofBalls) = 0;
	};
};// end namespace pong_super
