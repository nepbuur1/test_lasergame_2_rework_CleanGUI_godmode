// by Marius Versteegen, 2026

// Shows the images of the images folder one at a time, centered on the screen,
// with a button on either side to step through them.
//
// The images themselves are not in this file and not in DRAM: tools/img2rgb565.py
// turned images/**.png into const arrays in images/_rgb565/, which live in flash.
// _allImages.h declares an Image per array plus arAllImages[], so this viewer
// never has to name a single picture - add a .png, run the tool, and it shows up.

#pragma once
#include <Arduino.h>
#include <stdio.h>
#include <crt_CleanRTOS.h>
#include <crt_CleanGUI.h>
#include <crt_AsyncDisplay.h>
#include <crt_TouchscreenButton.h>
#include <crt_TFT_eSPI_DisplayAdapter.h>
#include <crt_TFT_eSPI_FreeFonts.h>

#include "images/_rgb565/_allImages.h"

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

namespace crt
{
	class TestImageViewer : public Task, public IButtonListener
	{
		static const int CommandBufferSize = 300;
		static const int MaxNofClientTasks = 10;
		static const int MaxNofTouchListeners = 20;

	private:
		TFT_eSPI_FreeFonts<20/*MaxNofFreeFonts*/> freeFonts;		// not used when LOAD_GFXFF is not defined.
		TFT_eSPI_DisplayAdapter<MaxNofTouchListeners> tft_eSPI_DisplayAdapter;
		AsyncDisplay<CommandBufferSize, MaxNofClientTasks> asyncDisplay;

		// The background owns the whole screen; the image panel is its child.
		// That parent-child pair is what makes switching images clean: a
		// transparent image deliberately paints no background of its own, so
		// without a parent to repaint first, the previous picture would stay
		// visible around it.
		Panel<1> backgroundPanel;
		Panel<0> imagePanel;

		Text<0> captionText;

		using BUTTON = TouchscreenButton<0 /*MaxNofChildren*/, 2/*MaxButtonListenerCount*/>;
		BUTTON buttonPrev;
		BUTTON buttonNext;

		Flag flagPrevPressed;
		Flag flagNextPressed;
		Flag flagPrevReleased;
		Flag flagNextReleased;

		int indexImage;
		char strCaption[48];

		// A raised-key profile, like the one the keyboard uses: a light gloss
		// along the top and left, a dark border along the bottom and right. A
		// flat rectangle in a single colour needs a lot of contrast against the
		// background before it reads as a button at all; this reads as one even
		// when the colours are close.
		// Swapping highlight and shadow makes the very same key look pressed in,
		// which is what gives the touch its feedback - see showButton().
		static constexpr uint32_t colKey          = 0x00246BC8;
		static constexpr uint32_t colKeyHighlight = 0x005E9FE8;
		static constexpr uint32_t colKeyShadow    = 0x000C2A54;

	public:
		TestImageViewer(const char *taskName, unsigned int taskPriority, unsigned int taskCoreNumber) :
			Task(taskName, taskPriority,
				10000+sizeof(freeFonts)+sizeof(tft_eSPI_DisplayAdapter)+sizeof(asyncDisplay)+
				2*sizeof(BUTTON), taskCoreNumber),

			tft_eSPI_DisplayAdapter(freeFonts, "/TouchCalData1"/*filenameCalibration*/, false/*bRepeatCalibration*/),

			asyncDisplay("AsyncDisplay", 2 /*priority*/, 20000 + CommandBufferSize*sizeof(DisplayCommand)+
				            MaxNofClientTasks*sizeof(PrintContext) /*stackBytes*/, ARDUINO_RUNNING_CORE,
				            tft_eSPI_DisplayAdapter, 3/*rotation*/, 2/*fontTouchCalibration*/),

			backgroundPanel("background", /*locPos*/Vec2(0, 0), CoordType::Promillage,
				/*size*/Vec2(1000, 1000), /*cornerRadius*/0, CoordType::Promillage, Alignment::TopLeft,
				/*colFg*/(uint32_t)0x00101018, /*colBg*/0x00000000),

			// Fills the screen, so the image lands in the middle of it. Its colFg
			// is the same as the background: an opaque image that does not cover
			// the whole panel makes the panel paint colFg first, and that should
			// not show as a rectangle around the picture.
			imagePanel("image", /*locPos*/Vec2(0, 0), CoordType::Promillage,
				/*size*/Vec2(1000, 1000), /*cornerRadius*/0, CoordType::Promillage, Alignment::MidMid,
				/*colFg*/(uint32_t)0x00101018, /*colBg*/0x00000000, *arAllImages[0]),

			captionText(/*locPos*/Vec2(500, 30), CoordType::Promillage, Alignment::TopMid,
				strCaption, /*font*/2, /*color*/0x00FFFFFF, /*fontScale*/1),

			// fontScale=0 lets the button match the scale to its own height.
			// cornerRadius is 0 on purpose: a bevel and a radius do not combine -
			// the bevel wins, because its lines are straight.
			buttonPrev("bn prev", /*locPos*/Vec2(20, 690), CoordType::Promillage,
				/*size*/Vec2(160, 270), /*cornerRadius*/0, CoordType::Promillage,
				Alignment::MidMid, /*colPanel*/colKey, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "<", /*buttonFont*/2, /*colFont*/0x00FFFFFF, /*fontScale*/0,
				Alignment::MidMid, /*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			buttonNext("bn next", /*locPos*/Vec2(820, 690), CoordType::Promillage,
				/*size*/Vec2(160, 270), /*cornerRadius*/0, CoordType::Promillage,
				Alignment::MidMid, /*colPanel*/colKey, /*colBg*/0x00000000,
				/*bInvertedArea*/false, ">", /*buttonFont*/2, /*colFont*/0x00FFFFFF, /*fontScale*/0,
				Alignment::MidMid, /*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			flagPrevPressed(this),
			flagNextPressed(this),
			flagPrevReleased(this),
			flagNextReleased(this),
			indexImage(0)
		{
			strCaption[0] = 0;
			buttonPrev.setBevel(colKeyHighlight, colKeyShadow);
			buttonNext.setBevel(colKeyHighlight, colKeyShadow);
			start(); // For simplicity, the task is started right away in it's constructor.
		}

	public:
		/* override */ void buttonPressed(IButton* pButton)
		{
			if (pButton == &buttonPrev)
			{
				flagPrevPressed.set();
			}
			else if (pButton == &buttonNext)
			{
				flagNextPressed.set();
			}
		}

		/* override */ void buttonReleased(IButton* pButton)
		{
			if (pButton == &buttonPrev)
			{
				flagPrevReleased.set();
			}
			else if (pButton == &buttonNext)
			{
				flagNextReleased.set();
			}
		}

	private:
		// Redraws one button as either raised or pressed in. TouchscreenButton
		// itself does not change its looks on a touch, so this is what tells the
		// user that the tap arrived - which matters here, because loading the
		// next image takes long enough to wonder otherwise.
		void showButton(BUTTON& button, bool bPressed)
		{
			if (bPressed)
			{
				button.setBevel(colKeyShadow, colKeyHighlight);		// swapped: sunken
			}
			else
			{
				button.setBevel(colKeyHighlight, colKeyShadow);
			}
			button.show(false);
		}

		// Draws the image that indexImage points at, and everything that belongs
		// on top of it. The background is redrawn along with it - see the comment
		// at backgroundPanel for why that is not optional.
		void showCurrentImage()
		{
			const Image* pImage = arAllImages[indexImage];
			imagePanel.setImage(pImage);

			snprintf(strCaption, sizeof(strCaption), "%d/%d  %s  %dx%d",
				indexImage + 1, nofAllImages, arAllImageNames[indexImage],
				(int)pImage->width, (int)pImage->height);

			backgroundPanel.show(true);		// background, then the image on it

			// The buttons and the caption are drawn last, so they end up on top
			// of the image no matter how large it is.
			captionText.show(false);
			buttonPrev.show(false);
			buttonNext.show(false);

			ESP_LOGI(taskName, "%s (%dx%d, %s, spritesheet %dx%d)",
				arAllImageNames[indexImage], (int)pImage->width, (int)pImage->height,
				pImage->bHasTransparency ? "transparent" : "opaque",
				(int)pImage->nofRows, (int)pImage->nofCols);
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

			// Added in drawing order: background (with the image inside it) first,
			// caption and buttons after, so they stay on top.
			page1.addChildWidget(backgroundPanel);
			backgroundPanel.addChildWidget(imagePanel);
			page1.addChildWidget(captionText);
			page1.addChildWidget(buttonPrev);
			page1.addChildWidget(buttonNext);

			asyncDisplay.addTouchListener(&buttonPrev);
			asyncDisplay.addTouchListener(&buttonNext);
			buttonPrev.addButtonListener(this);
			buttonNext.addButtonListener(this);

			showCurrentImage();

			while (true)
			{
				waitAny(flagPrevPressed + flagNextPressed + flagPrevReleased + flagNextReleased);

				// The button is drawn pressed in first, so it is already sunken
				// while the new image is being blitted, and pops back out when
				// the finger leaves.
				if (hasFired(flagPrevPressed))
				{
					showButton(buttonPrev, true);
					indexImage = (indexImage + nofAllImages - 1) % nofAllImages;
					showCurrentImage();
				}

				if (hasFired(flagNextPressed))
				{
					showButton(buttonNext, true);
					indexImage = (indexImage + 1) % nofAllImages;
					showCurrentImage();
				}

				if (hasFired(flagPrevReleased))
				{
					showButton(buttonPrev, false);
				}

				if (hasFired(flagNextReleased))
				{
					showButton(buttonNext, false);
				}
			}
		}
	}; // end class TestImageViewer
};// end namespace crt
