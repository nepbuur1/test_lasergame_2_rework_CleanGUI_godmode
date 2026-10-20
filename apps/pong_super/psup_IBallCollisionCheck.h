// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

// Waar de bal tegenaan zit, en wat dat betekent.
//
// UC05_BallHandling: raakt de bal een present crate, dan geeft hij dat door aan
// dat object (1c); raakt hij een vuurkorf, dan vliegt hij in brand (1d); kaatst
// hij op de peddel, dan gaat de score omhoog (1a). Alle drie gaan via deze ene
// interface, zodat de bal geen enkel ander spelobject bij naam kent, en die
// objecten geen ballen.
//
// Heette eerder IObstacleField en daarna IBallField. Het eerste dekte de lading
// niet: een vuurkorf is geen obstakel (je vliegt er doorheen). Het tweede zei
// alleen dat er een veld was, en niet wat de bal eraan heeft.
//
// getPaddlePosition() valt buiten de naam - dat is een vraag en geen botsing.
// Hij staat hier omdat de bal hem in dezelfde ademtocht stelt, en om de reden
// die er verderop bij staat: elke bal die het rechtstreeks aan PaddleControl
// vraagt kost een slotoperatie per beeld.

using namespace crt;
namespace pong_super
{
	// Wat de bal terugkrijgt als hij een crate heeft geraakt. De maat en plek
	// zitten erin omdat de bal ertegenaan moet kaatsen, en hij verder niets van
	// crates weet.
	struct CrateHit
	{
		CrateHit():bHit(false),indexSlot(-1),posX(0),posY(0),sizePix(0){}
		CrateHit(int indexSlot, int posX, int posY, int sizePix)
			:bHit(true),indexSlot(indexSlot),posX(posX),posY(posY),sizePix(sizePix){}

		bool bHit;
		int indexSlot;		// welke crate het was, zodat de mededeling niet opnieuw zoekt
		int posX;
		int posY;
		int sizePix;
	};

	// De naamgeving volgt de conventie van deze app: een booleaanse vraag begint met
	// "is", een vraag die instantaan iets oplevert met "get", en een mededeling
	// klinkt als een mededeling. Vragen veranderen niets - wie iets wil veranderen
	// zegt het.
	//
	// Wat de bal aan het SPEL meldt zit hier niet in, maar in IBallListener: dat gaat
	// niet over zijn omgeving maar over de score, en daar hoort het veld niet tussen.
	class IBallCollisionCheck
	{
	public:
		// Raakt de bal op deze plek een present crate, en zo ja: welke, waar, hoe
		// groot? bHit is onwaar als er niets is geraakt.
		//
		// Verandert niets aan die crate. Dat de bal hem heeft geraakt is een aparte
		// mededeling, hieronder - anders zou je niet kunnen kijken zonder te slopen.
		virtual CrateHit getCrateHitAtPos(int ballPosX, int ballPosY, int ballRadius) = 0;

		// De bal heeft de crate uit getCrateHitAtPos() geraakt (UC05 stap 1c). Wat
		// dat betekent - de teller omlaag, en bij nul de crate open - is de zaak van
		// het veld, niet van de bal.
		virtual void crateWasHitAtPos(const CrateHit& hit) = 0;

		// Staat de bal in een vuurkorf? Geen kaatsing en geen mededeling achteraf:
		// een korf is vuur en geen kist, dus de bal vliegt er doorheen en vat vlam.
		// De korf ondergaat niets - vandaar dat hier een vraag genoeg is.
		virtual bool isFireBinTouchedAtPos(int ballPosX, int ballPosY, int ballRadius) = 0;

		// Waar de peddel staat in het beeld dat nu berekend wordt.
		//
		// Via het veld en niet rechtstreeks bij PaddleControl, en dat is geen
		// stijlkeuze: PaddleControl::getPaddlePosition() gaat via een Pool, en die neemt
		// een SimpleMutex. Met vijftig ballen waren dat vijftig slotoperaties per
		// beeld op een slot dat ook door PaddleControl zelf wordt genomen - een eigen
		// taak, dus daar viel echt op te wachten. Het veld leest de stand nu een keer
		// per beeld, in dezelfde taak waarin de ballen daarna hun update() doen, en
		// er valt niets meer te vergrendelen.
		//
		// Dit is de enige plek waar de beeldstap nog een taakgrens oversteekt. Al het
		// andere - de depots, de standen, de score - zit sinds de beeldklok bij
		// GameControl woont in een taak; zie psup_GameControl.h.
		//
		// Bijvangst, en niet de minste: alle ballen van een beeld zien gegarandeerd
		// dezelfde peddel.
		virtual int getPaddlePosition() = 0;
	};
};// end namespace pong_super
