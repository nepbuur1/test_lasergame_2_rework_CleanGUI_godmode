// by Marius Versteegen, 2023

#pragma once
#include <Arduino.h>
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>
#include <crt_Panel.h>		// het achtergrondpaneel
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

namespace crt
{
	class TestTouchscreenButtonGroup : public Task, public IButtonListener
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

		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> tsButtonA;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> tsButtonB;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> tsButtonC;
		TouchscreenButton<0 /*MaxNofChildren*/, 10/*MaxButtonListenerCount*/> tsButtonD;

		TouchscreenButtonGroup<10 /*MaxNofChildren*/> tsButtonGroup;

		Flag flagBnAPressed, flagBnAReleased;
		Flag flagBnBPressed, flagBnBReleased;
		Flag flagBnCPressed, flagBnCReleased;
		Flag flagBnDPressed, flagBnDReleased;

	public:
		TestTouchscreenButtonGroup(const char *taskName, unsigned int taskPriority,unsigned int taskCoreNumber) :
			Task(taskName, taskPriority,
				10000+sizeof(freeFonts)+sizeof(tft_eSPI_DisplayAdapter)+sizeof(asyncDisplay)+
				sizeof(tsButtonA)+sizeof(tsButtonB)+sizeof(tsButtonGroup), taskCoreNumber),

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

			// fontScale=0 triggers automatical matching of fontscale with the button height.
			tsButtonA(/*name*/"bn_A",/*locPos*/Vec2(100, 100), CoordType::Promillage,
				/*size*/Vec2(500, 100), /*cornerRadius*/100, CoordType::Promillage,
				Alignment::MidMid,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "Button A", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 0, Alignment::MidMid,
			/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			tsButtonB(/*name*/"bn_B", /*locPos*/Vec2(100, 250), CoordType::Promillage,
				/*size*/Vec2(500, 100), /*cornerRadius*/100, CoordType::Promillage,
				Alignment::MidMid,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "Button B", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 0, Alignment::MidMid,
			/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			// fontScale=0 triggers automatical matching of fontscale with the button height.
			tsButtonC(/*name*/"bn_C",/*locPos*/Vec2(100, 400), CoordType::Promillage,
				/*size*/Vec2(500, 100), /*cornerRadius*/100, CoordType::Promillage,
				Alignment::MidMid,	/*colPanel*/0x00008800, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "Button C", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 0, Alignment::MidMid,
			/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			tsButtonD(/*name*/"bn_D",/*locPos*/Vec2(100, 550), CoordType::Promillage,
				/*size*/Vec2(500, 100), /*cornerRadius*/100, CoordType::Promillage,
				Alignment::MidMid,	/*colPanel*/0x00008800, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "Button D", /*buttonFont*/ 2, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 0, Alignment::MidMid,
			/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			tsButtonGroup("group"), // (no constructor parameters, so this line could be left out)

			flagBnAPressed(this), flagBnAReleased(this),
			flagBnBPressed(this), flagBnBReleased(this),
			flagBnCPressed(this), flagBnCReleased(this),
			flagBnDPressed(this), flagBnDReleased(this)
		{
			start(); // For simplicity, the task is started right away in it's constructor.
		}

	public:
		/* override */ void buttonPressed(IButton* pButton)
		{
			if (pButton == &tsButtonA)
			{
				flagBnAPressed.set();
			}
			else if (pButton == &tsButtonB)
			{
				flagBnBPressed.set();
			}
			else if (pButton == &tsButtonC)
			{
				flagBnCPressed.set();
			}
			else if (pButton == &tsButtonD)
			{
				flagBnDPressed.set();
			}
			else
			{
				ESP_LOGI(Task::taskName, "unhandled buttonpress");
			}
		}

		/* override */ void buttonReleased(IButton* pButton)
		{
			if (pButton == &tsButtonA)
			{
				flagBnAReleased.set();
			}
			else if (pButton == &tsButtonB)
			{
				flagBnBReleased.set();
			}
			else if (pButton == &tsButtonC)
			{
				flagBnCReleased.set();
			}
			else if (pButton == &tsButtonD)
			{
				flagBnDReleased.set();
			}
			else
			{
				ESP_LOGI(Task::taskName, "unhandled buttonpress");
			}
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

			// prev:
			// asyncDisplay.addTouchListener(&tsButtonA);
			// asyncDisplay.addTouchListener(&tsButtonB);
			// asyncDisplay.addTouchListener(&tsButtonC);
			// asyncDisplay.addTouchListener(&tsButtonD);
			// tsButtonA.addButtonListener(this);
			// tsButtonB.addButtonListener(this);
			// tsButtonC.addButtonListener(this);
			// tsButtonD.addButtonListener(this);
			// page1.addChildWidget(tsButtonA);
			// page1.addChildWidget(tsButtonB);
			// page1.addChildWidget(tsButtonGroup);
			// ESP_LOGI("temptest", "sizeX of pageroot:%d", page1.getSizePix().x);
			// tsButtonGroup.addChildWidget(tsButtonC);
			// ESP_LOGI("temptest", "sizeX of tsButtonC:%d", tsButtonC.getSizePix().x);

			tsButtonA.addButtonListener(this);
			tsButtonB.addButtonListener(this);
			tsButtonC.addButtonListener(this);
			tsButtonD.addButtonListener(this);

			page1.addChildWidget(backgroundPanel);		// als eerste, dus onderop
			page1.addChildWidget(tsButtonA);
			page1.addChildWidget(tsButtonB);
			page1.addChildWidget(tsButtonGroup);

			// A en B zitten in niet een buttongroup. C en D wel.
			asyncDisplay.addTouchListener(&tsButtonA);
			asyncDisplay.addTouchListener(&tsButtonB);
			asyncDisplay.addTouchListener(&tsButtonGroup);

			ESP_LOGI("temptest", "sizeX of pageroot:%d", page1.getSizePix().x);
			tsButtonGroup.addTouchListener(tsButtonC,tsButtonC);	// Dit voegt impliciet ook de widget to.
			ESP_LOGI("temptest", "sizeX of tsButtonC:%d", tsButtonC.getSizePix().x);

			tsButtonGroup.addTouchListener(tsButtonD,tsButtonD); // Dit voegt impliciet ook de widget to.

			page1.show(true);
			ESP_LOGI("temptest", "sizeX 2 of tsButtonA:%d", tsButtonA.getSizePix().x);
			ESP_LOGI("temptest", "sizeX 2 of tsButtonC:%d", tsButtonC.getSizePix().x);
			ESP_LOGI("temptest", "prom sizeX of tsButtonC:%d", tsButtonC.getSize().x);
			ESP_LOGI("temptest", "prom sizeX of tsButtonGroup:%d", tsButtonGroup.getSize().x);
			ESP_LOGI("temptest", "sizeX of tsButtonGroup:%d", tsButtonGroup.getSizePix().x);

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				waitAny(flagBnAPressed + flagBnAReleased + flagBnBPressed + flagBnBReleased
					  + flagBnCPressed + flagBnCReleased + flagBnDPressed + flagBnDReleased);
				if (hasFired(flagBnAPressed))
				{
					ESP_LOGI(taskName, "Button A pressed");
				}
				else if (hasFired(flagBnAReleased))
				{
					ESP_LOGI(taskName, "Button A released");
				}

				if (hasFired(flagBnBPressed))
				{
					ESP_LOGI(taskName, "Button B pressed");
				}
				else if (hasFired(flagBnBReleased))
				{
					ESP_LOGI(taskName, "Button B released");
				}

				if (hasFired(flagBnCPressed))
				{
					ESP_LOGI(taskName, "Button C pressed");
				}
				else if (hasFired(flagBnCReleased))
				{
					ESP_LOGI(taskName, "Button C released");
				}

				if (hasFired(flagBnDPressed))
				{
					ESP_LOGI(taskName, "Button D pressed");
				}
				else if (hasFired(flagBnDReleased))
				{
					ESP_LOGI(taskName, "Button D released");
				}
			}
		}
	}; // end class TestTimers
};// end namespace crt
