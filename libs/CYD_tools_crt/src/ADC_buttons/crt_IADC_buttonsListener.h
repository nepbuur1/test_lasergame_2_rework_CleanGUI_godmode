// by Marius Versteegen, 2026

#pragma once

namespace crt
{
	// Listener van crt::ADC_buttons.
	// buttonIndex loopt van 0 t/m 5: knop 0 = S1, ..., knop 5 = S6 in het schema.
	//
	// Let op: beide functies worden aangeroepen vanuit de taak van ADC_buttons.
	// Houd ze kort (zet bijv. een Flag of schrijf in een Queue) en blokkeer er niet in;
	// zolang een listener bezig is, wordt de ADC niet uitgelezen.
	class IADC_buttonsListener
	{
	public:
		virtual void buttonPressed(int buttonIndex) = 0;
		virtual void buttonReleased(int buttonIndex) = 0;
	};
};
