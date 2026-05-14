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

namespace crt
{
	extern ILogger& logger;

	class PaddleControl : public Task, public IButtonListener
	{
		enum State
		{
			STATE_ENABLED,
			STATE_DISABLED
		};

	private:
		IDisplay& display;
		TFT_eSPI_IFreeFonts& freeFonts;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonLeft;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonRight;
		State state;
		ISceneDisplayControl& sceneDisplayControl;
		BallControl& ballControl;
		Flag flagEnable;
		Flag flagDisable;
        Flag flagLeftPressed;
		Flag flagRightPressed;
		int screenWidth;
		int paddlePosition;
		int paddleWidth;

	public:
		PaddleControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts, ISceneDisplayControl& sceneDisplayControl, BallControl& ballControl) :	
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber), 
			display(display),
			freeFonts(freeFonts),
			buttonLeft
				("left",/*locPos*/Vec2(0, 800), CoordType::Promillage,
				/*size*/Vec2(200, 200), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::MidMid,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				"<-", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 0, Alignment::MidMid),
			buttonRight
				("right",/*locPos*/Vec2(800, 800), CoordType::Promillage,
				/*size*/Vec2(200, 200), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::MidMid,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				"->", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 0, Alignment::MidMid),
			state(STATE_DISABLED),
			sceneDisplayControl(sceneDisplayControl),
			ballControl(ballControl),
			flagEnable(this),	
			flagDisable(this),
			flagLeftPressed(this),
			flagRightPressed(this),
			screenWidth(sceneDisplayControl.getWidth()),
			paddlePosition(screenWidth/2),
			paddleWidth(3)
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

	// must override pure virtual function
	void buttonReleased(IButton* pButton)
	{
		// do nothing
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
		/*override keyword not supported*/
		void main()
		{
#ifdef LOAD_GFXFF							  // Don't select font indices 0 and 1 - they are reserved.
			freeFonts.setFreeFont(10, FSB9);  // Free serif 9 pix font.
			freeFonts.setFreeFont(12, FSB12); // Free serif 12 pix font.
#endif
			PageRoot<20> pagePaddles("rootPaddleControl",display);
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
							logger.logText("Left button press detected by paddleControl");
							if (paddlePosition>(1+(paddleWidth/2)))
							{
								paddlePosition--;
							}
						}
						else if(hasFired(flagRightPressed))
						{
							logger.logText("Right button press detected by paddleControl");
							if (paddlePosition<(screenWidth-2-paddleWidth/2))
							{
								paddlePosition++;
							}
						}
						else
						{
							assert(false);
						}
						sceneDisplayControl.updatePaddlePosition(paddlePosition);
						ballControl.updatePaddlePosition(paddlePosition);
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
