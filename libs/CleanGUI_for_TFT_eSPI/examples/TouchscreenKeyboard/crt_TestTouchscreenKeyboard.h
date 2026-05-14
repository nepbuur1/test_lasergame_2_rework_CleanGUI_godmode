// by Marius Versteegen, 2023

// To view ESP_LOGI output messages in Arduino IDE, 
// set Tools -> Core Debug Level to "info" 
// and Serial Monitor baud rate to 115200
// When using ESP_IDF, make sure that CONFIG_LOG_DEFAULT_LEVEL_INFO=y in sdkconfig

#pragma once
#include <Arduino.h>
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>
#include <crt_IKeyboardListener.h>
#include <crt_TouchscreenKeyboard.h>
#include <crt_AsyncDisplay.h>
#include <crt_TFT_eSPI_DisplayAdapter.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include <crt_Logger.h>

namespace crt
{
	class TestTouchscreenKeyboard : public Task, public IKeyboardListener, public ITouchListener
	{
		static const int CommandBufferSize = 300;
		static const int MaxNofClientTasks = 10;
		static const int MaxNofTouchListeners = 20;

	private:
		TFT_eSPI_FreeFonts<20/*MaxNofFreeFonts*/> freeFonts;		// not used when LOAD_GFXFF is not defined.
		TFT_eSPI_DisplayAdapter<MaxNofTouchListeners> tft_eSPI_DisplayAdapter;
		AsyncDisplay<CommandBufferSize, MaxNofClientTasks> asyncDisplay;
		
		Panel<1> outputPanel;									
		TouchscreenKeyboard<0 /*MaxNofChildren*/, 1 /*MaxNofKeyboardListeners*/> touchscreenKeyboard;

		Queue<const char*, 10> queueKeyPressedStrings;
		Queue<const char*, 10> queueKeyReleasedStrings;
		Flag flagTouchPressed;
		
	public:
		TestTouchscreenKeyboard(const char *taskName, unsigned int taskPriority,unsigned int taskCoreNumber) :
			Task(taskName, taskPriority, 
				10000+sizeof(freeFonts)+sizeof(tft_eSPI_DisplayAdapter)+sizeof(asyncDisplay)+
				sizeof(touchscreenKeyboard) + sizeof(queueKeyPressedStrings) + 
				sizeof(queueKeyReleasedStrings), taskCoreNumber),

			tft_eSPI_DisplayAdapter(freeFonts, "/TouchCalData1"/*filenameCalibration*/, false/*bRepeatCalibration*/),

			asyncDisplay("AsyncDisplay", 2 /*priority*/, 20000 + CommandBufferSize*sizeof(DisplayCommand)+
				            MaxNofClientTasks*sizeof(PrintContext) /*stackBytes*/, ARDUINO_RUNNING_CORE, 
				            tft_eSPI_DisplayAdapter, 3/*rotation*/, 2/*fontTouchCalibration*/),

			outputPanel("outputPanel", /*locPos*/Vec2(0, 0), CoordType::Promillage,
				/*size*/Vec2(1000, 400), /*cornerRadius*/0, CoordType::Promillage, Alignment::TopLeft,
				/*colFg*/(uint32_t)0x00666666, /*colBg*/0x00000000),

			touchscreenKeyboard("touchscreenKeyboard", /*locPos*/Vec2(0, 500), CoordType::Promillage,
				/*size*/Vec2(1000, 500), /*cornerRadius 0,*/ CoordType::Promillage, Alignment::TopLeft,
				/*colFg*/(uint32_t)0x00888888, /*colBg*/0x00000000, TouchscreenKeyboardMode::LowercaseAlphabet),
				
			queueKeyPressedStrings(this),
			queueKeyReleasedStrings(this),
			flagTouchPressed(this)
		{
			start(); // For simplicity, the task is started right away in it's constructor.
		}

	public:
		/* override */ void keyPressed(const char* strKey)
		{
			queueKeyPressedStrings.write(strKey);
		}

		/* override */ void keyReleased(const char* strKey)
		{
			queueKeyReleasedStrings.write(strKey);
		}

		/* override */ void touchPressed(const Vec2& pos)
		{
			flagTouchPressed.set();
		}

		/* override */ void touchReleased(const Vec2& pos)
		{
			; // not interested.
		}

	private:
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

#ifdef LOAD_GFXFF							  // Don't select font indices 0 and 1 - they are reserved.
			freeFonts.setFreeFont(10, FSB9);  // Free serif 9 pix font.
			freeFonts.setFreeFont(12, FSB12); // Free serif 12 pix font.
#endif
			IDisplay& display = asyncDisplay;   // To make it evident that the rest of the code is
			                                      // display independent.
			PageRoot<10> page1("root", display);
			asyncDisplay.addTouchListener(&touchscreenKeyboard);
			asyncDisplay.addTouchListener(this);

			touchscreenKeyboard.addKeyboardListener(this);

			ESP_LOGI("testkb", "88");

			page1.addChildWidget(touchscreenKeyboard);
			touchscreenKeyboard.show(true);

			const char* strKey;

			asyncDisplay.setPrintCursor(&asyncDisplay, Vec2(0, 0));
			asyncDisplay.setPrintColor(&asyncDisplay, 0x00FFFFFF);
			asyncDisplay.setPrintFont(&asyncDisplay, 2);
			asyncDisplay.setPrintScale(&asyncDisplay, 1);
			asyncDisplay.setPrintWrap(&asyncDisplay, true);
			asyncDisplay.println(&asyncDisplay, "TouchscreenKeyboardLowerCase test");

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				waitAny(queueKeyPressedStrings+queueKeyReleasedStrings+flagTouchPressed);

				if (hasFired(queueKeyPressedStrings))
				{
					queueKeyPressedStrings.read(strKey);
					flagTouchPressed.clear();

					if(strncmp(strKey, "OutsideKeyboardPressed", 22) == 0)
					{
						touchscreenKeyboard.hide(true);
					}
					else if(strncmp(strKey, "SHIFT", 5) == 0)
					{;}// do nothing, ignore shift key.
					else if(strncmp(strKey, "123", 3) == 0)
					{;} // do nothing, ignore 123 key.
					else if(strncmp(strKey, "abc", 3) == 0)
					{;} // do nothing, ignore abc key.
					else if(strncmp(strKey, "ENTER", 3) == 0)
					{
						asyncDisplay.print(&asyncDisplay, "\n");
					}
					else
					{
						asyncDisplay.print(&asyncDisplay, strKey);
					}
					//ESP_LOGI(Task::taskName, "Key Released:%s", str);
				}
				else if (hasFired(queueKeyReleasedStrings))
				{
					queueKeyReleasedStrings.read(strKey);
					//ESP_LOGI(Task::taskName, "Key Released:%s", str);
				}
				else if (hasFired(flagTouchPressed))
				{
					// A touch event outside the keyboard was already handled bover, via a keypressed event.
					// That hid the keyboard. For the sake of this test, lets' show it again.
					if(!touchscreenKeyboard.isShown())
					{
						touchscreenKeyboard.show(true);
					}
				}
				else
				{
					ESP_LOGI(Task::taskName, "unhandled waitable");
				}
			}
		}
	};
}// end namespace crt