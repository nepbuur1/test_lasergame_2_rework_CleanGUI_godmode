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
#include <string.h>
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

		// Only of use while a spritesheet is on show: a cross of four small
		// buttons that walks the frames. They are hidden and disabled for a
		// picture that is not a sheet.
		BUTTON buttonRowMinus, buttonRowPlus;
		BUTTON buttonColMinus, buttonColPlus;

		Flag flagPrevPressed;
		Flag flagNextPressed;
		Flag flagPrevReleased;
		Flag flagNextReleased;
		Flag flagFramePressed;
		Flag flagFrameReleased;

		int indexImage;
		char strCaption[64];

		// Which frame the cross is pointing at, and whether it has been used at
		// all for the sheet currently on show. Before the first press the whole
		// sheet is on display; the first press of any of the four lands on
		// frame 0,0 rather than stepping away from it.
		int32_t frameRow, frameCol;
		bool bFrameSelected;

		// Set by buttonPressed(), read by the task loop: which of the four was
		// hit, and what it should do to row and column.
		int32_t stepRow, stepCol;
		BUTTON* pButtonPressed;

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

			// Left aligned, and not centered: the line is longer than the 320
			// pixels are wide, and centering lets it run off on both sides at
			// once. Against the left edge, only the tail can clip - which is why
			// the name is at the end of it.
			captionText(/*locPos*/Vec2(20, 30), CoordType::Promillage, Alignment::TopLeft,
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

			// The cross, below the image and between the two big buttons. The
			// label is small enough that a fixed fontScale of 1 fits; automatic
			// scaling would size it to the button height and run over the sides.
			buttonRowMinus("bn row-", /*locPos*/Vec2(425, 658), CoordType::Promillage,
				/*size*/Vec2(150, 100), /*cornerRadius*/0, CoordType::Promillage,
				Alignment::MidMid, /*colPanel*/colKey, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "r-", /*buttonFont*/2, /*colFont*/0x00FFFFFF, /*fontScale*/1,
				Alignment::MidMid, /*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			buttonRowPlus("bn row+", /*locPos*/Vec2(425, 879), CoordType::Promillage,
				/*size*/Vec2(150, 100), /*cornerRadius*/0, CoordType::Promillage,
				Alignment::MidMid, /*colPanel*/colKey, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "r+", /*buttonFont*/2, /*colFont*/0x00FFFFFF, /*fontScale*/1,
				Alignment::MidMid, /*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			buttonColMinus("bn col-", /*locPos*/Vec2(281, 771), CoordType::Promillage,
				/*size*/Vec2(150, 100), /*cornerRadius*/0, CoordType::Promillage,
				Alignment::MidMid, /*colPanel*/colKey, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "c-", /*buttonFont*/2, /*colFont*/0x00FFFFFF, /*fontScale*/1,
				Alignment::MidMid, /*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			buttonColPlus("bn col+", /*locPos*/Vec2(569, 771), CoordType::Promillage,
				/*size*/Vec2(150, 100), /*cornerRadius*/0, CoordType::Promillage,
				Alignment::MidMid, /*colPanel*/colKey, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "c+", /*buttonFont*/2, /*colFont*/0x00FFFFFF, /*fontScale*/1,
				Alignment::MidMid, /*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			flagPrevPressed(this),
			flagNextPressed(this),
			flagPrevReleased(this),
			flagNextReleased(this),
			flagFramePressed(this),
			flagFrameReleased(this),
			indexImage(0),
			frameRow(0), frameCol(0), bFrameSelected(false),
			stepRow(0), stepCol(0), pButtonPressed(nullptr)
		{
			strCaption[0] = 0;
			buttonPrev.setBevel(colKeyHighlight, colKeyShadow);
			buttonNext.setBevel(colKeyHighlight, colKeyShadow);
			buttonRowMinus.setBevel(colKeyHighlight, colKeyShadow);
			buttonRowPlus.setBevel(colKeyHighlight, colKeyShadow);
			buttonColMinus.setBevel(colKeyHighlight, colKeyShadow);
			buttonColPlus.setBevel(colKeyHighlight, colKeyShadow);
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
			else
			{
				// One flag for all four of the cross, with the step alongside it.
				// Two of them cannot be pressed within the same handling round -
				// that would take two fingers and a few milliseconds - so a plain
				// pair of members does here what a Pool would do in earnest code.
				if (pButton == &buttonRowMinus)     { stepRow = -1; stepCol =  0; }
				else if (pButton == &buttonRowPlus) { stepRow = +1; stepCol =  0; }
				else if (pButton == &buttonColMinus){ stepRow =  0; stepCol = -1; }
				else if (pButton == &buttonColPlus) { stepRow =  0; stepCol = +1; }
				else                                { return; }

				pButtonPressed = static_cast<BUTTON*>(pButton);
				flagFramePressed.set();
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
			else
			{
				flagFrameReleased.set();
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

		static bool isSpritesheet(const Image* pImage)
		{
			return (pImage->nofRows > 1) || (pImage->nofCols > 1);
		}

		// The cross only has meaning for a spritesheet. Disabling as well as not
		// drawing it: a button that is invisible but still listening would swallow
		// touches meant for whatever is drawn there instead.
		void enableFrameButtons(bool bEnable)
		{
			// Mind the parameter of enable/disable: it is bIncludeChildren, not
			// the state. These buttons have no children, so it is false.
			BUTTON* arButtons[] = { &buttonRowMinus, &buttonRowPlus, &buttonColMinus, &buttonColPlus };

			for (BUTTON* pButton : arButtons)
			{
				if (bEnable)
				{
					pButton->enable(false);
				}
				else
				{
					pButton->disable(false);
				}
			}
		}

		// Draws the image that indexImage points at, and everything that belongs
		// on top of it. The background is redrawn along with it - see the comment
		// at backgroundPanel for why that is not optional.
		void showCurrentImage()
		{
			const Image* pImage = arAllImages[indexImage];

			// setImage drops any frame selection, so the frame is set after it.
			imagePanel.setImage(pImage);
			if (bFrameSelected)
			{
				imagePanel.setImageFrame(frameRow, frameCol);
			}

			int32_t widthPix  = pImage->width;
			int32_t heightPix = pImage->height;

			// The generated names all start with "image", which says nothing here
			// and costs a quarter of the line: the caption is one line of font 2
			// on a 320 pixel screen, so roughly 35 characters before it clips.
			const char* strName = arAllImageNames[indexImage];
			if (strncmp(strName, "image", 5) == 0)
			{
				strName += 5;
			}

			if (bFrameSelected)
			{
				widthPix  /= pImage->nofCols;
				heightPix /= pImage->nofRows;

				// (column,row) - x before y, as in a Vec2. The sheet layout is
				// left out here: with the frame on show and the cross under it,
				// where you are says more than how big the sheet is.
				snprintf(strCaption, sizeof(strCaption), "%d/%d (%d,%d) %dx%d %s",
					indexImage + 1, nofAllImages, (int)frameCol, (int)frameRow,
					(int)widthPix, (int)heightPix, strName);
			}
			else if (isSpritesheet(pImage))
			{
				// nofRows x nofCols, in the same order as the -ssh_RxC parameter
				// that put them there.
				snprintf(strCaption, sizeof(strCaption), "%d/%d [%dx%d] %dx%d %s",
					indexImage + 1, nofAllImages, (int)pImage->nofRows, (int)pImage->nofCols,
					(int)widthPix, (int)heightPix, strName);
			}
			else
			{
				snprintf(strCaption, sizeof(strCaption), "%d/%d %dx%d %s",
					indexImage + 1, nofAllImages, (int)widthPix, (int)heightPix, strName);
			}

			backgroundPanel.show(true);		// background, then the image on it

			// The buttons and the caption are drawn last, so they end up on top
			// of the image no matter how large it is.
			captionText.show(false);
			buttonPrev.show(false);
			buttonNext.show(false);

			if (isSpritesheet(pImage))
			{
				buttonRowMinus.show(false);
				buttonRowPlus.show(false);
				buttonColMinus.show(false);
				buttonColPlus.show(false);
			}

			ESP_LOGI(taskName, "%s (%dx%d, %s, spritesheet %dx%d, frame %s)",
				arAllImageNames[indexImage], (int)pImage->width, (int)pImage->height,
				pImage->bHasTransparency ? "transparent" : "opaque",
				(int)pImage->nofRows, (int)pImage->nofCols,
				bFrameSelected ? "selected" : "whole sheet");
		}

		// Moves to another image: the frame selection belongs to the sheet that
		// was on show, so it goes.
		void stepImage(int step)
		{
			indexImage = (indexImage + nofAllImages + step) % nofAllImages;
			bFrameSelected = false;
			frameRow = 0;
			frameCol = 0;

			enableFrameButtons(isSpritesheet(arAllImages[indexImage]));
			showCurrentImage();
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
			page1.addChildWidget(buttonRowMinus);
			page1.addChildWidget(buttonRowPlus);
			page1.addChildWidget(buttonColMinus);
			page1.addChildWidget(buttonColPlus);

			BUTTON* arButtons[] = { &buttonPrev, &buttonNext,
			                        &buttonRowMinus, &buttonRowPlus, &buttonColMinus, &buttonColPlus };
			for (BUTTON* pButton : arButtons)
			{
				asyncDisplay.addTouchListener(pButton);
				pButton->addButtonListener(this);
			}

			enableFrameButtons(isSpritesheet(arAllImages[indexImage]));
			showCurrentImage();

			while (true)
			{
				waitAny(flagPrevPressed + flagNextPressed + flagPrevReleased + flagNextReleased +
				        flagFramePressed + flagFrameReleased);

				// The button is drawn pressed in first, so it is already sunken
				// while the new image is being blitted, and pops back out when
				// the finger leaves.
				if (hasFired(flagPrevPressed))
				{
					showButton(buttonPrev, true);
					stepImage(-1);
				}

				if (hasFired(flagNextPressed))
				{
					showButton(buttonNext, true);
					stepImage(+1);
				}

				if (hasFired(flagFramePressed))
				{
					if (pButtonPressed != nullptr)
					{
						showButton(*pButtonPressed, true);
					}

					// The first press of any of the four lands on frame 0,0 - it
					// selects a frame rather than stepping away from one. Only
					// after that does the cross start walking, wrapping around
					// the edges of the sheet.
					const Image* pImage = arAllImages[indexImage];

					if (bFrameSelected)
					{
						frameRow = (frameRow + stepRow + pImage->nofRows) % pImage->nofRows;
						frameCol = (frameCol + stepCol + pImage->nofCols) % pImage->nofCols;
					}
					else
					{
						frameRow = 0;
						frameCol = 0;
						bFrameSelected = true;
					}

					// The held button keeps its sunken look right through this:
					// showButton swapped its bevel colours, and showCurrentImage
					// redraws it with whatever bevel it has.
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

				if (hasFired(flagFrameReleased) && (pButtonPressed != nullptr))
				{
					showButton(*pButtonPressed, false);
				}
			}
		}
	}; // end class TestImageViewer
};// end namespace crt
