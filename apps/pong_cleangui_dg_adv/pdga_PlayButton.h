// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanGUI.h>
#include <crt_TouchscreenButton.h>

#include "images/_pal8/_allImages.h"

// De Play-knop. Hij staat op de startpagina en op de game-over-pagina, en die
// twee horen precies dezelfde knop te tonen: hij verspringt anders bij het
// wisselen van pagina, en dat is nu juist wat je niet ziet zolang je de twee
// pagina's ieder apart bekijkt. Vandaar deze ene plek in plaats van twee keer
// dezelfde regels.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	typedef TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> PlayButton;

	// Waar de knop staat. De y is die van de brede "Click here to Start" van
	// voorheen (562 promille van 240 pixels); alleen de breedte is veranderd -
	// van 800 promille naar de 120 pixels van het plaatje.
	const int PlayButtonTopPix = 135;

	inline void initPlayButton(PlayButton& button, int screenWidth)
	{
		// Alles in pixels en niet in promillage. Bij een knop die uit een plaatje
		// bestaat is dat geen smaak: promillage wordt naar hele pixels afgerond,
		// en een panel dat daardoor een pixel groter uitvalt dan het plaatje laat
		// Panel::needsBackground() de vlakke colPanel-kleur er alsnog onder
		// schilderen. Die rand van een pixel is precies zichtbaar genoeg om lelijk
		// te zijn.
		Vec2 sizePix(imageButtonPlay.width, imageButtonPlay.height);

		// Het label is leeg omdat "PLAY" in het plaatje zit - met de schuine kant
		// en het verloop eromheen, die geen enkele fillRect kan maken. colPanel en
		// colFont blijven wel ingevuld: het plaatje is transparant rond zijn
		// afgeronde hoeken, dus zodra iemand de setImage() hieronder weghaalt is
		// er weer een gewone rode knop met tekst.
		button.setTouchScreenButtonProps
			("play",/*locPos*/Vec2((screenWidth - sizePix.x)/2, PlayButtonTopPix), CoordType::Pixels,
			/*size*/sizePix, /*cornerRadius*/0, CoordType::Pixels,
			Alignment::MidMid, /*colPanel*/0x00FF0000, /*colBg*/0x00000000,
			/*bInvertedArea*/false, "", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF,
			/*fontScale*/ 1, Alignment::MidMid,
			/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);

		// fontScale 1 en niet 0: bij 0 zoekt TouchscreenButton de grootste schaal
		// die past, en dat is werk zonder betekenis voor een leeg label.
		button.setImage(&imageButtonPlay);
	}
};// end namespace pong_cleangui_dg_adv
