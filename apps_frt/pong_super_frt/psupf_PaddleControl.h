// by Marius Versteegen, 2023
// FreeRTOS-variant (pong_super_frt): vanilla FreeRTOS in plaats van CleanRTOS. Zie
// models/pong_super_frt.drawio, tab STD-PaddleControl.

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <frt_IADC_buttons.h>
#include "psupf_GameDisplayControl.h"

using namespace crt;	// alleen voor de CleanGUI-typen (IButton, IButtonListener)
namespace pong_super_frt
{
	// De peddel luistert naar twee soorten knoppen tegelijk: de twee touchscreenknoppen
	// (IButtonListener, via GameDisplayControl) en twee knoppen van de weerstandsladder
	// (IADC_buttonsListener, via ADC_buttons). Beide zetten dezelfde eventbits; de
	// toestandsmachine hieronder merkt het verschil niet.
	class PaddleControl : public IButtonListener, public frt::IADC_buttonsListener
	{
		enum State
		{
			STATE_DISABLED,
			STATE_ENABLED,		// wacht op een knop die ingedrukt wordt
			STATE_MOVING		// een knop wordt vastgehouden: blijf schuiven
		};

		// Welke knoppen van de weerstandsladder de peddel besturen (buttonIndex 0 = S1).
		static const int AdcButtonLeft  = 0;	// S1
		static const int AdcButtonRight = 1;	// S2

		// De eventbits van eventGroup; in CleanRTOS waren dit de flags.
		static constexpr EventBits_t eventBitEnable        = (1 << 0);
		static constexpr EventBits_t eventBitDisable       = (1 << 1);
		static constexpr EventBits_t eventBitLeftPressed   = (1 << 2);
		static constexpr EventBits_t eventBitRightPressed  = (1 << 3);
		static constexpr EventBits_t eventBitLeftReleased  = (1 << 4);
		static constexpr EventBits_t eventBitRightReleased = (1 << 5);
		static constexpr EventBits_t eventBitMoveStepTimer = (1 << 6);

	private:
		State state;
		GameDisplayControl& gameDisplayControl;
		EventGroupHandle_t eventGroup;
		QueueHandle_t paddlePosQueue;		// lengte 1: was Pool<int> poolPaddlePos
		TimerHandle_t moveStepTimer;		// periodiek, op de beeldklok; tikt door zolang een knop wordt vastgehouden
		TaskHandle_t taskHandle;
		int screenWidth;
		int paddlePosition;
		int paddleWidth;
		int paddleDirection;		// -1 naar links, +1 naar rechts, 0 = stil
		IButton* pButtonLeft;
		IButton* pButtonRight;

	public:
		PaddleControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, GameDisplayControl& gameDisplayControl,
		              frt::IADC_buttons& adcButtons) :
			state(STATE_DISABLED),
			gameDisplayControl(gameDisplayControl),
			eventGroup(xEventGroupCreate()),
			paddlePosQueue(xQueueCreate(1, sizeof(int))),
			moveStepTimer(xTimerCreate("MoveStepTimer", pdMS_TO_TICKS(FrameIntervalMs), pdTRUE, this, moveStepTimerCallback)),
			taskHandle(nullptr),
			screenWidth(gameDisplayControl.getWidth()),
			paddlePosition(screenWidth/2),
			paddleWidth(PaddleWidthPix),
			paddleDirection(0),
			pButtonLeft(nullptr),
			pButtonRight(nullptr)
		{
			xQueueOverwrite(paddlePosQueue, &paddlePosition);
			pButtonLeft = gameDisplayControl.addLeftButtonListener(this);
			pButtonRight = gameDisplayControl.addRightButtonListener(this);
			adcButtons.addButtonListener(this);
			xTaskCreatePinnedToCore(staticMain, taskName, taskSizeBytes, this, taskPriority, &taskHandle, taskCoreNumber);
		}

	int getPaddlePosition()
	{
		int tempPaddlePos = 0;
		xQueuePeek(paddlePosQueue, &tempPaddlePos, 0);
		return tempPaddlePos;
	}

    void buttonPressed(IButton* pButton)
    {
		ESP_LOGI("PaddleControl", "%s", pButton->getButtonName());

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

	// Van ADC_buttons, uit diens taak. Alleen een eventbit zetten, net als bij de touchscreenknoppen.
	void buttonPressed(int buttonIndex)
	{
		if (buttonIndex == AdcButtonLeft)
		{
			xEventGroupSetBits(eventGroup, eventBitLeftPressed);
		}
		else if (buttonIndex == AdcButtonRight)
		{
			xEventGroupSetBits(eventGroup, eventBitRightPressed);
		}
		else
		{
			// not interested.
		}
	}

	void buttonReleased(int buttonIndex)
	{
		if (buttonIndex == AdcButtonLeft)
		{
			xEventGroupSetBits(eventGroup, eventBitLeftReleased);
		}
		else if (buttonIndex == AdcButtonRight)
		{
			xEventGroupSetBits(eventGroup, eventBitRightReleased);
		}
		else
		{
			// not interested.
		}
	}

	void enable()
	{
		xEventGroupSetBits(eventGroup, eventBitEnable);
	}

	void disable()
	{
		xEventGroupSetBits(eventGroup, eventBitDisable);
	}

	private:
		static void staticMain(void* pvParameters)
		{
			static_cast<PaddleControl*>(pvParameters)->main();
		}

		static void moveStepTimerCallback(TimerHandle_t xTimer)
		{
			PaddleControl* instance = static_cast<PaddleControl*>(pvTimerGetTimerID(xTimer));
			xEventGroupSetBits(instance->eventGroup, eventBitMoveStepTimer);
		}

		// Een stap in de gevraagde richting, tegen de randen aan begrensd.
		int calcNewPaddlePosition(int paddlePosition, int direction)
		{
			int minPosition = WallThicknessPix + paddleWidth/2;
			int maxPosition = screenWidth - WallThicknessPix - paddleWidth/2;

			paddlePosition += direction * PaddleStepPix;

			if (paddlePosition < minPosition) { paddlePosition = minPosition; }
			if (paddlePosition > maxPosition) { paddlePosition = maxPosition; }

			return  paddlePosition;
		}

		void stopMoving()
		{
			xTimerStop(moveStepTimer, 0);
			paddleDirection = 0;
		}

		void movePaddle()
		{
			paddlePosition=calcNewPaddlePosition(paddlePosition,paddleDirection);
			xQueueOverwrite(paddlePosQueue, &paddlePosition);		// facilitatie getPaddlePosition()
			gameDisplayControl.updatePaddlePosition(paddlePosition);
		}

		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

			while (true)
			{
				switch(state)
				{
					case State::STATE_DISABLED:
						xEventGroupWaitBits(eventGroup, eventBitEnable, pdTRUE, pdFALSE, portMAX_DELAY);
						xEventGroupClearBits(eventGroup, eventBitDisable);		// een disable van voor deze enable telt niet meer mee.
						// Drukken die tijdens het uitgeschakeld zijn zijn blijven
						// staan, mogen de peddel niet meteen op weg sturen.
						xEventGroupClearBits(eventGroup, eventBitLeftPressed | eventBitRightPressed);
						// Tekenen doet DisplayControl - dat is de hele reden dat de
						// peddelknoppen daar wonen en niet hier.
						gameDisplayControl.showPaddle();
						state = State::STATE_ENABLED;
					break;

					case State::STATE_ENABLED:
					{
						// Zoals CleanRTOS' waitAny + hasFired: niet wissen bij het
						// wakker worden, maar alleen het bit dat verwerkt wordt.
						EventBits_t bits = xEventGroupWaitBits(eventGroup,
							eventBitLeftPressed | eventBitRightPressed | eventBitDisable,
							pdFALSE, pdFALSE, portMAX_DELAY);
						if (bits & eventBitDisable)
						{
							xEventGroupClearBits(eventGroup, eventBitDisable);
							gameDisplayControl.hidePaddle();
							state = State::STATE_DISABLED;
							break;
						}
						else if (bits & eventBitLeftPressed)
						{
							xEventGroupClearBits(eventGroup, eventBitLeftPressed);
							paddleDirection = -1;
						}
						else if (bits & eventBitRightPressed)
						{
							xEventGroupClearBits(eventGroup, eventBitRightPressed);
							paddleDirection = +1;
						}
						else
						{
							assert(false);
						}

						movePaddle();
						xEventGroupClearBits(eventGroup, eventBitLeftReleased | eventBitRightReleased);	// een loslaten van voor deze druk telt niet mee.
						xEventGroupClearBits(eventGroup, eventBitMoveStepTimer);
						xTimerStart(moveStepTimer, portMAX_DELAY);
						state = State::STATE_MOVING;
					}
					break;

					case State::STATE_MOVING:
					{
						EventBits_t bits = xEventGroupWaitBits(eventGroup,
							eventBitLeftReleased | eventBitRightReleased | eventBitDisable | eventBitMoveStepTimer,
							pdFALSE, pdFALSE, portMAX_DELAY);
						if (bits & eventBitDisable)
						{
							xEventGroupClearBits(eventGroup, eventBitDisable);
							stopMoving();
							gameDisplayControl.hidePaddle();
							state = State::STATE_DISABLED;
						}
						else if (bits & eventBitLeftReleased)
						{
							xEventGroupClearBits(eventGroup, eventBitLeftReleased);
							stopMoving();
							state = State::STATE_ENABLED;
						}
						else if (bits & eventBitRightReleased)
						{
							xEventGroupClearBits(eventGroup, eventBitRightReleased);
							stopMoving();
							state = State::STATE_ENABLED;
						}
						else if (bits & eventBitMoveStepTimer)
						{
							xEventGroupClearBits(eventGroup, eventBitMoveStepTimer);
							movePaddle();
						}
						else
						{
							assert(false);
						}
					}
					break;

					default:
						assert(false);
					break;
				}
			}
		}
	}; // end class PaddleControl

};// end namespace pong_super_frt
