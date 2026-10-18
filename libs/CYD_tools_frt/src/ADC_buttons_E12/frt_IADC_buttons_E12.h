// by Marius Versteegen, 2026

#pragma once
#include <stdint.h>
#include "frt_IADC_buttons_E12Listener.h"

namespace frt
{
	// Interface van frt::ADC_buttons_E12, zodat gebruikers niet van de template-parameter
	// (MAXLISTENERCOUNT) hoeven af te hangen.
	class IADC_buttons_E12
	{
	public:
		virtual void addButtonListener(IADC_buttons_E12Listener* pListener) = 0;

		// Thread-safe. buttonIndex 0..5 (knop 0 = S1).
		virtual bool isButtonPressed(int buttonIndex) = 0;

		// Thread-safe. Bit i = knop i ingedrukt.
		virtual uint8_t getPressedMask() = 0;

		// Thread-safe. De laatst gemeten spanning op de ADC-pin (gemiddeld, gekalibreerd).
		virtual uint16_t getMillivolts() = 0;
	};
};
