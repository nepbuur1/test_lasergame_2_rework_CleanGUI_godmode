// by Marius Versteegen, 2026

#pragma once
#include <stdint.h>
#include "crt_IADC_buttonsListener.h"

namespace crt
{
	// Interface van crt::ADC_buttons, zodat gebruikers niet van de template-parameter
	// (MAXLISTENERCOUNT) hoeven af te hangen.
	class IADC_buttons
	{
	public:
		virtual void addButtonListener(IADC_buttonsListener* pListener) = 0;

		// Thread-safe. buttonIndex 0..5 (knop 0 = S1).
		virtual bool isButtonPressed(int buttonIndex) = 0;

		// Thread-safe. Bit i = knop i ingedrukt.
		virtual uint8_t getPressedMask() = 0;

		// Thread-safe. De laatst gemeten spanning op de ADC-pin (gemiddeld, gekalibreerd).
		virtual uint16_t getMillivolts() = 0;

		// Thread-safe. Of de weerstandsladder bij het opstarten aangesloten was: toen lag de
		// gemeten spanning rond het verwachte niveau voor "geen knop ingedrukt".
		// Geldig zodra de controle bij het opstarten klaar is (±100 ms na de start); daarvoor false.
		virtual bool isConnected() = 0;
	};
};
