// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

// Wat de naampagina te melden heeft aan degene die haar heeft laten tonen.
//
// Eén functie, en die is er om een reden die de andere pagina's niet hebben: bij
// het verbergen schuift eerst het toetsenbord uit beeld, en dat is nog een paar
// honderd milliseconden tekenwerk nadat hideNameInputPage() is teruggekeerd. Wie
// ondertussen de volgende pagina laat tekenen, staat er dwars doorheen - en dat
// is precies wat de regel "één PageRoot tegelijk in beeld" verbiedt (zie de kop
// van crt_PageRoot.h).
//
// NameInputControl implementeert dit; daar wordt op de melding gewacht.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class INameInputPageListener
	{
	public:
		// De pagina is uit beeld: het toetsenbord ligt weer onder de schermrand en
		// er wordt niets meer getekend. Pas hierna mag er een andere pagina
		// beginnen.
		virtual void nameInputPageHidden() = 0;
	};
};// end namespace pong_cleangui_dg_adv
