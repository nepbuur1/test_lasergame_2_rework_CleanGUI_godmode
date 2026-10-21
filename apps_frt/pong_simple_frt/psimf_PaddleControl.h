// by Marius Versteegen, 2023

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include "psimf_DisplayControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_simple_frt
{
	// Zie psimf_BallControl.h: logger komt uit namespace crt, niet uit deze.

	class PaddleControl : public IButtonListener
	{

		// --- FreeRTOS (was CleanRTOS) ---
		static constexpr EventBits_t eventBitEnable = (1 << 0);
		static constexpr EventBits_t eventBitDisable = (1 << 1);
		static constexpr EventBits_t eventBitLeftPressed = (1 << 2);
		static constexpr EventBits_t eventBitRightPressed = (1 << 3);
		static constexpr EventBits_t eventBitLeftReleased = (1 << 4);
		static constexpr EventBits_t eventBitRightReleased = (1 << 5);
		static constexpr EventBits_t eventBitRepeatTimer = (1 << 6);
		EventGroupHandle_t eventGroup;
		EventBits_t firedBits;		// wat de laatste wachtoproep op meer tegelijk opleverde; zie hasFired()
		TaskHandle_t taskHandle;

		enum State
		{
			STATE_DISABLED,
			STATE_ENABLED,		// wacht op een knop die ingedrukt wordt
			STATE_MOVING		// een knop wordt vastgehouden: blijf schuiven
		};

		// De peddel schuift op dezelfde beeldklok als de bal, zodat hij net zo
		// vloeiend beweegt en niet in horten en stoten.
		static constexpr uint64_t repeatPeriodUs = FrameIntervalMs * 1000;

	private:
		State state;
		DisplayControl& displayControl;
		QueueHandle_t paddlePosQueue;		// lengte 1: was Pool<int> poolPaddlePos
		TimerHandle_t repeatTimer;			// tikt door zolang een knop wordt vastgehouden.
		int screenWidth;
		int paddlePosition;
		int paddleWidth;
		int paddleDirection;		// -1 naar links, +1 naar rechts, 0 = stil
		IButton* pButtonLeft;
		IButton* pButtonRight;

	public:
		PaddleControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, DisplayControl& displayControl) :	
			eventGroup(xEventGroupCreate()),
			firedBits(0),
			taskHandle(nullptr), 
			state(STATE_DISABLED),
			displayControl(displayControl),
			repeatTimer(xTimerCreate("repeatTimer", 1, pdTRUE, this, repeatTimerCallback)),
			screenWidth(displayControl.getWidth()),
			paddlePosition(screenWidth/2),
			paddleWidth(PaddleWidthPix),
			paddleDirection(0),
			pButtonLeft(nullptr),
			pButtonRight(nullptr)
		{
			paddlePosQueue = xQueueCreate(1, sizeof(int));
			{ int initValue{}; xQueueOverwrite(paddlePosQueue, &initValue); }
			{ int poolValue = paddlePosition; xQueueOverwrite(paddlePosQueue, &poolValue); }
			pButtonLeft = displayControl.addLeftButtonListener(this);
			pButtonRight = displayControl.addRightButtonListener(this);
			xTaskCreatePinnedToCore(staticMain, taskName, taskSizeBytes, this, taskPriority, &taskHandle, taskCoreNumber);
		}

	int getPaddlePos()
	{
		int tempPaddlePos;
		xQueuePeek(paddlePosQueue, &tempPaddlePos, 0);
		return tempPaddlePos;
	}

    void buttonPressed(IButton* pButton)
    {
		ESP_LOGI("pong_simple_frt", "%s", pButton->getButtonName());

		if(pButton == pButtonLeft)
		{
        	xEventGroupSetBits(eventGroup, eventBitLeftPressed);
		}
		else if(pButton == pButtonRight)
		{
			xEventGroupSetBits(eventGroup, eventBitRightPressed);
		}
		else
		{
			// not interested.
		}
    }

	// De knop loslaten is hier net zo goed een gebeurtenis als hem indrukken:
	// daartussen schuift de peddel door.
	void buttonReleased(IButton* pButton)
	{
		if(pButton == pButtonLeft)
		{
			xEventGroupSetBits(eventGroup, eventBitLeftReleased);
		}
		else if(pButton == pButtonRight)
		{
			xEventGroupSetBits(eventGroup, eventBitRightReleased);
		}
		else
		{
			// not interested.
		}
	};

	void enable()
	{
		xEventGroupSetBits(eventGroup, eventBitEnable);
	}

	void disable()
	{
		xEventGroupSetBits(eventGroup, eventBitDisable);
	}

	private:
		// Een stap in de gevraagde richting, tegen de randen aan begrensd.
		int calcNewPaddlePosition(int paddlePosition, int direction) const
		{
			int minPosition = WallThicknessPix + paddleWidth/2;
			int maxPosition = screenWidth - WallThicknessPix - paddleWidth/2;

			paddlePosition += direction * PaddleStepPix;

			if (paddlePosition < minPosition) { paddlePosition = minPosition; }
			if (paddlePosition > maxPosition) { paddlePosition = maxPosition; }

			return paddlePosition;
		}

		// Maak een stap verder, en maak meteen de nieuwe positie beschikbaar voor andere taken (via getPaddlePos).
		void movePaddle(int direction)
		{
			paddlePosition = calcNewPaddlePosition(paddlePosition, direction);
			displayControl.updatePaddlePosition(paddlePosition);

			// De bal krijgt de positie niet meer toegeschoven, hij haalt hem zelf
			// op met getPaddlePos(). Deze pool is dus de enige koppeling die
			// daarvoor nodig is.
			xQueueOverwrite(paddlePosQueue, &paddlePosition);		// facilitatie getPaddlePos()
		}

		void stopMoving()
		{
			xTimerStop(repeatTimer, 0);
			paddleDirection = 0;
		}


		// --- FreeRTOS-hulpfuncties ---
		static void staticMain(void* pvParameters)
		{
			static_cast<PaddleControl*>(pvParameters)->main();
		}

		static void repeatTimerCallback(TimerHandle_t xTimer)
		{
			PaddleControl* instance = static_cast<PaddleControl*>(pvTimerGetTimerID(xTimer));
			xEventGroupSetBits(instance->eventGroup, eventBitRepeatTimer);
		}

		// CleanRTOS-timers rekenen in microseconden, FreeRTOS-timers in ticks (minstens 1).
		static TickType_t usToTicks(uint64_t us)
		{
			TickType_t ticks = pdMS_TO_TICKS(us / 1000);
			return (ticks > 0) ? ticks : 1;
		}

		// Zoals CleanRTOS' hasFired(): kijkt of het bit bij de laatste wachtoproep
		// gevuurd had, en wist alleen dat bit.
		bool hasFired(EventBits_t eventBit)
		{
			bool bFired = ((firedBits & eventBit) != 0);
			if (bFired)
			{
				xEventGroupClearBits(eventGroup, eventBit);
			}
			return bFired;
		}


		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

			while (true)
			{
				vTaskDelay(1);

				switch(state)
				{
					case State::STATE_DISABLED:
						firedBits = xEventGroupWaitBits(eventGroup, eventBitEnable, pdTRUE, pdFALSE, portMAX_DELAY);
						xEventGroupClearBits(eventGroup, eventBitDisable);		// een disable van voor deze enable telt niet meer mee.
						// Drukken die tijdens het uitgeschakeld zijn zijn blijven
						// staan, mogen de peddel niet meteen op weg sturen.
						xEventGroupClearBits(eventGroup, eventBitLeftPressed);
						xEventGroupClearBits(eventGroup, eventBitRightPressed);
						xEventGroupClearBits(eventGroup, eventBitLeftReleased);
						xEventGroupClearBits(eventGroup, eventBitRightReleased);
						// Tekenen doet DisplayControl - dat is de hele reden dat de
						// peddelknoppen daar wonen en niet hier.
						displayControl.showPaddle();
						state = State::STATE_ENABLED;
					break;

					case State::STATE_ENABLED:
						firedBits = xEventGroupWaitBits(eventGroup, eventBitLeftPressed | eventBitRightPressed | eventBitDisable, pdFALSE, pdFALSE, portMAX_DELAY);
						if (hasFired(eventBitDisable))
						{
							displayControl.hidePaddle();
							state = State::STATE_DISABLED;
						}
						else if(hasFired(eventBitLeftPressed))
						{
							paddleDirection = -1;
						}
						else if(hasFired(eventBitRightPressed))
						{
							paddleDirection = +1;
						}
						else
						{
							assert(false);
						}

						if (paddleDirection != 0)
						{
							// Meteen een stap, zodat een korte tik ook aankomt, en
							// daarna doortikken zolang de vinger blijft liggen.
							movePaddle(paddleDirection);
							xEventGroupClearBits(eventGroup, eventBitLeftReleased);	// een loslaten van voor deze druk telt niet mee.
							xEventGroupClearBits(eventGroup, eventBitRightReleased);
							xEventGroupClearBits(eventGroup, eventBitRepeatTimer);
			xTimerChangePeriod(repeatTimer, usToTicks(repeatPeriodUs), portMAX_DELAY);
							state = State::STATE_MOVING;
						}
					break;

					case State::STATE_MOVING:
						firedBits = xEventGroupWaitBits(eventGroup, eventBitLeftReleased | eventBitRightReleased | eventBitDisable | eventBitRepeatTimer, pdFALSE, pdFALSE, portMAX_DELAY);
						if (hasFired(eventBitDisable))
						{
							stopMoving();
							displayControl.hidePaddle();
							state = State::STATE_DISABLED;
						}
						else if (hasFired(eventBitLeftReleased) || hasFired(eventBitRightReleased))
						{
							stopMoving();
							state = State::STATE_ENABLED;
						}
						else if (hasFired(eventBitRepeatTimer))
						{
							movePaddle(paddleDirection);
						}
						else
						{
							assert(false);
						}
					break;
					
					default:
						assert(false);
					break;
				}
			}
		}
	}; // end class PaddleControl

};// end namespace crt
