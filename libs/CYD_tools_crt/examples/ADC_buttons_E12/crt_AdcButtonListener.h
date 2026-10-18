// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_IADC_buttons_E12.h>

namespace crt
{
	// Luistert naar een ADC_buttons_E12 en logt elke buttonPressed en buttonReleased.
	// Draait niet in een eigen taak: de aanroepen komen uit de taak van ADC_buttons_E12.
	class AdcButtonListener : public IADC_buttons_E12Listener
	{
	private:
		static const int NOF_BUTTONS = 6;
		IADC_buttons_E12& adcButtons;

	public:
		AdcButtonListener(IADC_buttons_E12& adcButtons) : adcButtons(adcButtons)
		{
			adcButtons.addButtonListener(this);
		}

		void buttonPressed(int buttonIndex)
		{
			logEvent("pressed ", buttonIndex);
		}

		void buttonReleased(int buttonIndex)
		{
			logEvent("released", buttonIndex);
		}

	private:
		void logEvent(const char* what, int buttonIndex)
		{
			// isButtonPressed() geeft hier al de nieuwe toestand: de Pool is bijgewerkt
			// voordat de listeners worden aangeroepen.
			char pressed[NOF_BUTTONS + 1];
			for (int i = 0; i < NOF_BUTTONS; i++)
			{
				pressed[i] = adcButtons.isButtonPressed(i) ? ('1' + i) : '-';
			}
			pressed[NOF_BUTTONS] = '\0';

			ESP_LOGI("AdcButtonListener", "S%d %s   ingedrukt: [%s]   %4u mV",
			         buttonIndex + 1, what, pressed, (unsigned)adcButtons.getMillivolts());
		}
	}; // end class AdcButtonListener
};// end namespace crt
