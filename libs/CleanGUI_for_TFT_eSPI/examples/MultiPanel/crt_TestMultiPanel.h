// by Marius Versteegen, 2026

// Barbarians walking about on a green map. Two buttons spawn them - one a
// transparent one, one an opaque one - and a third clears the lot. The frame
// rate is in the header, so you can watch what a sprite costs.
//
// The two kinds are the same picture: images/barbarian-trcolor_FF00FF.png is
// converted with a transparent colour, images/barbarianOpaque.png without one
// (which is why the opaque one carries a black square around it). They differ in
// what it takes to move them:
//
//   opaque       the panel covers its own rect, so moveTo() blits just the
//                sprite at its new spot                          ~1,7 ms
//   transparent  more than half its pixels let the map through, so the map has
//                to be repainted underneath first, and the blit itself is paid
//                per run of visible pixels (173 of them)         ~4 ms
//
// Widget::coversOwnRect() is what makes moveTo() tell those two apart.

#pragma once
#include <Arduino.h>
#include <esp_timer.h>
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
	class TestMultiPanel : public Task, public IButtonListener
	{
		static const int CommandBufferSize = 1000;
		static const int MaxNofClientTasks = 10;
		static const int MaxNofTouchListeners = 20;

		// Twenty of each kind. The panels for all of them exist from the start;
		// spawning only makes one drawable and puts it somewhere.
		static const int MaxNofSpritesPerKind = 20;
		static const int MaxNofSprites = 2 * MaxNofSpritesPerKind;

		static const int SpriteSizePix = 64;

		// Everything in this example is in pixels rather than promillage. The
		// layout has to line up with a 64x64 sprite and with the bounds it may
		// walk between, and rounding a promillage to the nearest pixel would make
		// those two disagree by one now and then.
		static const int32_t HeaderHeightPix = 18;
		static const int32_t MapTopPix       = 20;
		static const int32_t MapHeightPix    = 172;
		static const int32_t ButtonTopPix    = 196;
		static const int32_t ButtonHeightPix = 42;
		static const int32_t ButtonWidthPix  = 100;

	private:
		TFT_eSPI_FreeFonts<20/*MaxNofFreeFonts*/> freeFonts;		// not used when LOAD_GFXFF is not defined.
		TFT_eSPI_DisplayAdapter<MaxNofTouchListeners> tft_eSPI_DisplayAdapter;
		AsyncDisplay<CommandBufferSize, MaxNofClientTasks> asyncDisplay;

		// The header has a panel of its own so that the opaque fps text derives
		// the box it blanks from one line, not from the whole page - see
		// Text::calcPaddingSizePix.
		Panel<1> headerPanel;
		Text<0> fpsText;

		Panel<MaxNofSprites> mapPanel;
		Panel<0> arSpritePanels[MaxNofSprites];

		using BUTTON = TouchscreenButton<0 /*MaxNofChildren*/, 2/*MaxButtonListenerCount*/>;
		BUTTON buttonSpawnTransparent;
		BUTTON buttonSpawnOpaque;
		BUTTON buttonClear;

		// The frame clock. A periodic timer rather than a vTaskDelay in a free
		// running loop, because the button flags have to be waited on in the same
		// breath: hasFired() only means anything right after a waitAny.
		Timer timerFrame;

		Flag flagSpawnTransparent;
		Flag flagSpawnOpaque;
		Flag flagClear;
		Flag flagButtonReleased;

		BUTTON* pButtonPressed;

		// Per sprite: where it is within the map, where it is heading, and whether
		// it is out at all. Index 0..19 are the transparent ones, 20..39 the
		// opaque ones - so the kind follows from the index and needs no field.
		struct SpriteState
		{
			Vec2 posPix;
			int32_t dxPix, dyPix;
			bool bActive;
		};
		SpriteState arSpriteStates[MaxNofSprites];

		int nofActiveTransparent;
		int nofActiveOpaque;
		int nofFpsShown;

		char strFps[40];

		static constexpr uint32_t colBackground   = 0x00101018;
		static constexpr uint32_t colMap          = 0x00206030;

		static constexpr uint32_t colKey          = 0x00246BC8;
		static constexpr uint32_t colKeyHighlight = 0x005E9FE8;
		static constexpr uint32_t colKeyShadow    = 0x000C2A54;

	public:
		TestMultiPanel(const char *taskName, unsigned int taskPriority, unsigned int taskCoreNumber) :
			Task(taskName, taskPriority,
				10000+sizeof(freeFonts)+sizeof(tft_eSPI_DisplayAdapter)+sizeof(asyncDisplay)+
				3*sizeof(BUTTON), taskCoreNumber),

			tft_eSPI_DisplayAdapter(freeFonts, "/TouchCalData1"/*filenameCalibration*/, false/*bRepeatCalibration*/),

			// 8000 stack: gemeten met dumpStackHighWaterMarkIfIncreased onder de
			// zwaarste belasting die de voorbeelden kennen (MultiPanel met 20
			// sprites, Inputfield met schuivend toetsenbord en free fonts) raakt
			// deze taak ~2,5 KB aan. Bewust geen formule met CommandBufferSize
			// erin: de commandobuffer is een FreeRTOS-queue en staat op de heap,
			// niet op deze stack, en de PrintContexts zijn leden van AsyncDisplay.
			asyncDisplay("AsyncDisplay", 2 /*priority*/, 8000 /*stackBytes*/, ARDUINO_RUNNING_CORE,
				            tft_eSPI_DisplayAdapter, 3/*rotation*/, 2/*fontTouchCalibration*/),

			headerPanel("header", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(320, HeaderHeightPix), /*cornerRadius*/0, CoordType::Pixels, Alignment::TopLeft,
				/*colFg*/(uint32_t)colBackground, /*colBg*/0x00000000),

			fpsText(/*locPos*/Vec2(6, 1), CoordType::Pixels, Alignment::TopLeft,
				strFps, /*font*/2, /*color*/0x00FFFFFF, /*fontScale*/1),

			mapPanel("map", /*locPos*/Vec2(0, MapTopPix), CoordType::Pixels,
				/*size*/Vec2(320, MapHeightPix), /*cornerRadius*/0, CoordType::Pixels, Alignment::TopLeft,
				/*colFg*/(uint32_t)colMap, /*colBg*/0x00000000),

			buttonSpawnTransparent("bn spawn transparent", /*locPos*/Vec2(4, ButtonTopPix), CoordType::Pixels,
				/*size*/Vec2(ButtonWidthPix, ButtonHeightPix), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colPanel*/colKey, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "trans+", /*buttonFont*/2, /*colFont*/0x00FFFFFF, /*fontScale*/1,
				Alignment::MidMid, /*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			buttonSpawnOpaque("bn spawn opaque", /*locPos*/Vec2(110, ButtonTopPix), CoordType::Pixels,
				/*size*/Vec2(ButtonWidthPix, ButtonHeightPix), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colPanel*/colKey, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "opaak+", /*buttonFont*/2, /*colFont*/0x00FFFFFF, /*fontScale*/1,
				Alignment::MidMid, /*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			buttonClear("bn clear", /*locPos*/Vec2(216, ButtonTopPix), CoordType::Pixels,
				/*size*/Vec2(ButtonWidthPix, ButtonHeightPix), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colPanel*/colKey, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "wis", /*buttonFont*/2, /*colFont*/0x00FFFFFF, /*fontScale*/1,
				Alignment::MidMid, /*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0),

			timerFrame(this),
			flagSpawnTransparent(this),
			flagSpawnOpaque(this),
			flagClear(this),
			flagButtonReleased(this),
			pButtonPressed(nullptr),
			nofActiveTransparent(0),
			nofActiveOpaque(0),
			nofFpsShown(0)
		{
			strFps[0] = 0;

			// One pass over its own line instead of erase-then-draw, so a shorter
			// line cannot leave the tail of a longer one behind.
			fpsText.setOpaque(colBackground);

			buttonSpawnTransparent.setBevel(colKeyHighlight, colKeyShadow);
			buttonSpawnOpaque.setBevel(colKeyHighlight, colKeyShadow);
			buttonClear.setBevel(colKeyHighlight, colKeyShadow);

			for (int i = 0; i < MaxNofSprites; i++)
			{
				arSpriteStates[i].bActive = false;
			}

			start(); // For simplicity, the task is started right away in it's constructor.
		}

	public:
		/* override */ void buttonPressed(IButton* pButton)
		{
			if (pButton == &buttonSpawnTransparent)
			{
				flagSpawnTransparent.set();
			}
			else if (pButton == &buttonSpawnOpaque)
			{
				flagSpawnOpaque.set();
			}
			else if (pButton == &buttonClear)
			{
				flagClear.set();
			}
			else
			{
				return;
			}

			pButtonPressed = static_cast<BUTTON*>(pButton);
		}

		/* override */ void buttonReleased(IButton* pButton)
		{
			flagButtonReleased.set();
		}

	private:
		// Draws one button as either raised or pressed in - TouchscreenButton does
		// not change its own looks on a touch.
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
			// show(true), niet show(false): het label van een TouchscreenButton is een
			// kind-widget (zie bTextAddedToThisPanel in crt_TouchscreenButton.h).
			// Zonder de kinderen schildert het panel over het label heen en tekent
			// niemand het terug - dan verdwijnt de tekst bij de eerste aanraking.
			button.show(true);
		}

		static bool isTransparentKind(int indexSprite)
		{
			return (indexSprite < MaxNofSpritesPerKind);
		}

		// How far a sprite may walk: the map minus its own size, so it can touch
		// the edge but never cross it. Beyond the map lie the header and the
		// buttons, and a sprite that walked over those would repaint them.
		void calcBoundsPix(int32_t& maxXPix, int32_t& maxYPix)
		{
			Vec2 mapSizePix = mapPanel.getSizePix();
			maxXPix = mapSizePix.x - SpriteSizePix;
			maxYPix = mapSizePix.y - SpriteSizePix;
		}

		// A speed of -3..3 in both directions, never standing still.
		static void randomVelocity(int32_t& dxPix, int32_t& dyPix)
		{
			do
			{
				dxPix = (int32_t)random(-3, 4);
				dyPix = (int32_t)random(-3, 4);
			}
			while ((dxPix == 0) && (dyPix == 0));
		}

		void spawn(bool bTransparent)
		{
			int indexFirst = bTransparent ? 0 : MaxNofSpritesPerKind;
			int indexSprite = -1;

			for (int i = indexFirst; i < indexFirst + MaxNofSpritesPerKind; i++)
			{
				if (!arSpriteStates[i].bActive)
				{
					indexSprite = i;
					break;
				}
			}

			if (indexSprite < 0)
			{
				ESP_LOGI(taskName, "%s barbarians: all %d of them are out already",
					bTransparent ? "transparent" : "opaque", MaxNofSpritesPerKind);
				return;
			}

			int32_t maxXPix, maxYPix;
			calcBoundsPix(maxXPix, maxYPix);

			SpriteState& state = arSpriteStates[indexSprite];
			state.posPix.set((int32_t)random(0, maxXPix + 1), (int32_t)random(0, maxYPix + 1));
			randomVelocity(state.dxPix, state.dyPix);
			state.bActive = true;

			Panel<0>& panel = arSpritePanels[indexSprite];
			panel.setLocPos(state.posPix, CoordType::Pixels, Alignment::TopLeft);
			panel.setDrawable(true, true);
			panel.show(false);		// puts it on the map straight away

			if (bTransparent) { nofActiveTransparent++; } else { nofActiveOpaque++; }
			updateFpsText(-1);
		}

		void clearAll()
		{
			for (int i = 0; i < MaxNofSprites; i++)
			{
				arSpriteStates[i].bActive = false;
				arSpritePanels[i].setDrawable(false, true);
			}

			nofActiveTransparent = 0;
			nofActiveOpaque = 0;

			// One repaint of the whole map rather than forty little erases: the
			// sprites are gone from the tree's drawing, so this fills the map and
			// puts back whatever else lives in it - which is nothing.
			mapPanel.repaintRegion(mapPanel.getGlobPosPix(), mapPanel.getSizePix());
			updateFpsText(-1);
		}

		// Moves every sprite that is out, one step. This is the whole cost of a
		// frame: nothing else on the screen changes.
		void moveSprites()
		{
			int32_t maxXPix, maxYPix;
			calcBoundsPix(maxXPix, maxYPix);

			for (int i = 0; i < MaxNofSprites; i++)
			{
				SpriteState& state = arSpriteStates[i];
				if (!state.bActive)
				{
					continue;
				}

				state.posPix.x += state.dxPix;
				state.posPix.y += state.dyPix;

				// Bounce off the edges of the map.
				if (state.posPix.x < 0)        { state.posPix.x = 0;        state.dxPix = -state.dxPix; }
				if (state.posPix.x > maxXPix)  { state.posPix.x = maxXPix;  state.dxPix = -state.dxPix; }
				if (state.posPix.y < 0)        { state.posPix.y = 0;        state.dyPix = -state.dyPix; }
				if (state.posPix.y > maxYPix)  { state.posPix.y = maxYPix;  state.dyPix = -state.dyPix; }

				// Now and then a nudge, so that it wanders instead of playing
				// billiards forever.
				if (random(0, 100) < 2)
				{
					randomVelocity(state.dxPix, state.dyPix);
				}

				arSpritePanels[i].moveTo(state.posPix, CoordType::Pixels);
			}
		}

		// nofFps < 0: keep the number that is on show and only refresh the counts.
		void updateFpsText(int nofFps)
		{
			if (nofFps >= 0)
			{
				nofFpsShown = nofFps;
			}

			snprintf(strFps, sizeof(strFps), "%d fps   trans %d   opaak %d",
				nofFpsShown, nofActiveTransparent, nofActiveOpaque);
			fpsText.redraw();
		}

		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

			// Without a seed every run would walk exactly the same paths.
			randomSeed((uint32_t)esp_timer_get_time());

#ifdef LOAD_GFXFF							  // Don't select font indices 0 and 1 - they are reserved.
			freeFonts.setFreeFont(10, FSB9);  // Free serif 9 pix font.
			freeFonts.setFreeFont(12, FSB12); // Free serif 12 pix font.
#endif
			IDisplay& display = asyncDisplay;   // To make it evident that the rest of the code is
			                                      // display independent.
			PageRoot<10> page1("root", display);

			page1.addChildWidget(headerPanel);
			headerPanel.addChildWidget(fpsText);
			page1.addChildWidget(mapPanel);
			page1.addChildWidget(buttonSpawnTransparent);
			page1.addChildWidget(buttonSpawnOpaque);
			page1.addChildWidget(buttonClear);

			// Every sprite panel is set up completely here - size, image, place in
			// the tree - and then switched off. Spawning is no more than a position
			// and setDrawable(true), so it costs nothing but the one blit.
			// Note that setPanelProps() would not do: it assigns the fields but
			// leaves the pixel sizes stale. setSize/setLocPos do recalculate.
			for (int i = 0; i < MaxNofSprites; i++)
			{
				Panel<0>& panel = arSpritePanels[i];

				mapPanel.addChildWidget(panel);
				panel.setSize(Vec2(SpriteSizePix, SpriteSizePix), CoordType::Pixels);
				panel.setLocPos(Vec2(0, 0), CoordType::Pixels, Alignment::TopLeft);
				panel.setImage(isTransparentKind(i) ? &imageBarbarian : &imageBarbarianOpaque);
				panel.setDrawable(false, true);
			}

			BUTTON* arButtons[] = { &buttonSpawnTransparent, &buttonSpawnOpaque, &buttonClear };
			for (BUTTON* pButton : arButtons)
			{
				asyncDisplay.addTouchListener(pButton);
				pButton->addButtonListener(this);
			}

			snprintf(strFps, sizeof(strFps), "0 fps   trans 0   opaak 0");

			// The one moment the whole screen is painted. After this only the
			// sprites move, and each of those repaints its own rectangle.
			page1.show(true);

			int nofFrames = 0;
			int64_t timeLastReportUs = esp_timer_get_time();

			// A tick every 10 ms, so an empty map does not spin at full speed. Once
			// there are enough sprites out that a round takes longer than that, the
			// tick is already waiting when the round ends and the loop simply runs
			// as fast as it can - which is exactly what the fps counter is there to
			// show.
			timerFrame.start_periodic(10000);

			while (true)
			{
				waitAny(timerFrame + flagSpawnTransparent + flagSpawnOpaque +
				        flagClear + flagButtonReleased);

				if (hasFired(timerFrame))
				{
					moveSprites();
					nofFrames++;

					int64_t timeNowUs = esp_timer_get_time();
					if ((timeNowUs - timeLastReportUs) >= 1000000)
					{
						// Counted over the interval that really elapsed, not over an
						// assumed second: with forty sprites out, one round can
						// overshoot by a lot.
						int nofFps = (int)((int64_t)nofFrames * 1000000LL / (timeNowUs - timeLastReportUs));
						updateFpsText(nofFps);

						ESP_LOGI(taskName, "%d fps, %d transparent + %d opaque sprites, %d commands queued",
							nofFps, nofActiveTransparent, nofActiveOpaque,
							(int)asyncDisplay.getNofQueuedCommands());

						nofFrames = 0;
						timeLastReportUs = timeNowUs;
					}
				}

				if (hasFired(flagSpawnTransparent))
				{
					showButton(buttonSpawnTransparent, true);
					spawn(true);
				}

				if (hasFired(flagSpawnOpaque))
				{
					showButton(buttonSpawnOpaque, true);
					spawn(false);
				}

				if (hasFired(flagClear))
				{
					showButton(buttonClear, true);
					clearAll();
				}

				if (hasFired(flagButtonReleased) && (pButtonPressed != nullptr))
				{
					showButton(*pButtonPressed, false);
				}
			}
		}
	}; // end class TestMultiPanel
};// end namespace crt
