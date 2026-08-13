// by Marius Versteegen, 2023

// To view ESP_LOGI output messages in Arduino IDE, 
// set Tools -> Core Debug Level to "info" 
// and Serial Monitor baud rate to 115200
// When using ESP_IDF, make sure that CONFIG_LOG_DEFAULT_LEVEL_INFO=y in sdkconfig

#pragma once
#include <Arduino.h>
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>
#include <crt_Panel.h>		// het achtergrondpaneel
#include <crt_IKeyboardListener.h>
#include <crt_TouchscreenKeyboard.h>
#include <crt_AsyncDisplay.h>
#include <crt_TFT_eSPI_DisplayAdapter.h>
#include <crt_TFT_eSPI_DisplayAdapter_PAL8.h>
// Het palet van dit voorbeeld. Gegenereerd uit het handgeschreven
// images/_pal8/_userPalette.h; deze boom heeft geen plaatjes, dus de rest
// vult de converter met een kleurenkubus en grijstinten.
#include "images/_pal8/_palette.h"

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include <crt_Logger.h>

namespace crt
{
	class TestTouchscreenKeyboard : public Task, public IKeyboardListener, public ITouchListener
	{
		static const int CommandBufferSize = 1000;
		static const int MaxNofClientTasks = 10;
		static const int MaxNofTouchListeners = 20;

		// Kies hier de display-adapter. RGB565 tekent rechtstreeks op het paneel:
		// elk commando staat er meteen op. RGB332 tekent in een schermvullende
		// backbuffer van 8 bits per pixel en stuurt die bij present() in een keer
		// uit - vaste beeldtijd, geen flikkering, 256 vaste kleuren, 76.802 bytes.
		//using DISPLAYADAPTER = TFT_eSPI_DisplayAdapter_RGB565<MaxNofTouchListeners>;
		using DISPLAYADAPTER = TFT_eSPI_DisplayAdapter_PAL8<MaxNofTouchListeners>;

	private:
		TFT_eSPI_FreeFonts<20/*MaxNofFreeFonts*/> freeFonts;		// not used when LOAD_GFXFF is not defined.
		DISPLAYADAPTER tft_eSPI_DisplayAdapter;

		// PageRoot tekent zelf niets, dus zonder een widget die de achtergrond
		// bezit is er niemand die de vrije ruimte vult. Bij de directe adapter
		// viel dat niet op omdat begin() het scherm een keer zwart maakt; een
		// backbuffer die met de transparante index begint, laat het gat zien -
		// als magenta, want dat is wat index 255 in het palet is.
		Panel<0> backgroundPanel;
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

			backgroundPanel("background", /*locPos*/Vec2(0, 0), CoordType::Promillage,
				/*size*/Vec2(1000, 1000), /*cornerRadius*/0, CoordType::Promillage, Alignment::TopLeft,
				/*colFg*/(uint32_t)0x00000000, /*colBg*/0x00000000),


			// 8000 stack: gemeten met dumpStackHighWaterMarkIfIncreased onder de
			// zwaarste belasting die de voorbeelden kennen (MultiPanel met 20
			// sprites, Inputfield met schuivend toetsenbord en free fonts) raakt
			// deze taak ~2,5 KB aan. Bewust geen formule met CommandBufferSize
			// erin: de commandobuffer is een FreeRTOS-queue en staat op de heap,
			// niet op deze stack, en de PrintContexts zijn leden van AsyncDisplay.
			asyncDisplay("AsyncDisplay", 2 /*priority*/, 8000 /*stackBytes*/, ARDUINO_RUNNING_CORE, 
				            tft_eSPI_DisplayAdapter, 3/*rotation*/, 2/*fontTouchCalibration*/),

			outputPanel("outputPanel", /*locPos*/Vec2(0, 0), CoordType::Promillage,
				/*size*/Vec2(1000, 400), /*cornerRadius*/0, CoordType::Promillage, Alignment::TopLeft,
				/*colFg*/(uint32_t)0x00666666, /*colBg*/0x00000000),

			touchscreenKeyboard("touchscreenKeyboard", /*locPos*/Vec2(0, 500), CoordType::Promillage,
				/*size*/Vec2(1000, 500), /*cornerRadius 0,*/ CoordType::Promillage, Alignment::TopLeft,
				// colFg is de rug van het toetsenbord, waar de kieren tussen de
				// toetsen in vallen.
				/*colFg*/(uint32_t)0x00222222, /*colBg*/0x00000000, TouchscreenKeyboardMode::LowercaseAlphabet),
				
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
			// Zonder palet mapt de PAL8-adapter elke kleur op index 0 en blijft er
			// een scherm in die ene kleur over.
			tft_eSPI_DisplayAdapter.setPalette(arPalette);

			PageRoot<10> page1("root", display);
			asyncDisplay.addTouchListener(&touchscreenKeyboard);
			asyncDisplay.addTouchListener(this);

			touchscreenKeyboard.addKeyboardListener(this);

			ESP_LOGI("testkb", "88");

			page1.addChildWidget(backgroundPanel);		// als eerste, dus onderop
			page1.addChildWidget(touchscreenKeyboard);

			// De hele pagina, niet alleen het toetsenbord: anders wordt het
			// achtergrondpaneel nooit getekend en blijft de rest van het scherm van
			// niemand. Dat viel niet op zolang begin() het scherm zwart maakte en
			// het toetsenbord daar toevallig goed op stond.
			page1.show(true);

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
					if (!queueKeyPressedStrings.tryRead(strKey)) { continue; }	// tryRead na hasFired: zie crt_Queue.h
					flagTouchPressed.clear();

					if(strncmp(strKey, "OutsideKeyboardPressed", 22) == 0)
					{
						// Eerst doof, dan pas weg. Verbergen tekent - via de
						// commandobuffer, dus met een venster waarin de
						// touch-taak nog langskomt - en in dat venster zou een
						// toetsenbord dat al van het scherm is nog een toets
						// kunnen melden. Zonder deze disable blijft het bovendien
						// voorgoed dood liggen versturen: zijn rug meldt elke
						// aanraking buiten het toetsenbord als
						// "OutsideKeyboardPressed", en die tak wint dan van de
						// flagTouchPressed-tak die het terugzet.
						touchscreenKeyboard.disable(true);
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
					if (!queueKeyReleasedStrings.tryRead(strKey)) { continue; }	// tryRead na hasFired: zie crt_Queue.h
					//ESP_LOGI(Task::taskName, "Key Released:%s", str);
				}
				else if (hasFired(flagTouchPressed))
				{
					// A touch event outside the keyboard was already handled bover, via a keypressed event.
					// That hid the keyboard. For the sake of this test, lets' show it again.
					if(!touchscreenKeyboard.isDrawable())
					{
						touchscreenKeyboard.show(true);

						// Tegenhanger van de disable() hierboven: tonen maakt
						// niet mee aanraakbaar, dus zonder deze regel komt het
						// toetsenbord wel terug in beeld maar blijft het dood.
						touchscreenKeyboard.enable(true);
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