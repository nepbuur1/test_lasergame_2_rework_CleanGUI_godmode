// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include "crt_SceneDisplayControl.h"
#include "crt_BallControl.h"
#include "crt_PaddleControl.h"

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

	enum State
	{
		STATE_INITIAL,
		STATE_KLIK_OM_TE_STARTEN,
		STATE_GAME_RUNNING,
		STATE_GAME_OVER
	};

	class GameControl : public Task, public IButtonListener, public IBallControlListener
	{
	private:
	    State state;
		IDisplay& display;
		TFT_eSPI_IFreeFonts& freeFonts;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> buttonStart;
        Flag flagStartPressed;
		Flag flagBallMissedByPaddle;
		ISceneDisplayControl& sceneDisplayControl;
		BallControl& ballControl;
		PaddleControl& paddleControl;

	public:
		GameControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts, ISceneDisplayControl& sceneDisplayControl, BallControl& ballControl, PaddleControl& paddleControl) :	
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber), 
			state(STATE_INITIAL),
			display(display),
			freeFonts(freeFonts),
			buttonStart
				("start",/*locPos*/Vec2(500, 500), CoordType::Promillage,
				/*size*/Vec2(800, 40), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::MidMid,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				"Click here to Start", /*buttonFont*/ 1, /*colFont*/ 0x0000FFFF, /*fontScale*/ 0, Alignment::MidMid),
			flagStartPressed(this),
			flagBallMissedByPaddle(this),
			sceneDisplayControl(sceneDisplayControl),
			ballControl(ballControl),
			paddleControl(paddleControl)
		{
			display.addTouchListener(&buttonStart);
			buttonStart.addButtonListener(this);
			ballControl.setBallControlListener(this);
			start();
		}

    void buttonPressed(IButton* pButton)
    {
		logger.logText(pButton->getButtonName());

		if(pButton == &buttonStart)
		{
        	flagStartPressed.set();
		}
		else
		{
			;// not interested.
		}
    }

	// must override pure virtual function
	void buttonReleased(IButton* pButton)
	{
		// do nothing
	};

	void ballMissedByPaddle()
	{
		flagBallMissedByPaddle.set();
	}

	private:

		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

#ifdef LOAD_GFXFF							  // Don't select font indices 0 and 1 - they are reserved.
			freeFonts.setFreeFont(10, FSB9);  // Free serif 9 pix font.
			freeFonts.setFreeFont(12, FSB12); // Free serif 12 pix font.
#endif
			PageRoot<20> pageStart("rootStartMenu",display);
			pageStart.addChildWidget(buttonStart);
			
			pageStart.show(true);
			pageStart.enable(true);

			while (true)
			{
				vTaskDelay(1);
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				switch(state)
				{
					case STATE_INITIAL:
						logger.logText("STATE_INITIAL");
						vTaskDelay(200);
						ESP_LOGI("", "\n\n\nKlik op de linker of rechter knop om te starten.");
						state = STATE_KLIK_OM_TE_STARTEN;
						break;

					case STATE_KLIK_OM_TE_STARTEN:
						logger.logText("STATE_KLIK_OM_TE_STARTEN");
						vTaskDelay(200);
						flagStartPressed.clear();  // clear any pending button presses still meant to controll the paddle.
						wait(flagStartPressed);
						pageStart.hide(true);
						pageStart.disable(true);
						ballControl.startBall();
						paddleControl.enable();
						sceneDisplayControl.enable();
						state = STATE_GAME_RUNNING;
						break;
						
					case STATE_GAME_RUNNING:
						logger.logText("STATE_GAME_RUNNING");
						wait(flagBallMissedByPaddle);
						state = STATE_GAME_OVER;
						break;

					case STATE_GAME_OVER:
						sceneDisplayControl.disable();
						vTaskDelay(200); // wacht op evt parallel afrenderen van scene.
						logger.logText("STATE_GAME_OVER");

						ESP_LOGI("Game Over", "\n"
						"  _____                         ____                  \n"
						" / ____|                       / __ \\                 \n"
						"| |  __  __ _ _ __ ___   ___  | |  | |_   _____ _ __  \n"
						"| | |_ |/ _` | '_ ` _ \\ / _ \\ | |  | \\ \\ / / _ \\ '__| \n"
						"| |__| | (_| | | | | | |  __/ | |__| |\\ V /  __/ |    \n"
						" \\_____|\\__,_|_| |_| |_|\\___|  \\____/  \\_/ \\___|_|    \n"
						"\n"
						"Klik op de linker of rechter knop om opnieuw te starten.");

						state = STATE_KLIK_OM_TE_STARTEN;
						break;

					default:
						assert(false);
				}

				vTaskDelay(1);
			}
		}
	}; // end class PaddleControl

};// end namespace crt
