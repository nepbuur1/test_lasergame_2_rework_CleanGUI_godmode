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

namespace crt
{
	enum TouchscreenKeyboardMode {LowercaseAlphabet,UppercaseAlphabet,Numerical};

	template <size_t MaxNofChildren, size_t MaxNofKeyboardListeners>
	class TouchscreenKeyboard : public TouchscreenButtonGroup<MaxNofChildren+40>,
		public IKeyboard, public IButtonListener
	{
	private: 
		const int nofKeysTopRow = 10;
		const int keyWidth = 1000 * 1000 / (1160 * nofKeysTopRow); // reserve 1/6 buttonwidth per button for padding
		const int paddingX  = (1000 - keyWidth * nofKeysTopRow)/(nofKeysTopRow+1);
		const int stepSizeX = keyWidth + paddingX;
		
		const int nofRows = 4;
		const int keyHeight = 1000 * 1000 / (1250 * nofRows); // reserve 1/4 buttonheight per button for padding
		const int paddingY = (1000 - keyHeight * nofRows)/(nofRows+1);
		const int stepSizeY = keyHeight + paddingY;

		const uint32_t colBg   = 0x00000000;
		const uint32_t colFont = 0x00FFFFFF;

		IKeyboardListener* arKeyboardListener[MaxNofKeyboardListeners] = {};
		uint16_t nofKeyboardListeners;

		bool bChildWidgetsAdded;

		const int keyCornerRadiusPromillage=50;
		const int buttonFont = 12;
		TouchscreenKeyboardMode touchscreenKeyboardMode;

		//TouchscreenButton<0 /*MaxNofChildren*/, 1/*MaxButtonListenerCount*/> tsButtonA;
		using TSButton = TouchscreenButton<0, 1>;
		TSButton tsButtonQ, tsButtonW, tsButtonE, tsButtonR, tsButtonT, tsButtonY, tsButtonU, tsButtonI, tsButtonO, tsButtonP, 
		            tsButtonA, tsButtonS, tsButtonD, tsButtonF, tsButtonG, tsButtonH, tsButtonJ, tsButtonK, tsButtonL,
					  tsButtonZ, tsButtonX, tsButtonC, tsButtonV, tsButtonB, tsButtonN, tsButtonM, tsButtonDel,
				 tsButtonShift, tsButtonSpace, tsButtonToggleNumeric, tsButtonEnter;

		TSButton tsOutsideKeyboardButton; // This button is used to register a touch outside the keyboard.

	public:
		using Base = TouchscreenButtonGroup<MaxNofChildren + 40>;
		using WBase = Widget<MaxNofChildren + 40>;

			/*tsButtonB, tsButtonC, tsButtonD, tsButtonD, tsButtonE,
			tsButtonF, tsButtonG, tsButtonI, tsButtonJ, tsButtonK, tsButtonL, tsButtonM,
			tsButtonN, tsButtonO, tsButtonP, tsButtonQ, tsButtonR, tsButtonS, tsButtonT,
			tsButtonU, tsButtonV, tsButtonW, tsButtonX, tsButtonY, tsButtonZ, tsButtonShiftToUpper,
			tsButtonToggleNumeric, tsButtonSlashForward, tsButtonBackSpace, tsButtonDot, tsButtonEnter*/

		TouchscreenKeyboard(const char* strName, const Vec2& locPos, CoordType coordTypeLocPos,
			  const Vec2& size, /*int32_t cornerRadius,*/ CoordType coordTypeSize, 
			  Alignment alignment, uint32_t colFg, uint32_t colBg, TouchscreenKeyboardMode initialTouchscreenKeyboardMode) :
			/*Base::TouchscreenButtonGroupPanel(strName, locPos, coordTypeLocPos, size, cornerRadius, coordTypeSize, alignment, colFg, colBg),*/
			Base::TouchscreenButtonGroup(strName, locPos, coordTypeLocPos, size, /*cornerRadius,*/ coordTypeSize, alignment),
			nofKeyboardListeners(0),
			bChildWidgetsAdded(false),
			touchscreenKeyboardMode(TouchscreenKeyboardMode::LowercaseAlphabet) /* will be updated at the end of the constructor. */,
			tsOutsideKeyboardButton("tsOutsideKeyboardButton", Vec2(0,0), CoordType::Promillage, Vec2(1000,1000), 0, CoordType::Promillage, Alignment::TopLeft, colFg, colBg, true /*use inverted area for touch detection*/, "", 0, 0, 0, Alignment::TopLeft, 0, 0)
		{
			tsOutsideKeyboardButton.addButtonListener(this);	// This large "button" displays the back of the keyboard, and will be used to register a touch outside the keyboard.

			initTsButton(tsButtonQ, "q", 0, 0);
			initTsButton(tsButtonW, "w", 0, 1);
			initTsButton(tsButtonE, "e", 0, 2);
			initTsButton(tsButtonR, "r", 0, 3);
			initTsButton(tsButtonT, "t", 0, 4);
			initTsButton(tsButtonY, "y", 0, 5);
			initTsButton(tsButtonU, "u", 0, 6);
			initTsButton(tsButtonI, "i", 0, 7);
			initTsButton(tsButtonO, "o", 0, 8);
			initTsButton(tsButtonP, "p", 0, 9);
			initTsButton(tsButtonA, "a", 1, 0);
			initTsButton(tsButtonS, "s", 1, 1);
			initTsButton(tsButtonD, "d", 1, 2);
			initTsButton(tsButtonF, "f", 1, 3);
			initTsButton(tsButtonG, "g", 1, 4);
			initTsButton(tsButtonH, "h", 1, 5);
			initTsButton(tsButtonJ, "j", 1, 6);
			initTsButton(tsButtonK, "k", 1, 7);
			initTsButton(tsButtonL, "l", 1, 8);
			initTsButton(tsButtonZ, "z", 2, 0);
			initTsButton(tsButtonX, "x", 2, 1);
			initTsButton(tsButtonC, "c", 2, 2);
			initTsButton(tsButtonV, "v", 2, 3);
			initTsButton(tsButtonB, "b", 2, 4);
			initTsButton(tsButtonN, "n", 2, 5);
			initTsButton(tsButtonM, "m", 2, 6);
			initTsButton(tsButtonDel, "DEL", 2, 7);
			initTsButton(tsButtonShift, "SHIFT", 3, 0);
			initTsButton(tsButtonSpace, " ", 3, 1);
			initTsButton(tsButtonToggleNumeric, "123", 3, 2);
			initTsButton(tsButtonEnter, "ENTER", 3, 3);

			switchKeyboardMode(touchscreenKeyboardMode);
		}

		void switchKeyboardMode(TouchscreenKeyboardMode touchscreenKeyboardMode)
		{
			if(touchscreenKeyboardMode!=this->touchscreenKeyboardMode)
			{
				this->touchscreenKeyboardMode = touchscreenKeyboardMode;
				switch(this->touchscreenKeyboardMode)
				{
					case TouchscreenKeyboardMode::LowercaseAlphabet:
						switchToLowerCaseAlphabet();
					break;

					case TouchscreenKeyboardMode::UppercaseAlphabet:
						switchToUpperCaseAlphabet();
					break;

					case TouchscreenKeyboardMode::Numerical:
						switchToNumerical();
					break;

					default:
						assert(false);
					break;
				}
			}
		}

	private:
		void initTsButton(TSButton& tsButton, const char* name, int row, int col)
		{
			int x = 0;
			int y = paddingY / 2 + row * stepSizeY;
			int keyWidthMultiplier = 1; // keyWidth is for the smallest keys. On the bottom row, the keys ar three times as wide.

			float keyWidthMultipliersRow3[] = {3.0f, 3.0f, 2.0f, 3.0f}; // For Shift, Space, 123, Enter
			if (row < 3) // The small keys on the top 3 rows.
			{
				if((row==2)&&(col==7)) // the DEL key
				{
					keyWidthMultiplier = 2;
				}

				x = paddingX / 2 + col * stepSizeX + row * stepSizeX / 2;
			}
			else // row == 3, the bottom row with larger keys.
			{
				keyWidthMultiplier = keyWidthMultipliersRow3[col];
				x = paddingX / 2;
				for(int i = 0; i < col; i++)
				{
					x += (paddingX + keyWidth * keyWidthMultipliersRow3[i]); // add the space of the previous keys.
				} 
			}
			// fontScale=0 triggers automatical matching of fontscale with the button height.
			tsButton.setTouchScreenButtonProps(/*name*/name,
				/*locPos*/Vec2(x, y),
				CoordType::Promillage, /*size*/Vec2(keyWidth*keyWidthMultiplier, keyHeight), /*cornerRadius*/keyCornerRadiusPromillage, CoordType::Promillage,
				Alignment::TopLeft,	/*colPanel*/0x00FF0000, /*colBg*/0x00000000, /*bInvertedArea*/false,
				/*text*/ name, /*buttonFont*/ buttonFont, /*colFont*/ 0x00FFFFFF, /*fontScale*/ 0, Alignment::MidMid, /*lowerCaseOffsetY*/-120, /*upperCaseOffsetY*/-70);

			tsButton.addButtonListener(this);
		}

		void addChildWidgets()
		{
			ESP_LOGI("keyb", "will Add ChildWidgets");
			Base::addTouchListener(tsOutsideKeyboardButton, tsOutsideKeyboardButton);

			Base::addTouchListener(tsButtonQ, tsButtonQ);
			Base::addTouchListener(tsButtonW, tsButtonW);
			Base::addTouchListener(tsButtonE, tsButtonE);
			Base::addTouchListener(tsButtonR, tsButtonR);
			Base::addTouchListener(tsButtonT, tsButtonT);
			Base::addTouchListener(tsButtonY, tsButtonY);
			Base::addTouchListener(tsButtonU, tsButtonU);
			Base::addTouchListener(tsButtonI, tsButtonI);
			Base::addTouchListener(tsButtonO, tsButtonO);
			Base::addTouchListener(tsButtonP, tsButtonP);
			Base::addTouchListener(tsButtonA, tsButtonA);
			Base::addTouchListener(tsButtonS, tsButtonS);
			Base::addTouchListener(tsButtonD, tsButtonD);
			Base::addTouchListener(tsButtonF, tsButtonF);
			Base::addTouchListener(tsButtonG, tsButtonG);
			Base::addTouchListener(tsButtonH, tsButtonH);
			Base::addTouchListener(tsButtonJ, tsButtonJ);
			Base::addTouchListener(tsButtonK, tsButtonK);	
			Base::addTouchListener(tsButtonL, tsButtonL);
			Base::addTouchListener(tsButtonZ, tsButtonZ);
			Base::addTouchListener(tsButtonX, tsButtonX);
			Base::addTouchListener(tsButtonC, tsButtonC);
			Base::addTouchListener(tsButtonV, tsButtonV);
			Base::addTouchListener(tsButtonB, tsButtonB);
			Base::addTouchListener(tsButtonN, tsButtonN);
			Base::addTouchListener(tsButtonM, tsButtonM);	
			Base::addTouchListener(tsButtonDel, tsButtonDel);	
			Base::addTouchListener(tsButtonShift, tsButtonShift);
			Base::addTouchListener(tsButtonSpace, tsButtonSpace);
			Base::addTouchListener(tsButtonToggleNumeric, tsButtonToggleNumeric);
			Base::addTouchListener(tsButtonEnter, tsButtonEnter);

			bChildWidgetsAdded = true;
		}

		void updateButtonLabel(TSButton& tsButton, const char* label)
		{
			tsButton.setText(label);
			tsButton.setButtonName(label); // because IButton does not have a setText function.
		}

		void switchToLowerCaseAlphabet()
		{
			updateButtonLabel(tsButtonQ, "q");
			updateButtonLabel(tsButtonW, "w");
			updateButtonLabel(tsButtonE, "e");
			updateButtonLabel(tsButtonR, "r");
			updateButtonLabel(tsButtonT, "t");
			updateButtonLabel(tsButtonY, "y");
			updateButtonLabel(tsButtonU, "u");
			updateButtonLabel(tsButtonI, "i");
			updateButtonLabel(tsButtonO, "o");
			updateButtonLabel(tsButtonP, "p");
			updateButtonLabel(tsButtonA, "a");
			updateButtonLabel(tsButtonS, "s");
			updateButtonLabel(tsButtonD, "d");
			updateButtonLabel(tsButtonF, "f");
			updateButtonLabel(tsButtonG, "g");
			updateButtonLabel(tsButtonH, "h");
			updateButtonLabel(tsButtonJ, "j");
			updateButtonLabel(tsButtonK, "k");
			updateButtonLabel(tsButtonL, "l");
			updateButtonLabel(tsButtonZ, "z");
			updateButtonLabel(tsButtonX, "x");
			updateButtonLabel(tsButtonC, "c");
			updateButtonLabel(tsButtonV, "v");
			updateButtonLabel(tsButtonB, "b");
			updateButtonLabel(tsButtonN, "n");
			updateButtonLabel(tsButtonM, "m");
			updateButtonLabel(tsButtonToggleNumeric, "123");
			Base::show(true); // redraw this keyboard.
		}

		void switchToUpperCaseAlphabet()
		{
			updateButtonLabel(tsButtonQ, "Q");
			updateButtonLabel(tsButtonW, "W");
			updateButtonLabel(tsButtonE, "E");
			updateButtonLabel(tsButtonR, "R");
			updateButtonLabel(tsButtonT, "T");
			updateButtonLabel(tsButtonY, "Y");
			updateButtonLabel(tsButtonU, "U");
			updateButtonLabel(tsButtonI, "I");
			updateButtonLabel(tsButtonO, "O");
			updateButtonLabel(tsButtonP, "P");
			updateButtonLabel(tsButtonA, "A");
			updateButtonLabel(tsButtonS, "S");
			updateButtonLabel(tsButtonD, "D");
			updateButtonLabel(tsButtonF, "F");
			updateButtonLabel(tsButtonG, "G");
			updateButtonLabel(tsButtonH, "H");
			updateButtonLabel(tsButtonJ, "J");
			updateButtonLabel(tsButtonK, "K");
			updateButtonLabel(tsButtonL, "L");
			updateButtonLabel(tsButtonZ, "Z");
			updateButtonLabel(tsButtonX, "X");
			updateButtonLabel(tsButtonC, "C");
			updateButtonLabel(tsButtonV, "V");
			updateButtonLabel(tsButtonB, "B");
			updateButtonLabel(tsButtonN, "N");
			updateButtonLabel(tsButtonM, "M");
			updateButtonLabel(tsButtonToggleNumeric, "123");
			Base::show(true); // redraw this keyboard.
		}

		void switchToNumerical()
		{
			updateButtonLabel(tsButtonQ, "!");
			updateButtonLabel(tsButtonW, "7");
			updateButtonLabel(tsButtonE, "8");
			updateButtonLabel(tsButtonR, "9");
			updateButtonLabel(tsButtonT, "%");
			updateButtonLabel(tsButtonY, "*");
			updateButtonLabel(tsButtonU, "-");
			updateButtonLabel(tsButtonI, "+");
			updateButtonLabel(tsButtonO, "(");
			updateButtonLabel(tsButtonP, ")");
			updateButtonLabel(tsButtonA, "#");
			updateButtonLabel(tsButtonS, "4");
			updateButtonLabel(tsButtonD, "5");
			updateButtonLabel(tsButtonF, "6");
			updateButtonLabel(tsButtonG, "@");
			updateButtonLabel(tsButtonH, ":");
			updateButtonLabel(tsButtonJ, ";");
			updateButtonLabel(tsButtonK, "<");
			updateButtonLabel(tsButtonL, ">");
			updateButtonLabel(tsButtonZ, "0");
			updateButtonLabel(tsButtonX, "1");
			updateButtonLabel(tsButtonC, "2");
			updateButtonLabel(tsButtonV, "3");
			updateButtonLabel(tsButtonB, ",");
			updateButtonLabel(tsButtonN, ".");
			updateButtonLabel(tsButtonM, "?");
			updateButtonLabel(tsButtonToggleNumeric, "abc");
			Base::show(true); // redraw this keyboard.
		}

	public:
		// next function will be called by the parent, when this touchscreenKeyBoardLowerCase
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

			if (pButton == &tsOutsideKeyboardButton)
			{
				//logger.logText("tsOutsideKeyboardButton pressed");
				//ESP_LOGI("tsOutsideKeyboardButton", "pressed");
				for (i = 0; i < nofKeyboardListeners; i++)
				{
					arKeyboardListener[i]->keyPressed("OutsideKeyboardPressed");
				}
				// Base::hide(true);	// Pressing outside the keyboard will hide the keyboard - Nope, this is not the responsibility of the keyboard.
				return;
			}

			for (i = 0; i < nofKeyboardListeners; i++)
			{
				arKeyboardListener[i]->keyPressed(pButton->getButtonName());
			}

			if(pButton == &tsButtonShift)
			{
				if(touchscreenKeyboardMode!=TouchscreenKeyboardMode::LowercaseAlphabet)
				{
					switchKeyboardMode(TouchscreenKeyboardMode::LowercaseAlphabet);
				}
				else
				{
					switchKeyboardMode(TouchscreenKeyboardMode::UppercaseAlphabet);
				}
			}
			else if(pButton == &tsButtonToggleNumeric)
			{
				if(touchscreenKeyboardMode!=TouchscreenKeyboardMode::Numerical)
				{
					switchKeyboardMode(TouchscreenKeyboardMode::Numerical);
				}
				else
				{
					switchKeyboardMode(TouchscreenKeyboardMode::LowercaseAlphabet);
				}
			}
		}

		/* override */ void buttonReleased(IButton* pButton)
		{
			//ESP_LOGI("Released", "button: %s", pButton->getButtonName());
			int i;
			for (i = 0; i < nofKeyboardListeners; i++)
			{
				arKeyboardListener[i]->keyReleased(pButton->getButtonName());
			}
		}
		
		/* override */
		void addKeyboardListener(IKeyboardListener* pKeyboardListener)
		{
			if (!isAlreadyPresent(pKeyboardListener))
			{
				assert(nofKeyboardListeners < MaxNofKeyboardListeners);
				arKeyboardListener[nofKeyboardListeners++] = pKeyboardListener;
			}
		}

	private:
		bool isAlreadyPresent(IKeyboardListener* pKeyboardListener)
		{
			for (int i = 0; i < nofKeyboardListeners; i++)
			{
				//ESP_LOGI("adasdffasfsadf", "%s", i);
				if (arKeyboardListener[i] == pKeyboardListener)
				{
					return true;
				}
			}
			return false;
		}
	};
} // end namespace crt
