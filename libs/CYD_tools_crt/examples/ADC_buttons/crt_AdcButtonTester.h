// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_IADC_buttons.h>

namespace crt
{
	// Luistert naar een ADC_buttons en logt elke buttonPressed en buttonReleased.
	//
	// buttonPressed() en buttonReleased() worden aangeroepen vanuit de taak van
	// ADC_buttons. Ze doen daarom zo min mogelijk: ze zetten alleen een ButtonEvent
	// in een Queue. De eigen taak van AdcButtonTester wacht op die Queue en logt.
	class AdcButtonTester : public Task, public IADC_buttonsListener
	{
	private:
		static const int NOF_BUTTONS = 6;

		enum ButtonTransition { eButtonPressed, eButtonReleased };

		struct ButtonEvent
		{
			ButtonTransition buttonTransition;
			int              buttonIndex;
		};

		IADC_buttons& adcButtons;
		Queue<ButtonEvent, 10> queueButtonEvents;

	public:
		AdcButtonTester(const char* taskName, unsigned int taskPriority, unsigned int taskStackSizeBytes, unsigned int taskCoreNumber,
		                IADC_buttons& adcButtons) :
			Task(taskName, taskPriority, taskStackSizeBytes, taskCoreNumber),
			adcButtons(adcButtons), queueButtonEvents(this)
		{
			adcButtons.addButtonListener(this);
			start();
		}

		// Aangeroepen vanuit de taak van ADC_buttons.
		void buttonPressed(int buttonIndex) /*override*/
		{
			writeEvent(eButtonPressed, buttonIndex);
		}

		// Aangeroepen vanuit de taak van ADC_buttons.
		void buttonReleased(int buttonIndex) /*override*/
		{
			writeEvent(eButtonReleased, buttonIndex);
		}

	private:
		void writeEvent(ButtonTransition buttonTransition, int buttonIndex)
		{
			ButtonEvent buttonEvent;
			buttonEvent.buttonTransition = buttonTransition;
			buttonEvent.buttonIndex      = buttonIndex;
			if (!queueButtonEvents.write(buttonEvent))
			{
				ESP_LOGW(taskName, "queueButtonEvents was vol; event van S%d gaat verloren.", buttonIndex + 1);
			}
		}

		void logEvent(const ButtonEvent& buttonEvent)
		{
			// Let op: de ingedrukte knoppen en de spanning zijn die van nú, bij het loggen.
			// Staan er meerdere events in de queue, dan kan dat al een latere toestand zijn
			// dan die van dit event.
			char pressed[NOF_BUTTONS + 1];
			for (int i = 0; i < NOF_BUTTONS; i++)
			{
				pressed[i] = adcButtons.isButtonPressed(i) ? ('1' + i) : '-';
			}
			pressed[NOF_BUTTONS] = '\0';

			ESP_LOGI(taskName, "S%d %s   ingedrukt: [%s]   %4u mV",
			         buttonEvent.buttonIndex + 1,
			         (buttonEvent.buttonTransition == eButtonPressed) ? "pressed " : "released",
			         pressed, (unsigned)adcButtons.getMillivolts());
		}

		/*override keyword not supported*/
		void main()
		{
			// Wacht tot ADC_buttons de controle bij het opstarten gedaan heeft (±100 ms).
			vTaskDelay(pdMS_TO_TICKS(300));
			if (adcButtons.isConnected())
			{
				ESP_LOGI(taskName, "Weerstandsladder aangesloten.");
			}
			else
			{
				ESP_LOGW(taskName, "Weerstandsladder NIET aangesloten (of er werd bij het opstarten een knop vastgehouden).");
			}

			ButtonEvent buttonEvent;
			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); // This function call takes about 0.25ms! It should be called while debugging only.

				wait(queueButtonEvents);
				// Eén event per ronde. Staat er nog meer in de queue, dan blijft het
				// event-bit aan en komt wait() meteen terug.
				if (queueButtonEvents.tryRead(buttonEvent))
				{
					logEvent(buttonEvent);
				}
			}
		}
	}; // end class AdcButtonTester
};// end namespace crt
