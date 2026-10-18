// by Marius Versteegen, 2026

#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <frt_IADC_buttons_E12.h>

namespace frt
{
	// Luistert naar een ADC_buttons_E12 en logt elke buttonPressed en buttonReleased.
	// FreeRTOS-variant van crt::AdcButtonTester (libs/CYD_tools_crt).
	//
	// buttonPressed() en buttonReleased() worden aangeroepen vanuit de taak van
	// ADC_buttons_E12. Ze doen daarom zo min mogelijk: ze zetten alleen een ButtonEvent
	// in queueButtonEvents. De eigen taak van AdcButtonTester wacht op die queue en logt.
	//
	// De taak wacht alleen op queueButtonEvents en op niets anders. Daarom is er geen
	// eventGroup en geen eventbit nodig: hij wacht en leest in één keer met een
	// blokkerende xQueueReceive.
	class AdcButtonTester : public IADC_buttons_E12Listener
	{
	private:
		static const int NOF_BUTTONS = 6;
		static const int BUTTON_EVENT_QUEUE_SIZE = 10;

		enum ButtonTransition { eButtonPressed, eButtonReleased };

		struct ButtonEvent
		{
			ButtonTransition buttonTransition;
			int              buttonIndex;
		};

		const char*  taskName;
		TaskHandle_t taskHandle;

		IADC_buttons_E12& adcButtons;
		QueueHandle_t queueButtonEvents;

	public:
		AdcButtonTester(const char* taskName, UBaseType_t taskPriority, uint32_t taskStackSizeBytes, BaseType_t taskCoreNumber,
		                IADC_buttons_E12& adcButtons) :
			taskName(taskName), taskHandle(nullptr), adcButtons(adcButtons)
		{
			queueButtonEvents = xQueueCreate(BUTTON_EVENT_QUEUE_SIZE, sizeof(ButtonEvent));
			configASSERT(queueButtonEvents != NULL);

			adcButtons.addButtonListener(this);
			xTaskCreatePinnedToCore(taskWrapper, taskName, taskStackSizeBytes, this, taskPriority, &taskHandle, taskCoreNumber);
		}

		// Aangeroepen vanuit de taak van ADC_buttons_E12.
		void buttonPressed(int buttonIndex) /*override*/
		{
			writeEvent(eButtonPressed, buttonIndex);
		}

		// Aangeroepen vanuit de taak van ADC_buttons_E12.
		void buttonReleased(int buttonIndex) /*override*/
		{
			writeEvent(eButtonReleased, buttonIndex);
		}

	private:
		static void taskWrapper(void* arg)
		{
			static_cast<AdcButtonTester*>(arg)->main();
		}

		void writeEvent(ButtonTransition buttonTransition, int buttonIndex)
		{
			ButtonEvent buttonEvent;
			buttonEvent.buttonTransition = buttonTransition;
			buttonEvent.buttonIndex      = buttonIndex;
			// Wachttijd 0, net als de CleanRTOS-Queue zonder bWriteWaitIfQueueFull: de taak van
			// ADC_buttons_E12 mag hier niet blijven hangen als deze queue vol is.
			if (xQueueSend(queueButtonEvents, &buttonEvent, 0) != pdPASS)
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

		void main()
		{
			ButtonEvent buttonEvent;
			while (true)
			{
				// Wacht tot er iets in queueButtonEvents staat en lees het in één keer.
				xQueueReceive(queueButtonEvents, &buttonEvent, portMAX_DELAY);
				logEvent(buttonEvent);
			}
		}
	}; // end class AdcButtonTester
};// end namespace frt
