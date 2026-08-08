// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

// Wat een bal aan het spel te melden heeft.
//
// Eén functie, en dat is het punt: de bal meldt rechtstreeks aan wie er iets mee
// doet, in plaats van via het veld waarin hij ligt. Spawnables gaf dit eerst door, en
// dat was doorgeefwerk zonder eigen betekenis - opslag hoort geen boodschappen te
// bezorgen.
//
// GameControl implementeert dit; daar woont de score.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class IBallListener
	{
	public:
		// UC05 stap 1a: de score gaat omhoog bij een kaatsing op de peddel. Alleen
		// de peddel - de wanden en de crates zijn geen verdienste van de speler.
		virtual void ballBouncedOnPaddle() = 0;
	};
};// end namespace pong_cleangui_dg_adv
