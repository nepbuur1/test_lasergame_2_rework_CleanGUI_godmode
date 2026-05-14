// by Marius Versteegen, 2023

#pragma once
#include <crt_CleanRTOS.h>
#include <crt_CharMatrix.h>
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

	struct TwoNumbers
	{
		TwoNumbers():number1(0),number2(0){}
		TwoNumbers(int number1, int number2):number1(number1),number2(number2){}
		
		int number1;
		int number2;
	};

	class ISceneDisplayControl
	{
		public:
			virtual int getWidth() = 0;
			virtual int getHeight() = 0;
			virtual void updateBallPosition(int x, int y) = 0;
			virtual void updatePaddlePosition(int x) = 0;
			virtual void enable() = 0;
			virtual void disable() = 0;
	};

	template <int WIDTH, int HEIGHT>
	class SceneDisplayControl : public Task, public ISceneDisplayControl
	{
		enum State
		{
			STATE_ENABLED,
			STATE_DISABLED
		};

	private:
		IDisplay& display;
		TFT_eSPI_IFreeFonts& freeFonts;
		Panel<0> ballPanel;
		Panel<0> paddlePanel;
		Panel<0> bottomLinePanel;
		Panel<0> leftBorderPanel;
		Panel<0> rightBorderPanel;
		Panel<0> topBorderPanel;
	
		int width;
		int height;
		int frameDelayMs;

		Pool<TwoNumbers> poolBallPosition;
		Pool<int> poolPaddlePosition;
		Flag flagEnable;
		Flag flagDisable;
		Flag flagBallPositionUpdated;
		Flag flagPaddlePositionUpdated;
		State state;

	public:
		SceneDisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, int frameDelayMs,
							IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
			display(display),
			freeFonts(freeFonts),
			ballPanel("ballPanel", /*locPos*/Vec2(width/2, height*7/10), CoordType::Pixels,
				/*size*/Vec2(50, 50), /*cornerRadius*/12, CoordType::Pixels, Alignment::MidMid,
				/*colFg*/0x00FFFF00, /*colBg*/0x00000000),
			paddlePanel("paddlePanel", /*locPos*/Vec2(width/2,height*8/10), CoordType::Pixels,
				/*size*/Vec2(100, 20), /*cornerRadius*/5, CoordType::Pixels, Alignment::BottomMid,
				/*colFg*/0x0000FFff, /*colBg*/0x00000000),
			bottomLinePanel("bottomLinePanel", /*locPos*/Vec2(0, 200), CoordType::Promillage,
				/*size*/Vec2(1000, 20), /*cornerRadius*/0, CoordType::Promillage, Alignment::BottomLeft,
				/*colFg*/0x0000FFff, /*colBg*/0x00000000),
			leftBorderPanel("leftBorderPanel", /*locPos*/Vec2(0, 0), CoordType::Promillage,
				/*size*/Vec2(10, 1000), /*cornerRadius*/0, CoordType::Promillage, Alignment::TopLeft,
				/*colFg*/0x0000FFff, /*colBg*/0x00000000),	
			rightBorderPanel("rightBorderPanel", /*locPos*/Vec2(990, 0), CoordType::Promillage,
				/*size*/Vec2(10, 1000), /*cornerRadius*/0, CoordType::Promillage, Alignment::TopRight,
				/*colFg*/0x0000FFff, /*colBg*/0x00000000),	
			topBorderPanel("topBorderPanel", /*locPos*/Vec2(0, 0), CoordType::Promillage,
				/*size*/Vec2(1000, 10), /*cornerRadius*/0, CoordType::Promillage, Alignment::TopLeft,
				/*colFg*/0x0000FFff, /*colBg*/0x00000000),	
			width(0),	
			height(0),
			frameDelayMs(frameDelayMs),
			flagEnable(this),
			flagDisable(this),
			flagBallPositionUpdated(this),
			flagPaddlePositionUpdated(this),
			state (STATE_DISABLED)
		{
			Vec2 screenSize = display.getScreenSize();
			width = screenSize.x;
			height = screenSize.y;
			poolBallPosition.write(TwoNumbers(width/2,1));
			poolPaddlePosition.write(width/2);
			start();
		}

    int getWidth()
    {
		return width;
    }

	int getHeight()
	{
		return height;
	}

	void updateBallPosition(int x, int y)
	{
		poolBallPosition.write(TwoNumbers(x,y));
		flagBallPositionUpdated.set();
	}

	void updatePaddlePosition(int x)
	{
		logger.logText("updatePaddlePosition");
		logger.logInt32(x);
		poolPaddlePosition.write(x);
		flagPaddlePositionUpdated.set();
	}

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
			vTaskDelay(1000); // wait for other threads to have started up as well.

#ifdef LOAD_GFXFF							  // Don't select font indices 0 and 1 - they are reserved.
			freeFonts.setFreeFont(10, FSB9);  // Free serif 9 pix font.
			freeFonts.setFreeFont(12, FSB12); // Free serif 12 pix font.
#endif
			PageRoot<20> pageGame("rootSceneDisplay",display);

			pageGame.addChildWidget(bottomLinePanel);
			pageGame.addChildWidget(leftBorderPanel);
			pageGame.addChildWidget(rightBorderPanel);
			pageGame.addChildWidget(topBorderPanel);
			pageGame.addChildWidget(ballPanel);
			pageGame.addChildWidget(paddlePanel);

			pageGame.hide(true);
			pageGame.disable(true);

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.
				vTaskDelay(1);
				switch(state)
				{
					case STATE_ENABLED:
						logger.logText("pageGame.show(STATE_ENABLED)");
						waitAny(flagDisable+flagBallPositionUpdated+flagPaddlePositionUpdated);
						if(hasFired(flagDisable))
						{
							pageGame.hide(true);
							pageGame.disable(true);
							state = STATE_DISABLED;
						}
						else if(hasFired(flagPaddlePositionUpdated))
						{
							int paddlePosition;
							poolPaddlePosition.read(paddlePosition);
							paddlePanel.setLocPos(Vec2(paddlePosition,1), CoordType::Pixels, Alignment::MidMid);
							// state blijft STATE_ENABLED
							state = STATE_ENABLED;
						}
						else if(hasFired(flagBallPositionUpdated))
						{
							TwoNumbers ballPosition;
							poolBallPosition.read(ballPosition);
							ballPanel.setLocPos(Vec2(ballPosition.number1,ballPosition.number2), CoordType::Pixels, Alignment::MidMid);
							// state blijft STATE_ENABLED
							state = STATE_ENABLED;
						}
						break;

					case STATE_DISABLED:
						wait(flagEnable);
						flagDisable.clear();
						pageGame.show(true);
						pageGame.enable(true);
						state = STATE_ENABLED;
						break;
					default:
						assert(false);
				}
			}
		}
	}; // end class SceneDisplayControl

};// end namespace crt
