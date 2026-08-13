// by Marius Versteegen, 2024

#pragma once
#include <assert.h>
#include <crt_FreeRTOS.h>
#include "crt_typesAndConstants.h"
#include "crt_IDisplay.h"
#include "crt_Widget.h"
#include "crt_ITouchListener.h"
#include "crt_TouchscreenButtonGroup.h" 
#include <crt_IButton.h>
#include <crt_IInputField.h>
#include <crt_IInputFieldListener.h>
#include <crt_string.h>
#include <crt_IKeyboard.h>

namespace crt
{
	enum InputfieldType {TextField,IntField,FloatField};

	using INPUTFIELD_TOUCHSCREENBUTTONGROUP = TouchscreenButtonGroup<5/*MaxNofChildren*/>;

	template <size_t MaxNofChildren, size_t MaxnofInputfieldListeners, size_t MaxTextSize>
	class Inputfield : public INPUTFIELD_TOUCHSCREENBUTTONGROUP,
		public IInputfield, public IButtonListener, public IKeyboardListener
	{
	private:

		Pool<std::string<MaxTextSize>> poolFieldContent;	// For exchanging the content of the inputfield with the outside world.
		std::string<MaxTextSize> _strFieldContent;	 		// The content of the inputfield. Optimization: Rather than copying the entire string to the
															// pool after every character, it is done only when required. May be worthwhile for large textblocks.

		IInputfieldListener* arInputfieldListener[MaxnofInputfieldListeners] = {};
		uint16_t nofInputfieldListeners;

		bool bChildWidgetsAdded;
		InputfieldType inputfieldType;

		const int keyCornerRadiusPromillage=50;
		const int buttonFont = 12;

		// Verzonken rand. Lichter dan het veld aan de onder- en rechterkant, want
		// daar valt het licht op de binnenwand; donker rondom als de kier.
		const uint32_t colFieldHighlight = 0x00B8B8B8;
		const uint32_t colFieldShadow    = 0x00303030;

		//TouchscreenButton<0 /*MaxNofChildren*/, 1/*MaxButtonListenerCount*/> tsButtonA;
		using TSButton = TouchscreenButton<0, 1>;
		
		TSButton tsOutsideInputfieldButton; // This button is used to register a touch outside the input field.
		TSButton tsInputfieldButton;		// This button is used to display the input field, and can be selected.

	public:
		using Base = INPUTFIELD_TOUCHSCREENBUTTONGROUP;
		using WBase = Widget<MaxNofChildren + 10>;

			/*tsButtonB, tsButtonC, tsButtonD, tsButtonD, tsButtonE,
			tsButtonF, tsButtonG, tsButtonI, tsButtonJ, tsButtonK, tsButtonL, tsButtonM,
			tsButtonN, tsButtonO, tsButtonP, tsButtonQ, tsButtonR, tsButtonS, tsButtonT,
			tsButtonU, tsButtonV, tsButtonW, tsButtonX, tsButtonY, tsButtonZ, tsButtonShiftToUpper,
			tsButtonToggleNumeric, tsButtonSlashForward, tsButtonBackSpace, tsButtonDot, tsButtonEnter*/

		Inputfield(IKeyboard* pKeyboard, const InputfieldType& inputfieldType, const char* strName, const Vec2& locPos, CoordType coordTypeLocPos,
			  const Vec2& size, /*int32_t cornerRadius,*/ CoordType coordTypeSize, 
			  Alignment alignment, uint32_t colPanel, uint32_t colBg, uint32_t colFont, InputfieldType initialInputfieldType) :
			Base::TouchscreenButtonGroup(strName, locPos, coordTypeLocPos, size, /*cornerRadius,*/ coordTypeSize, alignment),
			nofInputfieldListeners(0),
			bChildWidgetsAdded(false),
			inputfieldType(inputfieldType) /* will be updated at the end of the constructor. */,
			tsOutsideInputfieldButton("tsOutsideInputfieldButton", Vec2(0,0), CoordType::Promillage, Vec2(1000,1000), 0, CoordType::Promillage, Alignment::TopLeft, colBg, colBg, true /*use inverted area for touch detection*/, "", 0, 0, 0, Alignment::TopLeft, 0, 0, /*drawable*/false),
			tsInputfieldButton("tsInputfieldButton", Vec2(0,0), CoordType::Promillage, Vec2(1000,1000), 0, CoordType::Promillage, Alignment::TopLeft, colPanel, colBg, false, "", 12, colFont, 0, Alignment::MidLeft, 0, 0)
		{
			pKeyboard->addKeyboardListener(this);
			tsOutsideInputfieldButton.addButtonListener(this);
			tsInputfieldButton.addButtonListener(this);	
		}

	private:
		void addChildWidgets()
		{
			ESP_LOGI("Inputfield", "will Add ChildWidgets");
			Base::addTouchListener(tsOutsideInputfieldButton, tsOutsideInputfieldButton);
			Base::addTouchListener(tsInputfieldButton, tsInputfieldButton);

			// Sunken instead of raised: the gloss sits along the bottom and right,
			// so the field reads as a recess in the screen rather than a key
			// standing on it. Counterpart of the keyboard, whose keys stand out.
			tsInputfieldButton.setBevel(colFieldHighlight, colFieldShadow, BevelStyle::Sunken);

			// The field content changes on every keystroke, so let it paint its
			// own background: that is what allows updateButtonLabel to replace
			// the text without erasing the field first (see there).
			// The button size is not known yet at this point; the request is
			// applied as soon as it is.
			tsInputfieldButton.setOpaqueText(true);

			bChildWidgetsAdded = true;
		}

		void updateButtonLabel(TSButton& tsButton, const char* label)
		{
			tsButton.setButtonName(label); // because IButton does not have a setText function.

			// Only the label is redrawn; the panel underneath is deliberately NOT
			// repainted. Hier stond Base::show(true), en dat was de flikker: die
			// vulde eerst het hele veld met colPanel - waardoor de tekst weg is -
			// en tekent hem daarna pas opnieuw. Gemeten was dat veld daardoor 2,9
			// tot 3,9 ms leeg per toetsaanslag (fillRect 2,57 ms + drawString).
			// Het label is opaak (zie addChildWidgets), dus het schildert zijn
			// eigen achtergrond en wist in diezelfde pass de staart van een
			// langere vorige tekst. Er is dus geen moment waarop er niets staat.
			//
			// Let op: dit werkt doordat de tekst niet verspringt tussen updates.
			// De inputfield-knop krijgt lowerCaseOffsetY == upperCaseOffsetY == 0,
			// dus applyOffsetForLowerCaseText verplaatst hem nooit. Zou dat wel
			// gebeuren, dan bleef de oude tekst op de oude hoogte staan.
			tsButton.updateText(label);

			// Hier stond vTaskDelay(100) met als commentaar "wait for the redraw
			// to be completed". Dat werkte averechts: deze functie draait in de
			// AsyncDisplay-taak (pollTouch -> touchlistener -> keyPressed), en
			// tekenen zet de commando's alleen maar in queueCommands. Die
			// queue wordt uitsluitend leeggehaald door de lus van diezelfde
			// AsyncDisplay-taak, dus tijdens het slapen kon de redraw juist niet
			// plaatsvinden - hij begon pas erna. Het was dus 100 ms dode tijd
			// voor elke toetsaanslag, plus twee gemiste touch-polls van 50 ms.
		}

	public:
		// next function will be called by the parent, when this touchscreenInputfield
		// is added to it as a child widget.
		/*override*/ /*virtual*/ void setDisplay(IDisplay& display)
		{
			Base::setDisplay(display);
			if (!bChildWidgetsAdded)
			{
				ESP_LOGI("setDisplay", "%s started adding ChildWidgets", this->getName());

				addChildWidgets();

				ESP_LOGI("setDisplay", "%s Added ChildWidgets", this->getName());
			}
		}

		/* override */ void buttonPressed(IButton* pButton)
		{
			//ESP_LOGI("Pressed", "button: %s", pButton->getButtonName());
			int i;

			if (pButton == &tsOutsideInputfieldButton)
			{
				//logger.logText("tsOutsideInputfieldButton pressed");
				//ESP_LOGI("tsOutsideInputfieldButton", "pressed");
				for (i = 0; i < nofInputfieldListeners; i++)
				{
					arInputfieldListener[i]->outsideInputfieldPressed(this);
				}
			}
			else
			{
				for (i = 0; i < nofInputfieldListeners; i++)
				{
					arInputfieldListener[i]->inputfieldPressed(this);
				}
			}
		}

		/* override */ void buttonReleased(IButton* pButton)
		{
			; // not interested.
		}

		/* override */ void keyPressed(const char* strKey)
		{
			int i=0;
			if(strncmp(strKey, "OutsideKeyboardPressed", 22) == 0)
			{return;} // ignore

			if(strncmp(strKey, "SHIFT", 5) == 0)
			{;}// do nothing, ignore shift key.
			else if(strncmp(strKey, "123", 3) == 0)
			{;} // do nothing, ignore 123 key.
			else if(strncmp(strKey, "abc", 3) == 0)
			{;} // do nothing, ignore abc key.
			else if(strncmp(strKey, "ENTER", 5) == 0)
			{;}
			else if(strncmp(strKey, "DEL", 3) == 0)
			{
				int len = _strFieldContent.length();
				if(len>0)
				{
					_strFieldContent[_strFieldContent.length()-1] = '\0';
					_strFieldContent = _strFieldContent.range_find_length(0, _strFieldContent.length()-1);
				}
				updateButtonLabel(tsInputfieldButton, _strFieldContent.begin());
			}
			else
			{
				if(_strFieldContent.length() >= (MaxTextSize-1))
				{
					//logger.logText("Inputfield: MaxTextSize reached");
					return; // ignore the key.
				}
				// if it is a normal key, add it to the inputfield.
				_strFieldContent += strKey; // don't copy to pool yet - only when the result is requested.
				updateButtonLabel(tsInputfieldButton, _strFieldContent.begin());
			}

			// pass on the pressed key.
			for (i = 0; i < nofInputfieldListeners; i++)
			{
				arInputfieldListener[i]->inputNewKey(this, strKey);
			}
		}

		// /* override */ void getTextValue(std::string_base& strFieldContent)
		// {
		// 	poolFieldContent.write(_strFieldContent); // transfer the content via the pool.
		// 	poolFieldContent.read(strFieldContent);
		// }

		/* override */ void getTextValue(std::string_base& strFieldContent) 
		{
			// Correcteer de schrijfoperatie naar de pool
			poolFieldContent.write(_strFieldContent); // Schrijf de huidige waarde naar de pool
			
			// Zorg ervoor dat je een tijdelijke waarde vermijdt
			std::string<MaxTextSize> tempStr;        // Creëer een tijdelijke variabele
			poolFieldContent.read(tempStr);          // Lees uit de pool
			strFieldContent = tempStr;               // Wijs toe aan de outputparameter
		}


		/* override */ void getFloatValue(float& fValue)
		{
			fValue=0.0f; // not implemented yet.
		}
		
		/* override */ void getIntValue(int& nValue)
		{
			nValue = 0; // not implemented yet.
		}
		
		/* override */ void keyReleased(const char* strKey)
		{
			; // not interested.
		}

		/* override */
		void addInputfieldListener(IInputfieldListener* pInputfieldListener)
		{
			if (!isAlreadyPresent(pInputfieldListener))
			{
				assert(nofInputfieldListeners < MaxnofInputfieldListeners);
				arInputfieldListener[nofInputfieldListeners++] = pInputfieldListener;
			}
		}

	private:
		bool isAlreadyPresent(IInputfieldListener* pInputfieldListener)
		{
			for (int i = 0; i < nofInputfieldListeners; i++)
			{
				//ESP_LOGI("adasdffasfsadf", "%s", i);
				if (arInputfieldListener[i] == pInputfieldListener)
				{
					return true;
				}
			}
			return false;
		}
	};
} // end namespace crt
