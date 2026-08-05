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

using namespace crt;
namespace pong_cleangui_dg
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
		Panel<0> backgroundPanel;
		Panel<1> gameOverPanel;		// het "Game Over" hoort op het scherm, niet in de seriele monitor.
		Text<0> gameOverText;
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
			// Let op: locPos is de LINKERBOVENHOEK van de widget. De Alignment
			// hieronder geldt alleen voor de tekst binnen de knop, niet voor de
			// plek van de knop zelf. Een knop van 800 promille breed op x=500
			// promille begint dus in het midden en loopt rechts het scherm af -
			// vandaar x=100: (1000-800)/2.
			backgroundPanel
				("startBackground",/*locPos*/Vec2(0, 0), CoordType::Promillage,
				/*size*/Vec2(1000, 1000), /*cornerRadius*/0, CoordType::Promillage,
				Alignment::TopLeft,	/*colFg*/0x00000000, /*colBg*/0x00000000),
			gameOverPanel
				("gameOverPanel",/*locPos*/Vec2(100, 250), CoordType::Promillage,
				/*size*/Vec2(800, 200), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::TopLeft,	/*colFg*/0x00202020, /*colBg*/0x00000000),
			// locPos (0,0): voor een MidMid-tekst is de oorsprong al het midden van
			// de ouder (zie Text::calcOriginPix). Er nog eens 500 promille bij
			// optellen schoof het label een halve paneelbreedte naar rechts.
			// fontScale is hier expliciet: het automatisch schalen op 0 zit in
			// TouchscreenButton, niet in een kale Text.
			gameOverText
				(/*locPos*/Vec2(0, 0), CoordType::Promillage, Alignment::MidMid,
				"GAME OVER", /*font*/2, /*color*/0x00FF4040, /*fontScale*/2),
			buttonStart
				("start",/*locPos*/Vec2(100, 562), CoordType::Promillage,
				/*size*/Vec2(800, 225), /*cornerRadius*/50, CoordType::Promillage,
				Alignment::TopLeft,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "Click here to Start", /*buttonFont*/ 1, /*colFont*/ 0x0000FFFF, /*fontScale*/ 0, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),
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
			pageStart.addChildWidget(backgroundPanel);	// eerst, want alles hierna tekent erop.
			pageStart.addChildWidget(gameOverPanel);	// pas hierna heeft gameOverPanel een display,
			gameOverPanel.addChildWidget(gameOverText);	// en daar staat addChildWidget op: van boven naar beneden.
			pageStart.addChildWidget(buttonStart);

			pageStart.show(true);
			pageStart.enable(true);
			gameOverPanel.hide(true);	// bij het eerste opstarten is er nog niets verloren.

			while (true)
			{
				vTaskDelay(1);
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				switch(state)
				{
					case STATE_INITIAL:
						logger.logText("STATE_INITIAL");
						vTaskDelay(200);
						ESP_LOGI("", "\n\n\nRaak de startknop op het scherm aan om te starten.");
						state = STATE_KLIK_OM_TE_STARTEN;
						break;

					case STATE_KLIK_OM_TE_STARTEN:
						logger.logText("STATE_KLIK_OM_TE_STARTEN");
						vTaskDelay(200);
						flagStartPressed.clear();  // clear any pending button presses still meant to controll the paddle.
						flagBallMissedByPaddle.clear();	// en een gemiste bal van het vorige spel mag dit spel niet meteen beeindigen.
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
						paddleControl.disable();	// tegenhanger van de enable() hierboven: peddelknoppen weg.
						sceneDisplayControl.disable();
						vTaskDelay(200); // wacht op evt parallel afrenderen van scene.
						logger.logText("STATE_GAME_OVER");

						// Het startscherm moet hier terug. Zonder dit blijft de
						// startknop verborgen en uitgeschakeld en is er na een
						// spel geen weg meer terug.
						pageStart.show(true);
						pageStart.enable(true);

						ESP_LOGI("Game Over", "Game Over - raak de startknop aan om opnieuw te starten.");

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
