// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

// Het speelveld zoals de bal het ervaart.
//
// UC05_BallHandling: raakt de bal een present crate, dan geeft hij dat door aan
// dat object (1c); raakt hij een vuurkorf, dan vliegt hij in brand (1d); kaatst
// hij op de peddel, dan gaat de score omhoog (1a). Alle drie gaan via deze ene
// interface, zodat de bal geen enkel ander spelobject bij naam kent, en die
// objecten geen ballen.
//
// Heette eerder IObstacleField, maar dat dekte de lading niet meer: een vuurkorf
// is geen obstakel (je vliegt er doorheen) en de peddel al helemaal niet.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// Wat de bal terugkrijgt als hij een crate heeft geraakt. De maat en plek
	// zitten erin omdat de bal ertegenaan moet kaatsen, en hij verder niets van
	// crates weet.
	struct CrateHit
	{
		CrateHit():bHit(false),posX(0),posY(0),sizePix(0){}
		CrateHit(int posX, int posY, int sizePix)
			:bHit(true),posX(posX),posY(posY),sizePix(sizePix){}

		bool bHit;
		int posX;
		int posY;
		int sizePix;
	};

	class IBallField
	{
	public:
		// Meldt een treffer aan de geraakte present crate en geeft terug waar die
		// stond. bHit is onwaar als er niets is geraakt.
		virtual CrateHit reportCrateHit(int ballPosX, int ballPosY, int ballRadius) = 0;

		// Staat de bal in een vuurkorf? Geen kaatsing en geen melding terug: een
		// korf is vuur en geen kist, dus de bal vliegt er doorheen en vat vlam.
		virtual bool touchesFireBin(int ballPosX, int ballPosY, int ballRadius) = 0;

		// De bal is op de peddel gekaatst (UC05 stap 1a). De score is van het spel
		// en niet van de bal, dus hij meldt het alleen.
		virtual void reportPaddleBounce() = 0;
	};
};// end namespace pong_cleangui_dg_adv
