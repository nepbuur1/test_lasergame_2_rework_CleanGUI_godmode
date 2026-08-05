// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include "crt_SceneDisplayControl.h"
#include "crt_BallControl.h"

#include <crt_CleanGUI.h>

//#include <crt_TFT_eSPI_DisplayAdapter.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

using namespace crt;
namespace pong_cleangui_dg
{
	extern ILogger& logger;

	class PaddleControl : public Task, public IButtonListener
	{
		enum State
		{
			STATE_ENABLED,		// wacht op een knop die ingedrukt wordt
			STATE_MOVING,		// een knop wordt vastgehouden: blijf schuiven
			STATE_DISABLED
		};

		// De peddel schuift op dezelfde beeldklok als de bal, zodat hij net zo
		// vloeiend beweegt en niet in horten en stoten.
		static constexpr uint64_t repeatPeriodUs = FrameIntervalMs * 1000;

	private:
		IDisplay& display;
		TFT_eSPI_IFreeFonts& freeFonts;
		Panel<0> backgroundPanel;	// de onderste kwart van het scherm - de rest is speelveld.
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonLeft;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonRight;
		State state;
		ISceneDisplayControl& sceneDisplayControl;
		BallControl& ballControl;
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

	public:
		PaddleControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts, ISceneDisplayControl& sceneDisplayControl, BallControl& ballControl) :	
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber), 
			display(display),
			freeFonts(freeFonts),
			// Let op: locPos is de LINKERBOVENHOEK van de widget. De Alignment
			// hieronder geldt alleen voor de tekst binnen de knop, niet voor de
			// plek van de knop zelf.
			backgroundPanel
				("paddleBackground",/*locPos*/Vec2(0, 750), CoordType::Promillage,
				/*size*/Vec2(1000, 250), /*cornerRadius*/0, CoordType::Promillage,
				Alignment::TopLeft,	/*colFg*/0x00000000, /*colBg*/0x00000000),
			buttonLeft
				("left",/*locPos*/Vec2(20, 790), CoordType::Promillage,
				/*size*/Vec2(250, 190), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::TopLeft,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "<-", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 0, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),
			buttonRight
				("right",/*locPos*/Vec2(730, 790), CoordType::Promillage,
				/*size*/Vec2(250, 190), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::TopLeft,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "->", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 0, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),
			state(STATE_DISABLED),
			sceneDisplayControl(sceneDisplayControl),
			ballControl(ballControl),
			flagEnable(this),	
			flagDisable(this),
			flagLeftPressed(this),
			flagRightPressed(this),
			flagLeftReleased(this),
			flagRightReleased(this),
			repeatTimer(this),
			screenWidth(sceneDisplayControl.getWidth()),
			paddlePosition(screenWidth/2),
			paddleWidth(PaddleWidthPix),
			paddleDirection(0)
		{
			display.addTouchListener(&buttonLeft);
			display.addTouchListener(&buttonRight);
			buttonLeft.addButtonListener(this);
			buttonRight.addButtonListener(this);
			start();
		}

    void buttonPressed(IButton* pButton)
    {
		logger.logText(pButton->getButtonName());

		if(pButton == &buttonLeft)
		{
        	flagLeftPressed.set();
		}
		else if(pButton == &buttonRight)
		{
			flagRightPressed.set();
		}
		else
		{
			assert(false);
		}
    }

	// De knop loslaten is hier net zo goed een gebeurtenis als hem indrukken:
	// daartussen schuift de peddel door.
	void buttonReleased(IButton* pButton)
	{
		if(pButton == &buttonLeft)
		{
			flagLeftReleased.set();
		}
		else if(pButton == &buttonRight)
		{
			flagRightReleased.set();
		}
		else
		{
			assert(false);
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
		// Een stap in de gevraagde richting, tegen de randen aan begrensd, en
		// meteen doorgegeven aan wie er iets aan heeft.
		void movePaddle(int direction)
		{
			int minPosition = WallThicknessPix + paddleWidth/2;
			int maxPosition = screenWidth - WallThicknessPix - paddleWidth/2;

			paddlePosition += direction * PaddleStepPix;

			if (paddlePosition < minPosition) { paddlePosition = minPosition; }
			if (paddlePosition > maxPosition) { paddlePosition = maxPosition; }

			sceneDisplayControl.updatePaddlePosition(paddlePosition);
			ballControl.updatePaddlePosition(paddlePosition);
		}

		void stopMoving()
		{
			repeatTimer.stop();
			paddleDirection = 0;
			state = State::STATE_ENABLED;
		}

		/*override keyword not supported*/
		void main()
		{
#ifdef LOAD_GFXFF							  // Don't select font indices 0 and 1 - they are reserved.
			freeFonts.setFreeFont(10, FSB9);  // Free serif 9 pix font.
			freeFonts.setFreeFont(12, FSB12); // Free serif 12 pix font.
#endif
			PageRoot<20> pagePaddles("rootPaddleControl",display);
			pagePaddles.addChildWidget(backgroundPanel);
			pagePaddles.addChildWidget(buttonLeft);
			pagePaddles.addChildWidget(buttonRight);

			pagePaddles.hide(true);
			pagePaddles.disable(true);

			vTaskDelay(1000); // wait for other threads to have started up as well.

			while (true)
			{
				vTaskDelay(1);
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				switch(state)
				{
					case State::STATE_ENABLED:
						waitAny(flagLeftPressed + flagRightPressed + flagDisable);
						if (hasFired(flagDisable))
						{
							pagePaddles.hide(true);
							pagePaddles.disable(true);
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
							pagePaddles.hide(true);
							pagePaddles.disable(true);
							state = State::STATE_DISABLED;
						}
						else if (hasFired(flagLeftReleased) || hasFired(flagRightReleased))
						{
							stopMoving();
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
					case State::STATE_DISABLED:
						wait(flagEnable);
						pagePaddles.show(true);
						pagePaddles.enable(true);
						state = State::STATE_ENABLED;
					break;
					default:
						assert(false);
					break;
				}
			}
		}
	}; // end class PaddleControl

};// end namespace crt
