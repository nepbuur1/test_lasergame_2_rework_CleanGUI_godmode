// by Marius Versteegen, 2023

#pragma once
#include <Arduino.h>
#include <esp_timer.h>
#include <crt_CleanRTOS.h>
#include <TFT_eSPI.h>              // Hardware-specific library
#include <crt_TFT_eSPI_DisplayAdapter.h>
#include <crt_Panel.h>
#include <crt_PageRoot.h>
#include "images/_allImages.h"   // imageGradient and imageRing, both in flash.

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>


namespace crt
{
	class TestPanel : public Task
	{
	public:
		TestPanel(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber)
		{
			start(); // For simplicity, the task is started right away in it's constructor.
		}

	private:
		static const int MaxNofTouchListeners = 20;

		// Exponential moving average, integer style:
		//     ema += (measured - ema) / EmaDivisor
		// which is  ema = alpha*measured + (1-alpha)*ema  with alpha = 1/EmaDivisor.
		// No history buffer needed - just the one accumulator.
		// Note that the integer division makes the average stop moving once the
		// difference drops below EmaDivisor microseconds; at ~30 ms per frame
		// that is a resolution of 0.05%, far better than the measurement itself.
		static const int EmaDivisor = 16;

		static void updateEma(int64_t& emaUs, int64_t measuredUs)
		{
			if (emaUs == 0)
			{
				emaUs = measuredUs;		// seed on the first sample
			}
			else
			{
				emaUs += (measuredUs - emaUs) / EmaDivisor;
			}
		}

		void main()
		{
			TFT_eSPI_FreeFonts<20/*MaxNofFreeFonts*/> freeFonts;		// not used when LOAD_GFXFF is not defined.
			TFT_eSPI_DisplayAdapter<MaxNofTouchListeners> tft_eSPI_DisplayAdapter(freeFonts, "/TouchCalData1"/*filenameCalibration*/, false/*bRepeatCalibration*/);

			// Initialize the display without calibration (since we don't need touch for this example)
			tft_eSPI_DisplayAdapter.begin(3/*rotation*/);

			IDisplay& display = tft_eSPI_DisplayAdapter; // To make it evident that the rest of the code is
			                                      // display independent.
			PageRoot<4> page1("root",display);

			Panel<0> panel("panel 1", /*locPos*/Vec2(10, 10), CoordType::Pixels,
				/*size*/Vec2(80, 80), /*cornerRadius*/0, CoordType::Pixels, Alignment::TopLeft,
				/*colFg*/0x00FF0000, /*colBg*/0x00000000);
			page1.addChildWidget(panel);

			// --- Image panels ---------------------------------------------------
			// A Panel that got an Image at construction blits that image instead of
			// painting a plain rectangle. The image is aligned within the panel
			// according to the panel's Alignment, and clipped to the panel.

			// 1) Panel larger than the image, image centered.
			//    The image is opaque and does not cover the whole panel, so the
			//    panel first paints its colFg and then blits the image on top:
			//    you see a dark blue border around the gradient.
			Panel<0> panelImageCentered("image centered", /*locPos*/Vec2(100, 10), CoordType::Pixels,
				/*size*/Vec2(84, 68), /*cornerRadius*/0, CoordType::Pixels, Alignment::MidMid,
				/*colFg*/0x00000080, /*colBg*/0x00000000, imageGradient);
			page1.addChildWidget(panelImageCentered);

			// 2) Panel smaller than the image: the image is clipped to the panel.
			//    Aligned bottom right, so the bottom right corner of the gradient
			//    (including its white frame) is what survives.
			Panel<0> panelImageClipped("image clipped", /*locPos*/Vec2(200, 10), CoordType::Pixels,
				/*size*/Vec2(40, 40), /*cornerRadius*/0, CoordType::Pixels, Alignment::BottomRight,
				/*colFg*/0x00808000, /*colBg*/0x00000000, imageGradient);
			page1.addChildWidget(panelImageClipped);

			// 3) A transparent image on top of a coloured parent panel.
			//    The child panel paints no background of its own, so the green of
			//    its parent remains visible through the transparent pixels of the
			//    ring - which is exactly what a background fill would have ruined.
			Panel<1> panelBackdrop("backdrop", /*locPos*/Vec2(10, 100), CoordType::Pixels,
				/*size*/Vec2(230, 130), /*cornerRadius*/12, CoordType::Pixels, Alignment::TopLeft,
				/*colFg*/0x00006020, /*colBg*/0x00000000);
			page1.addChildWidget(panelBackdrop);

			Panel<0> panelImageTransparent("image transparent", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(1000, 1000), /*cornerRadius*/0, CoordType::Promillage,
				Alignment::MidMid, /*colFg*/0x00000000, /*colBg*/0x00000000, imageRing);
			panelBackdrop.addChildWidget(panelImageTransparent);

			vTaskDelay(1000); // wait for other threads to have started up as well.

			// Redraw as fast as the SPI bus allows, and report an exponential
			// moving average of the frame rate every 2 seconds.
			// Every panel is timed separately, so that it is visible where the
			// time actually goes. The fullscreen clear alone moves
			// 320*240*2 = 153.600 bytes, which at ~5 MB/s effective is ~31 ms.
			int64_t emaFrameUs    = 0;
			int64_t emaClearUs    = 0;
			int64_t emaRedUs      = 0;
			int64_t emaCenteredUs = 0;
			int64_t emaClippedUs  = 0;
			int64_t emaBackdropUs = 0;
			int64_t emaRingUs     = 0;

			int64_t timeFrameStartUs = esp_timer_get_time();
			int64_t timeLastReportUs = timeFrameStartUs;

			while (true)
			{
				int64_t t0 = esp_timer_get_time();
				display.fillScreen(0x00000000);

				// Drawn one by one instead of via page1.show(true), purely so
				// that each panel can be timed. The drawing is identical.
				int64_t t1 = esp_timer_get_time();
				panel.show(false);
				int64_t t2 = esp_timer_get_time();
				panelImageCentered.show(false);
				int64_t t3 = esp_timer_get_time();
				panelImageClipped.show(false);
				int64_t t4 = esp_timer_get_time();
				panelBackdrop.show(false);				// parent first,
				int64_t t5 = esp_timer_get_time();
				panelImageTransparent.show(false);		// then its child on top.
				int64_t t6 = esp_timer_get_time();

				updateEma(emaClearUs,    t1 - t0);
				updateEma(emaRedUs,      t2 - t1);
				updateEma(emaCenteredUs, t3 - t2);
				updateEma(emaClippedUs,  t4 - t3);
				updateEma(emaBackdropUs, t5 - t4);
				updateEma(emaRingUs,     t6 - t5);
				updateEma(emaFrameUs,    t6 - timeFrameStartUs);
				timeFrameStartUs = t6;

				if ((t6 - timeLastReportUs) >= 2000000)
				{
					timeLastReportUs = t6;

					// Integer math throughout: no %f in the log format, so that
					// this keeps working with newlib nano formatting.
					int fps100   = (int)((100LL * 1000000LL) / emaFrameUs);	// hundredths of a frame per second
					int clear100 = (int)(emaClearUs / 10);					// hundredths of a millisecond

					ESP_LOGI("TestPanel", "%d.%02d fps | clear %d.%02d ms",
						fps100 / 100, fps100 % 100, clear100 / 100, clear100 % 100);
					ESP_LOGI("TestPanel", "  show (us): red %d | centered %d | clipped %d | backdrop %d | ring %d",
						(int)emaRedUs, (int)emaCenteredUs, (int)emaClippedUs,
						(int)emaBackdropUs, (int)emaRingUs);
				}

				// Yield, so that the idle task on this core gets to run and the
				// task watchdog stays quiet. One tick is 1 ms (FREERTOS_HZ=1000),
				// which is well below the frame time - it does show up in the
				// frame rate, but not in the clear/show timings.
				vTaskDelay(1);
			}
		}
	}; // end class TestTimers
};// end namespace crt
