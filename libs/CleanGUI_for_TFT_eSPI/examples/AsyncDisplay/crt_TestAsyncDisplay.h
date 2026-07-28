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
#include <crt_TFT_eSPI_FreeFonts.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

//#include "crt_string.h"

namespace crt
{
	class TestAsyncDisplay : public Task
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
		AsyncDisplay<CommandBufferSize, MaxNofClientTasks> displayControl;

	public:
		TestAsyncDisplay(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber),
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
			displayControl("AsyncDisplay", 2 /*priority*/, 8000 /*stackBytes*/, ARDUINO_RUNNING_CORE, tft_eSPI_DisplayAdapter, 3/*rotation*/, 2/*fontTouchCalibration*/)
		{
			start(); // For simplicity, the task is started right away in it's constructor.
		}

	private:
		void main()
		{
	
#ifdef LOAD_GFXFF							  // Don't select font indices 0 and 1 - they are reserved.
			freeFonts.setFreeFont(10, FSB9);  // Free serif 9 pix font.
			freeFonts.setFreeFont(12, FSB12); // Free serif 12 pix font.
#endif

			IDisplay& display = displayControl;   // To make it evident that the rest of the code is
			                                      // display independent.
			// Zonder palet mapt de PAL8-adapter elke kleur op index 0 en blijft er
			// een scherm in die ene kleur over.
			tft_eSPI_DisplayAdapter.setPalette(arPalette);

			PageRoot<2> page1("root", display);		// 2: het achtergrondpaneel telt mee

			Panel<12> panel("panel", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(100, 100), /*cornerRadius*/0, CoordType::Pixels, Alignment::TopLeft,
				/*colFg*/0x00FF0000, /*colBg*/0x00000000);
			page1.addChildWidget(backgroundPanel);		// als eerste, dus onderop
			page1.addChildWidget(panel);

			Text<0> textTL(/*locPos*/Vec2(0, 0), CoordType::Pixels, Alignment::TopLeft,
				"TL", /*font*/2, /*color*/0x00FFFFFF, /*scale*/1);
			Text<0> textML(/*locPos*/Vec2(0, 0), CoordType::Pixels, Alignment::MidLeft,
				"ML", /*font*/2, /*color*/0x00FFFFFF, /*scale*/1);
			Text<0> textBL(/*locPos*/Vec2(0, 0), CoordType::Pixels, Alignment::BottomLeft,
				"BL", /*font*/2, /*color*/0x00FFFFFF, /*scale*/1);
			Text<0> textTR(/*locPos*/Vec2(0, 0), CoordType::Pixels, Alignment::TopRight,
				"TR", /*font*/2, /*color*/0x00FFFFFF, /*scale*/1);
			Text<0> textMR(/*locPos*/Vec2(0, 0), CoordType::Pixels, Alignment::MidRight,
				"MR", /*font*/2, /*color*/0x00FFFFFF, /*scale*/1);
			Text<0> textBR(/*locPos*/Vec2(0, 0), CoordType::Pixels, Alignment::BottomRight,
				"BR", /*font*/2, /*color*/0x00FFFFFF, /*scale*/1);
			Text<0> textTM(/*locPos*/Vec2(0, 0), CoordType::Pixels, Alignment::TopMid,
				"TM", /*font*/2, /*color*/0x00FFFFFF, /*scale*/1);
			Text<0> textMM(/*locPos*/Vec2(0, 0), CoordType::Pixels, Alignment::MidMid,
				"MM", /*font*/2, /*color*/0x00FFFFFF, /*scale*/1);
			Text<0> textBM(/*locPos*/Vec2(0, 0), CoordType::Pixels, Alignment::BottomMid,
				"BM", /*font*/2, /*color*/0x00FFFFFF, /*scale*/1);

			panel.addChildWidget(textTL);
			panel.addChildWidget(textML);
			panel.addChildWidget(textBL);
			panel.addChildWidget(textTR);
			panel.addChildWidget(textMR);
			panel.addChildWidget(textBR);
			panel.addChildWidget(textTM);
			panel.addChildWidget(textMM);
			panel.addChildWidget(textBM);

			vTaskDelay(1000); // wait for other threads to have started up as well.

			display.fillScreen(0x00000000);

			displayControl.setPrintFont(this, 2);
			displayControl.setPrintScale(this, 2);
			displayControl.setPrintCursor(this, Vec2(10, 120));
			displayControl.setPrintColor(this, 0x0000ff00);
			displayControl.setPrintWrap(this, true);
			displayControl.print(this, "Hello ");
			displayControl.println(this, "Jack");
			displayControl.println(this, "This sentence is too long and won't fit on the screen without wrapping.");

			Vec2 locPos(0, 0);
			bool bMovingRight = true;

			Vec2 screenSize = displayControl.getScreenSize();

			while (true)
			{
				//ESP_LOGI("Currently", "clearing screen");
				page1.show(true);
				vTaskDelay(10);

				//ESP_LOGI("Currently", "displaying Panel");
				//display.fillRect(Vec2(10,10), Vec2(50,50), 0x0000FFFF);
				panel.hide(true);
				panel.setLocPos(locPos, CoordType::Pixels, Alignment::TopLeft);
				if (bMovingRight)
				{
					locPos.x++;
					if (locPos.x + panel.getSize().x >= screenSize.x) // y because rotated..
					{
						bMovingRight = false;
					}
				}
				else
				{
					locPos.x--;
					if (locPos.x == 0)
					{
						bMovingRight = true;
					}
				}

				//ESP_LOGI("variable locpos y", "%d",locPos.y);

				//vTaskDelay(10);
			}
		}
	}; // end class TestTimers
};// end namespace crt