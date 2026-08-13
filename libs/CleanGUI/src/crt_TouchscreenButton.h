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
		bool bOpaqueText;			// see setOpaqueText()
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
			  const char* str, uint8_t font, uint32_t colFont, uint8_t fontScale, Alignment fontAlignment, int32_t lowerCaseOffsetY, int32_t upperCaseOffsetY,
			  bool bDrawable = true) :
			  PBase(buttonName, locPos, coordTypeLocPos, size, cornerRadius, coordTypeSize, alignment, colPanel, colBg, bDrawable),
			  text(Vec2(0,0), CoordType::Promillage, fontAlignment, str, font, colFont, fontScale, bDrawable),
			  bTextAddedToThisPanel(false), bOpaqueText(false), bPressed(false), bInvertedArea(bInvertedArea), nofButtonListeners(0), lowerCaseOffsetY(lowerCaseOffsetY), upperCaseOffsetY(upperCaseOffsetY)
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
					
				// Note: this also resets the x to 0, undoing the indent that
				// setSizeOfParent gives a MidLeft label. That indent is fontHeight -
				// a vertical metric used as a horizontal margin - which is wider
				// than a character, so flush left looks better. Left as it was.
				text.setLocPos(Vec2(0,offsetY),text.getCoordTypeLocPos(),text.getAlignment()); // move the text up a bit, to center it on the button.
			}
		}

		void setText(const char* str)
		{
			text.setText(str);
			applyOffsetForLowerCaseText();
		}

		// Makes the label draw itself opaquely against this button's own panel
		// colour, reserving the button width so that a longer previous label is
		// blanked in the same pass. That is what makes updateText() flicker-free.
		// The panel size is not known at construction time, so the request is
		// remembered and applied as soon as setSizeOfParent has run.
		void setOpaqueText(bool bOpaque)
		{
			bOpaqueText = bOpaque;
			applyOpaqueTextIfPossible();
		}

		// Replaces the label WITHOUT repainting the panel underneath.
		// That is the whole point: redrawing the button first fills the panel -
		// wiping the old text - and only then draws the new one, which is exactly
		// the gap you see as flicker. Requires setOpaqueText(true), otherwise the
		// previous label is still on screen and the two overlap.
		void updateText(const char* str)
		{
			setText(str);
			text.redraw(false);		// alleen het label, niet de knop eronder
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

					uint64_t textWidth = (uint64_t)(pDisplay->getTextWidth(text.getText(), text.getFont()));
					if (fontHeight > 0)
					{
						uint64_t fontScale = 1;	// start at 1.
						uint64_t largestApplicableFontScale = 1;	// start at 1.
						uint64_t panelSizeY = (uint64_t)panelSize.y;
						uint64_t panelSizeX = (uint64_t)panelSize.x;

						//ESP_LOGI("panelSizeY", "%d", (uint32_t)panelSizeY);
						//ESP_LOGI("fontHeight", "%d", (uint32_t)fontHeight);

						// Twee eisen, allebei met dezelfde marge van 10%: het datum
						// moet in de hoogte van de knop passen en in de breedte.
						// Alleen op hoogte kijken laat een lang label gewoon links
						// en rechts van de knop af lopen - een knop is immers vaak
						// veel breder dan hoog, dus de hoogte laat een schaal toe
						// die de breedte nooit haalt.
						uint64_t factorPctY = 100000;
						uint64_t factorPctX = 100000;
						while ((factorPctY > 110) && (factorPctX > 110))
						{
							largestApplicableFontScale = fontScale;

							fontScale++;
							factorPctY = 100 * panelSizeY / (fontScale * fontHeight);
							factorPctX = (textWidth > 0) ? (100 * panelSizeX / (fontScale * textWidth))
							                             : 100000;	// leeg label: breedte legt niets op.
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

			// The panel size (and the text's position within it) are known now, so
			// a pending setOpaqueText request can finally be honoured.
			applyOpaqueTextIfPossible();
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

		// Een knop die wordt uitgeschakeld terwijl er een vinger op ligt, mag niet
		// ingedrukt blijven staan. touchReleased() keert bij een uitgeschakelde
		// knop meteen terug, dus bPressed blijft dan hangen - en bij de volgende
		// enable() ziet touchPressed() een knop die al ingedrukt is en meldt nooit
		// meer een buttonPressed(). De knop is dan dood.
		//
		// Dat is precies wat er gebeurt bij een knop die zijn eigen pagina wegzet:
		// druk -> listener verbergt en disabelt de pagina -> de vinger gaat los in
		// het niets. Widget::hide() disabelt sinds de splitsing van de assen niet
		// meer mee, dus wie een pagina verbergt moet er zelf een disable() bij
		// zetten - en dan komt hij hier langs.
		/*override*/ void disable(bool bIncludeChildren)
		{
			bPressed = false;
			PBase::disable(bIncludeChildren);
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
			// Alleen bij een echte overgang van uit naar aan. De bedoeling is een
			// verouderde druk weggooien van een knop die ondertussen uit stond -
			// niet een druk wissen die op dit moment loopt.
			//
			// Dat laatste gebeurde wel, want Widget::show() riep enable(false)
			// aan. Wie de knop ingedrukt tekende, deed dus tegelijk alsof hij
			// nooit was ingedrukt: touchReleased() zag bPressed == false en
			// stuurde geen bericht meer, waarna de knop voorgoed verzonken bleef
			// staan. Sinds show() en enable() losse assen zijn kan tekenen dit
			// niet meer veroorzaken; de test hieronder blijft staan voor het
			// geval iemand wél expliciet enablet terwijl er een vinger op ligt.
			if (!PBase::isEnabled())
			{
				bPressed = false;
			}
			PBase::enable(bIncludeChildren);
		}

	private:
		// The reserved width runs from the text's own origin to the right edge of
		// the panel, so that the blanking never paints outside the button.
		void applyOpaqueTextIfPossible()
		{
			if (!bOpaqueText)
			{
				text.setTransparent();
				return;
			}

			Vec2 panelSizePix = PBase::getSizePix();
			if (panelSizePix.x <= 0)
			{
				return;		// size not known yet - setSizeOfParent will call us again.
			}

			// Text derives the reserved area from its parent's rect at draw time,
			// so it stays correct when the label's position shifts afterwards
			// (applyOffsetForLowerCaseText does exactly that).
			text.setOpaque(PBase::getColFg(), PBase::getBevelInsetPix());

			// Opaque text paints a background behind its glyph cell, and that cell
			// is kept inside the button. If the font is taller than the button,
			// descenders get clipped. Transparent text hid this - it simply drew
			// outside the button - so say it out loud rather than clip silently.
			int32_t fontHeightPix = PBase::getDisplay()->getFontHeight(text.getFont()) * text.getFontScale();
			if (fontHeightPix > panelSizePix.y)
			{
				ESP_LOGI("TouchscreenButton", "%s: font is %d pix high but the button only %d - descenders will be clipped. Use a smaller font or a taller button.",
					this->getName(), (int)fontHeightPix, (int)panelSizePix.y);
			}
		}

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
