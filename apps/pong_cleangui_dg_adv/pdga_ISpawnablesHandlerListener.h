// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

// Wat de beeldklok aan het spel meldt, en het enige dat hij eraan vraagt.
//
// Los van IBallListener, want de gebruikers zijn anders: een bal meldt zijn eigen
// kaatsing (zie pdga_IBallListener.h), terwijl deze twee over het veld als geheel
// gaan en dus alleen bekend zijn bij wie het per beeld afloopt.
//
// GameControl implementeert allebei.
//
// Waarom een interface en niet gewoon een GameControl& in de handler: dan zou
// pdga_SpawnablesHandler.h pdga_GameControl.h moeten includen, terwijl die
// andersom al de handler includet - en in een header-only app is er geen .cpp
// waar je die knoop mee doorhakt. Bovendien bestaat de handler eerder dan zijn
// luisteraar: GameControl krijgt hem als constructorargument mee, dus de
// bedrading kan alleen achteraf via setListener().
//
// De tweede reden weegt zwaarder dan de eerste. SpawnablesHandler::update()
// draait in de Handler-taak en GameControl woont in die van MainControl. Deze
// twee methodes zijn precies wat er over die grens heen mag - zie de
// toelichting bij score in pdga_GameControl.h. Met een volle GameControl& in
// handen zou doGame() er net zo goed bij staan.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class ISpawnablesHandlerListener
	{
	public:
		// UC04 stap 4: het spel loopt tot er geen ballen meer in het spel zijn. Wie
		// de depots per beeld afloopt is de enige die de overgang ziet, dus die
		// meldt het.
		virtual void allBallsGone() = 0;

		// De score gaat in hetzelfde beeldbericht mee als de standen van de
		// spelobjecten; twee losse berichten zouden twee hertekeningen kunnen
		// opleveren. Wordt dus elk beeld gevraagd, vanuit de Handler-taak.
		virtual int32_t getScore() = 0;
	};
};// end namespace pong_cleangui_dg_adv
