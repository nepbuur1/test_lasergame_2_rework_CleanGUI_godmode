// by Marius Versteegen, 2024

#pragma once
#include <assert.h>
#include <crt_FreeRTOS.h>
#include "crt_typesAndConstants.h"
#include "crt_IDisplay.h"
#include "crt_Widget.h"
#include "crt_ITouchListener.h"
#include <crt_IButton.h>

namespace crt
{
	extern ILogger& logger;

	template <size_t MaxNofChildren, size_t MaxButtonListenerCount>
	class TouchscreenButton : public Panel<MaxNofChildren+1>, public IButton, public ITouchListener
	{
	public:
		using PBase = Panel<MaxNofChildren+1>;

		Text<0> text;

		bool bTextAddedToThisPanel;
		bool bPressed;
		bool bInvertedArea;	        // if true, the button registers a touch when the touch is outside the button.

		IButtonListener* arButtonListeners[MaxButtonListenerCount] = {};
		uint16_t nofButtonListeners;
		int32_t lowerCaseOffsetY;	// Promillage of vertical button size to give text y offset if it is all lowercase, to center it on the button. 
		                            // It is dependent on the font.
		int32_t upperCaseOffsetY;   // Also the other characters may need an offset, because the fontheight takes into account underscoring as well.

		// fontScale=0 can be used to activate automatic font scaling.
		TouchscreenButton(const char* buttonName, const Vec2& locPos, CoordType coordTypeLocPos,
			  const Vec2& size, int32_t cornerRadius, CoordType coordTypeSize, 
			  Alignment alignment, uint32_t colPanel, uint32_t colBg, bool bInvertedArea,
			  const char* str, uint8_t font, uint32_t colFont, uint8_t fontScale, Alignment fontAlignment, int32_t lowerCaseOffsetY, int32_t upperCaseOffsetY) :
			  PBase(buttonName, locPos, coordTypeLocPos, size, cornerRadius, coordTypeSize, alignment, colPanel, colBg),
			  text(Vec2(0,0), CoordType::Promillage, fontAlignment, str, font, colFont, fontScale),
			  bTextAddedToThisPanel(false), bPressed(false), bInvertedArea(bInvertedArea), nofButtonListeners(0), lowerCaseOffsetY(lowerCaseOffsetY), upperCaseOffsetY(upperCaseOffsetY)
		{
			applyOffsetForLowerCaseText();
		}

		TouchscreenButton() : TouchscreenButton("", Vec2(0,0), CoordType::Pixels, Vec2(0,0), 0, CoordType::Pixels, Alignment::TopLeft, 0, 0, false, "", 0, 0, 0, Alignment::TopLeft, 0, 0)
		{
		}

		// convenience function to set most properties at once.
		void setTouchScreenButtonProps(const char* buttonName, const Vec2& locPos, CoordType coordTypeLocPos,
			  const Vec2& size, int32_t cornerRadius, CoordType coordTypeSize, 
			  Alignment alignment, uint32_t colPanel, uint32_t colBg, bool bInvertedArea,
			  const char* str, uint8_t font, uint32_t colFont, uint8_t fontScale, Alignment fontAlignment, int32_t lowerCaseOffsetY, int32_t upperCaseOffsetY)
		{
			PBase::setPanelProps(buttonName, locPos, coordTypeLocPos, size, cornerRadius, coordTypeSize, alignment, colPanel, colBg);
			text.setTextProps(Vec2(0,0), CoordType::Promillage, fontAlignment, str, font, colFont, fontScale);
			this->lowerCaseOffsetY=lowerCaseOffsetY;
			this->upperCaseOffsetY=upperCaseOffsetY;
			applyOffsetForLowerCaseText();
		}

		void applyOffsetForLowerCaseText()
		{
			const char* str = text.getText();
			if(*str != '\0')
			{
				// If the text is all lowercase, we can move it up a bit to center it on the button.
				// This is a heuristic, and may not work for all fonts.
				int32_t offsetY = (isAllLowercaseLetters(str, 1000)) ? lowerCaseOffsetY : upperCaseOffsetY;
					
				text.setLocPos(Vec2(0,offsetY),text.getCoordTypeLocPos(),text.getAlignment()); // move the text up a bit, to center it on the button.
			}
		}

		void setText(const char* str)
		{
			text.setText(str);
			applyOffsetForLowerCaseText();
		}

		/* override */ WidgetType getWidgetType()
		{
			return WidgetType::TouchscreenButton;
		}

		// Note: The function below is called in Widget.addChildWidget after the display has been set.
		/* override */  void setSizeOfParent(const Vec2& sizeOfParent)
		{
			IDisplay* pDisplay = this->getDisplay();
			assert(pDisplay != nullptr);

			// pass on the call to our base / the panel.
			PBase::setSizeOfParent(sizeOfParent);

			// Now we can obtain the size of the panel, even if CoordType is CoordType::Promillage
			// (because in that case, the size depends on the parent size)

			// From that size, we can determine a strategy for sizing and positioning our text.

			if (!bTextAddedToThisPanel)
			{
				// As soon as the display is known (normally due to an addToChild call of the parent),
				// it is okay to add children to this Widget.

				//ESP_LOGI("TouchscreenButton calling addChildWidget from setSizeOfParent:", "%s", this->getName());
				this->addChildWidget(text);

				bTextAddedToThisPanel = true;

				uint64_t fontHeight = (uint64_t)(pDisplay->getFontHeight(text.getFont()));
				if (text.getFontScale() == 0)
				{
					// Automatically select a suitable font scale
					Vec2 panelSize = this->getSizePix();

					if (fontHeight > 0)
					{
						uint64_t fontScale = 1;	// start at 1.
						uint64_t largestApplicableFontScale = 1;	// start at 1.
						uint64_t panelSizeY = (uint64_t)panelSize.y;

						//ESP_LOGI("panelSizeY", "%d", (uint32_t)panelSizeY);
						//ESP_LOGI("fontHeight", "%d", (uint32_t)fontHeight);

						uint64_t factorPctY = 100000;
						while (factorPctY > 110)
						{
							largestApplicableFontScale = fontScale;

							fontScale++;
							factorPctY = 100 * panelSizeY / (fontScale * fontHeight);
							//ESP_LOGI("fontScale", "%d", (uint32_t)fontScale);
							//ESP_LOGI("factorPctY", "%d", (uint32_t)factorPctY);
						}

						text.setFontScale((uint8_t)largestApplicableFontScale);
					}
					else
					{
						ESP_LOGI("zero font height?", "div by zero avoided");
					}
				}
				Alignment curTextAlignment = text.getAlignment();
				switch (curTextAlignment)
				{
				case Alignment::MidLeft:
					text.setLocPos(Vec2(fontHeight, 0), CoordType::Pixels, curTextAlignment);
					break;
				case Alignment::MidMid:
					// no action required.
					break;
				default:
					ESP_LOGI("buttonTextAlignments", "only MidLeft and MidMid are currently supported");
					break;
				}
			}
		}

		/*override keyword not supported in current compiler*/
		void addButtonListener(IButtonListener* pButtonListener)
		{
			if (!isAlreadyPresent(pButtonListener))
			{
				assert(nofButtonListeners < MaxButtonListenerCount);
				arButtonListeners[nofButtonListeners++] = pButtonListener;
			}
		}

		/* override */ void touchPressed(const Vec2& pos)
		{
			//ESP_LOGI("tsb touchpressed first", "%s", this->getName());
			//vTaskDelay(200);
			///logger.logText("touchPressed");

			if (!PBase::isEnabled())
			{
				return;
			}

			//logger.logText("touchPressed_2");
			// if bPressed, nothing changes.
			if (!bPressed)
			{
				//logger.logText("touchPressed_3");
				if (isPosInButton(pos))
				{
					//ESP_LOGI("tsb touchpressed second", "%s",this->getName());
					//vTaskDelay(200);

					//logger.logText("BnPressed");
					bPressed = true;
					for (int i = 0; i < nofButtonListeners; i++)
					{
						arButtonListeners[i]->buttonPressed(this);
					}
				}
			}
		}

		/* override */ void touchReleased(const Vec2& pos)
		{
			if (!PBase::isEnabled())
			{
				return;
			}

			// if !bPressed, nothing changes.
			if (bPressed)
			{
				bPressed = false;
				//if (isPosInButton(pos))
				//{
					for (int i = 0; i < nofButtonListeners; i++)
					{
						arButtonListeners[i]->buttonReleased(this);
					}
				//}
			}
		}

		/* override */ bool isPressed()
		{
			return bPressed;
		}

		/*override*/ void setButtonName(const char* str)
		{
			PBase::setName(str);
		}

		/*override*/ const char* getButtonName()
		{
			return PBase::getName();
		}

		/* override */ void enable(bool bIncludeChildren)
		{
			bPressed = false; // Clear any previously stored press state.
			PBase::enable(bIncludeChildren);
		}

	private:
		bool isAllLowercaseLetters(const char* str, int maxAmountCheck) {
			const char* pChar = str;
			size_t i = 0;
			for (i = 0; i < maxAmountCheck; i++)
			{
				if(*pChar != '\0') {
					unsigned char c = static_cast<unsigned char>(*pChar);
					if (!islower(c) || !isalpha(c)) {
						//ESP_LOGI("Not all lowerCase", "%s", str);
						return false;
					}
					++pChar;
				}
				else
				{
					break;
				}
			}
			if(i>=maxAmountCheck) {
				// Should not happen. If it does, increase maxAmountCheck or check if string is null-terminated.
				// show in ESP_LOG: the first 10 letters of the string, i and amountcheck:
				ESP_LOGI("maxAmountcheck", "%d", maxAmountCheck);
				ESP_LOGI("i", "%d", i);
				
				return false;
			}
			
			//ESP_LOGI("AllLowerCase", "%s", str);

			return true;
		}

		bool isPosInBounds(const Vec2& origin, const Vec2& size, const Vec2& pos)
		{
			bool bIsInBounds = (origin.x <= pos.x) && (pos.x <= (origin.x + size.x)) &&
								(origin.y <= pos.y) && (pos.y <= (origin.y + size.y));
			return bIsInBounds;
		}

		bool isPosInButton(const Vec2& pos)
		{
			Vec2 globPosPix = this->getGlobPosPix();
			Vec2 sizePix = this->getSizePix();
			return (isPosInBounds(globPosPix, sizePix, pos) != bInvertedArea);
		}

		bool isAlreadyPresent(IButtonListener* pButtonListener)
		{
			for (int i = 0; i < nofButtonListeners; i++)
			{
				//ESP_LOGI("adasdffasfsadf", "%s", i);
				if (arButtonListeners[i] == pButtonListener)
				{
					return true;
				}
			}
			return false;
		}
	};
};
