// by Marius Versteegen, 2024

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
#include <crt_InputField.h>
#include <crt_AsyncDisplay.h>
#include <crt_TFT_eSPI_DisplayAdapter.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include <crt_Logger.h>

namespace crt
{
	class TestInputfield : public Task, public IInputfieldListener
	{
		static const int CommandBufferSize = 300;
		static const int MaxNofClientTasks = 10;
		static const int MaxNofTouchListeners = 20;

	private:
		TFT_eSPI_FreeFonts<20/*MaxNofFreeFonts*/> freeFonts;		// not used when LOAD_GFXFF is not defined.
		TFT_eSPI_DisplayAdapter<MaxNofTouchListeners> tft_eSPI_DisplayAdapter;
		AsyncDisplay<CommandBufferSize, MaxNofClientTasks> asyncDisplay;
		
		// Bezit de achtergrond. Nodig om het toetsenbord te kunnen laten
		// wegschuiven: PageRoot tekent zelf niets, dus zonder een widget die de
		// achtergrond bezit is er niets om de vrijgekomen strook mee te
		// herstellen en blijft het oude beeld staan.
		Panel<0> backgroundPanel;

		Panel<1> outputPanel;						
		using TOUCHSCREENKEYBOARD = TouchscreenKeyboard<0 /*MaxNofChildren*/, 1 /*MaxNofKeyboardListeners*/>;
		TOUCHSCREENKEYBOARD touchscreenKeyboard;			

		using INPUTFIELD = Inputfield<0 /*MaxNofChildren*/, 2 /*MaxNofInputfieldListeners*/, 20 /*MaxTextSize*/>;
		INPUTFIELD inputfield;

		Flag flagNewKey;
		Pool<const char*> poolNewKey;
		Flag flagInputfieldPressed;
		Flag flagOutsideInputfieldPressed;
		
	public:
		TestInputfield(const char *taskName, unsigned int taskPriority,unsigned int taskCoreNumber) :
			Task(taskName, taskPriority, 
				10000+sizeof(freeFonts)+sizeof(tft_eSPI_DisplayAdapter)+sizeof(asyncDisplay)+
				sizeof(INPUTFIELD) + sizeof(TOUCHSCREENKEYBOARD), taskCoreNumber),

			tft_eSPI_DisplayAdapter(freeFonts, "/TouchCalData1"/*filenameCalibration*/, false/*bRepeatCalibration*/),

			asyncDisplay("AsyncDisplay", 2 /*priority*/, 20000 + CommandBufferSize*sizeof(DisplayCommand)+
				            MaxNofClientTasks*sizeof(PrintContext) /*stackBytes*/, ARDUINO_RUNNING_CORE, 
				            tft_eSPI_DisplayAdapter, 3/*rotation*/, 2/*fontTouchCalibration*/),

			backgroundPanel("background", /*locPos*/Vec2(0, 0), CoordType::Promillage,
				/*size*/Vec2(1000, 1000), /*cornerRadius*/0, CoordType::Promillage, Alignment::TopLeft,
				/*colFg*/(uint32_t)0x00000000, /*colBg*/0x00000000),

			outputPanel("MenuPanel", /*locPos*/Vec2(0, 0), CoordType::Promillage,
				/*size*/Vec2(1000, 400), /*cornerRadius*/0, CoordType::Promillage, Alignment::TopLeft,
				/*colFg*/(uint32_t)0x00666666, /*colBg*/0x00000000),

			touchscreenKeyboard("touchscreenKeyboard", /*locPos*/Vec2(0, 500), CoordType::Promillage,
				/*size*/Vec2(1000, 500), /*cornerRadius 0,*/ CoordType::Promillage, Alignment::TopLeft,
				// colFg is de rug van het toetsenbord, waar de kieren tussen de
				// toetsen in vallen. Buiten het toetsenbord blijft de achtergrond
				// van de demo staan.
				/*colFg*/(uint32_t)0x00222222, /*colBg*/0x00000000, TouchscreenKeyboardMode::LowercaseAlphabet),

			inputfield(&touchscreenKeyboard, InputfieldType::TextField, "", /*locPos*/Vec2(100, 100), CoordType::Promillage,
				// 140 promillage van 240 = 33 pix. Moet minstens de fontHeight van
				// FSB12 (29 pix) zijn, anders klipt de staart van een 'g' eraf.
				/*size*/Vec2(800, 140), /*cornerRadius 0,*/ CoordType::Promillage, Alignment::TopLeft,
				/*colPanel*/(uint32_t)0x00888888, /*colBg*/0x00000000, /*colFont*/0x00FFFFFF, InputfieldType::TextField),
			
			flagNewKey(this),
			flagInputfieldPressed(this),
			flagOutsideInputfieldPressed(this)
		{
			start(); // For simplicity, the task is started right away in it's constructor.
		}

	public:
		/* override */ void inputNewKey(IInputfield* inputfield, const char* strKey)
		{
			poolNewKey.write(strKey);
			flagNewKey.set();
		}
		/* override */ void inputfieldPressed(IInputfield* inputfield)
		{	
			flagInputfieldPressed.set();
		}
		/* override */ void outsideInputfieldPressed(IInputfield* inputfield)
		{
			flagOutsideInputfieldPressed.set();
		}


	private:
		// Schuift het toetsenbord van yFromPix naar yToPix. moveTo herstelt per
		// stap de vrijgekomen strook door de tree eroverheen te laten tekenen;
		// _bVisible en _bEnabled blijven onaangeroerd, dus er knippert niets aan
		// en uit. Wel de touch uitzetten tijdens de beweging: tikken op
		// verschuivende toetsen slaat nergens op.
		void slideKeyboard(int32_t yFromPix, int32_t yToPix)
		{
			const int32_t stepPix = 8;
			int32_t step = (yToPix > yFromPix) ? stepPix : -stepPix;

			touchscreenKeyboard.disable(true);

			int32_t y = yFromPix;
			while (y != yToPix)
			{
				y += step;
				if (((step > 0) && (y > yToPix)) || ((step < 0) && (y < yToPix)))
				{
					y = yToPix;
				}
				touchscreenKeyboard.moveTo(Vec2(0, y), CoordType::Pixels);
			}

			if (yToPix < yFromPix)		// alleen als hij in beeld staat
			{
				touchscreenKeyboard.enable(true);
			}
		}

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

			// Eerst toegevoegd, dus eerst getekend: painter's algorithm.
			page1.addChildWidget(backgroundPanel);

			asyncDisplay.addTouchListener(&inputfield);

			asyncDisplay.addTouchListener(&touchscreenKeyboard);
			page1.addChildWidget(touchscreenKeyboard);

			inputfield.addInputfieldListener(this);

			// page1.addChildWidget(outputPanel);
			// outputPanel.addChildWidget(inputfield);

			page1.addChildWidget(inputfield);

			// Het toetsenbord begint net onder het scherm, zodat het straks van
			// onderaf in beeld kan schuiven.
			Vec2 screenSizePix = display.getScreenSize();
			const int32_t yKeyboardDownPix = screenSizePix.y;
			const int32_t yKeyboardUpPix   = screenSizePix.y - touchscreenKeyboard.getSizePix().y;
			touchscreenKeyboard.setLocPos(Vec2(0, yKeyboardDownPix), CoordType::Pixels, Alignment::TopLeft);
			bool bKeyboardIsUp = false;

			page1.show(true);

#ifdef CRT_INPUTFIELD_AUTOSLIDE
			// Off unless CRT_INPUTFIELD_AUTOSLIDE is defined: zet dezelfde vlag
			// die een aanraking op het invoerveld zet, zodat de schuifanimatie
			// zonder touch reproduceerbaar is - langs precies het echte codepad.
			vTaskDelay(3000);
			ESP_LOGI("autoslide", "=== trigger via flagInputfieldPressed ===");
			flagInputfieldPressed.set();
#endif

#ifdef CRT_INPUTFIELD_AUTOTYPE
			// Off unless CRT_INPUTFIELD_AUTOTYPE is defined: drives the inputfield
			// without touch, so the redraw per keystroke can be measured
			// repeatably. keyPressed is the same entry point the keyboard uses.
			// Combine with CRT_ASYNCDISPLAY_COMMAND_TIMING to see which commands
			// a keystroke actually produces.
			vTaskDelay(5000);	// let the initial draw of the whole page finish first,
								// otherwise the keystrokes land while the command queue
								// is still working through ~30 keyboard buttons.
			ESP_LOGI("autotype", "=== start ===");
			{
				static const char* arKeys[] = { "a", "b", "c", "d", "e", "f", "g", "DEL", "DEL", "DEL", "DEL", "DEL", "DEL", "DEL" };
				for (int round = 0; round < 3; round++)
				{
					for (int i = 0; i < 14; i++)
					{
						ESP_LOGI("autotype", "--- key '%s'", arKeys[i]);
						inputfield.keyPressed(arKeys[i]);
						vTaskDelay(400);
					}
				}
			}
			ESP_LOGI("autotype", "=== done ===");
#endif

			const char* strKey;

			const int nMaxTextSize = 30; // >= that of the inputfield.
			std::string<nMaxTextSize> strFieldContent;

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				waitAny(flagInputfieldPressed+flagOutsideInputfieldPressed+flagNewKey);
				if(hasFired(flagInputfieldPressed))
				{
					ESP_LOGI(Task::taskName, "Inputfield got focus.");
					
					if(!bKeyboardIsUp)
					{
						slideKeyboard(yKeyboardDownPix, yKeyboardUpPix);
						bKeyboardIsUp = true;
					}

					inputfield.enable(true);
				}
				else if(hasFired(flagOutsideInputfieldPressed))
				{
					//inputfield.enable(false);
					//ESP_LOGI(Task::taskName, "Inputfield lost focus.");
					; // do nothing - probably the keyboard was pressed.
				}
				else if(hasFired(flagNewKey))
				{
					poolNewKey.read(strKey);
					if(strncmp(strKey, "ENTER", 5) == 0)
					{
						// The input has been completed.
						inputfield.getTextValue(strFieldContent);
						ESP_LOGI(Task::taskName, "Input completed:%s", strFieldContent.begin());
						if(bKeyboardIsUp)
						{
							slideKeyboard(yKeyboardUpPix, yKeyboardDownPix);
							bKeyboardIsUp = false;
						}
					}
				}
			}
		}
	};
}// end namespace crt