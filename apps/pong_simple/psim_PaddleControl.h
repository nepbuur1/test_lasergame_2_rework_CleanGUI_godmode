// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include "psim_DisplayControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_simple
{
	// Zie psim_BallControl.h: logger komt uit namespace crt, niet uit deze.

	class PaddleControl : public Task, public IButtonListener
	{
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
		Pool<int> poolPaddlePos;
		Flag flagEnable;
		Flag flagDisable;
        Flag flagLeftPressed;
		Flag flagRightPressed;
		Flag flagLeftReleased;
		Flag flagRightReleased;
		Timer repeatTimer;			// tikt door zolang een knop wordt vastgehouden.
		int screenWidth;
		int paddlePosition;
		int paddleWidth;
		int paddleDirection;		// -1 naar links, +1 naar rechts, 0 = stil
		IButton* pButtonLeft;
		IButton* pButtonRight;

	public:
		PaddleControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, DisplayControl& displayControl) :	
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber), 
			state(STATE_DISABLED),
			displayControl(displayControl),
			flagEnable(this),
			flagDisable(this),
			flagLeftPressed(this),
			flagRightPressed(this),
			flagLeftReleased(this),
			flagRightReleased(this),
			repeatTimer(this),
			screenWidth(displayControl.getWidth()),
			paddlePosition(screenWidth/2),
			paddleWidth(PaddleWidthPix),
			paddleDirection(0),
			pButtonLeft(nullptr),
			pButtonRight(nullptr)
		{
			poolPaddlePos.write(paddlePosition);
			pButtonLeft = displayControl.addLeftButtonListener(this);
			pButtonRight = displayControl.addRightButtonListener(this);
			start();
		}

	int getPaddlePos()
	{
		int tempPaddlePos;
		poolPaddlePos.read(tempPaddlePos);
		return tempPaddlePos;
	}

    void buttonPressed(IButton* pButton)
    {
		logger.logText(pButton->getButtonName());

		if(pButton == pButtonLeft)
		{
        	flagLeftPressed.set();
		}
		else if(pButton == pButtonRight)
		{
			flagRightPressed.set();
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
			flagLeftReleased.set();
		}
		else if(pButton == pButtonRight)
		{
			flagRightReleased.set();
		}
		else
		{
			// not interested.
		}
	};

	void enable()
	{
		flagEnable.set();
	}

	void disable()
	{
		flagDisable.set();
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
			poolPaddlePos.write(paddlePosition);		// facilitatie getPaddlePos()
		}

		void stopMoving()
		{
			repeatTimer.stop();
			paddleDirection = 0;
		}

		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

			while (true)
			{
				vTaskDelay(1);
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				switch(state)
				{
					case State::STATE_DISABLED:
						wait(flagEnable);
						flagDisable.clear();		// een disable van voor deze enable telt niet meer mee.
						// Drukken die tijdens het uitgeschakeld zijn zijn blijven
						// staan, mogen de peddel niet meteen op weg sturen.
						flagLeftPressed.clear();
						flagRightPressed.clear();
						flagLeftReleased.clear();
						flagRightReleased.clear();
						// Tekenen doet DisplayControl - dat is de hele reden dat de
						// peddelknoppen daar wonen en niet hier.
						displayControl.showPaddle();
						state = State::STATE_ENABLED;
					break;

					case State::STATE_ENABLED:
						waitAny(flagLeftPressed + flagRightPressed + flagDisable);
						if (hasFired(flagDisable))
						{
							displayControl.hidePaddle();
							state = State::STATE_DISABLED;
						}
						else if(hasFired(flagLeftPressed))
						{
							paddleDirection = -1;
						}
						else if(hasFired(flagRightPressed))
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
							flagLeftReleased.clear();	// een loslaten van voor deze druk telt niet mee.
							flagRightReleased.clear();
							repeatTimer.start_periodic(repeatPeriodUs);
							state = State::STATE_MOVING;
						}
					break;

					case State::STATE_MOVING:
						waitAny(flagLeftReleased + flagRightReleased + flagDisable + repeatTimer);
						if (hasFired(flagDisable))
						{
							stopMoving();
							displayControl.hidePaddle();
							state = State::STATE_DISABLED;
						}
						else if (hasFired(flagLeftReleased) || hasFired(flagRightReleased))
						{
							stopMoving();
							state = State::STATE_ENABLED;
						}
						else if (hasFired(repeatTimer))
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
