// by Marius Versteegen, 2024

#pragma once
#include <assert.h>
#include <crt_FreeRTOS.h>
#include "crt_typesAndConstants.h"
#include "crt_IDisplay.h"
#include "crt_Widget.h"
#include "crt_Vec2.h"

namespace crt
{
	// The String used here

	template <size_t MaxNofChildren> 
	class Text : public Widget<MaxNofChildren>
	{
	private:
		//const char* str;
		uint8_t _font;
		uint8_t _fontScale;  // integer pix multiplier
		uint32_t _color;
		bool _bSizeUpToDate; // no longer used? Obsolete?
	public:
		using WBase = Widget<MaxNofChildren>;

		Text(const Vec2& locPos, CoordType coordTypeLocPos, 
			  Alignment alignment,
			  const char* str, uint8_t font, uint32_t color, uint8_t fontScale) :
			  WBase(str, locPos, coordTypeLocPos, Vec2(0,0), CoordType::Pixels, alignment),
			  /*str(str), */_font(font), _fontScale(fontScale), _color(color), _bSizeUpToDate(false)
			  {
				  // determine dimensions of the text .. that'll need to 
				  // be postponed till we have a handle of the display to ask it.
			  }

		Text() : Text(Vec2(0,0), CoordType::Pixels, Alignment::TopLeft, "", 0, 0, 0)
		{}

		// convenience function to set most properties at once.
		void setTextProps(const Vec2& locPos, CoordType coordTypeLocPos, 
			  Alignment alignment,
			  const char* str, uint8_t font, uint32_t color, uint8_t fontScale)
		{
			WBase::setWidgetProps(str, locPos, coordTypeLocPos, Vec2(0,0), CoordType::Pixels, alignment);
			//str = str;
			_font = font;
			_fontScale = fontScale;
			_color = color;
			_bSizeUpToDate = false;
		}

		/* override */ WidgetType getWidgetType()
		{
			return WidgetType::Text;
		}

		uint8_t getFontScale()
		{
			return _fontScale;
		}

		void setFontScale(uint8_t fontScale)
		{
			if (fontScale != _fontScale)
			{
				_fontScale = fontScale;
				updateSize();
			}
		}

		uint8_t getFont()
		{
			return _font;
		}

		void setFont(uint8_t font)
		{
			if (font != _font)
			{
				_font = font;
				updateSize();
			}
		}

		/* override */  virtual void setName(const char* str)
		{
			setText(str);
		}

		/* override */  virtual const char* getName()
		{
			return getText();
		}

		const char* getText()
		{
			//return str;
			return WBase::getName();
		}

		void setText(const char* str)
		{
			if (str != WBase::getName())
			{
				WBase::setName(str);
				updateSize();
			}
		}

		// show immediately draws it. hide immediately undraws it.
		// note: _widgetProps only serves as a node in the display-tree.
		//       It conains the props and links to children.
		//       The actual (un-)display functionality is to be implemented
		//       by the specific widget class (like TouchScreenButton),
		//       which is derived from IWidget as well.
		/*override*/ void show(bool bIncludeChildren)
		{
			showText();
			WBase::show(bIncludeChildren); // set the boolean bVisible to remember.
		}

		// show immediately draws it. hide immediately undraws it.
		/*override*/ void hide(bool bIncludeChildren)
		{
			hideText();
			WBase::hide(bIncludeChildren); // sets the boolean bVisible to false to remember.
		}

		// CoordType can be Pixels or Promillage
		/*override*/ void setLocPos(const Vec2& locPos, const CoordType coordTypeLocPos, const Alignment alignment)
		{
			WBase::setLocPos(locPos, coordTypeLocPos, alignment);
		}

		/*override*/ Alignment getAlignment()
		{
			return WBase::getAlignment();
		}

		/*override*/ CoordType getCoordTypeLocPos()
		{
			return WBase::getCoordTypeLocPos();
		}
		
		/*override*/  CoordType getCoordTypeSize()
		{
			return WBase::getCoordTypeSize();
		}

	private:
		void updateSize()
		{
			IDisplay* display = WBase::getDisplay();
			if (display == nullptr)
			{
				return; // Size cannot be updated till the display is attached.
			}

			Vec2 size(display->getTextWidth(WBase::getName(), _font) * _fontScale, display->getFontHeight(_font) * _fontScale);
			WBase::setSize(size, CoordType::Pixels);
		}

		void showText()
		{
			Vec2 origin;

			switch (WBase::getAlignment())
			{
			case Alignment::BottomLeft:
				{
					int32_t parentVertSizeMinusLocVertSize = WBase::getSizeOfParent().y;// -WBase::getSize().y;
					origin = WBase::getGlobPosOfParent() + Vec2(0, parentVertSizeMinusLocVertSize);
				}
				break;

			case Alignment::TopLeft:
				origin = WBase::getGlobPosOfParent();
				break;

			case Alignment::MidLeft:
			{
				int32_t parentVertSizeMinusLocVertSizeDiv2 = WBase::getSizeOfParent().y/2;// -WBase::getSize().y;
				origin = WBase::getGlobPosOfParent() + Vec2(0, parentVertSizeMinusLocVertSizeDiv2);
			}
			break;

			case Alignment::TopRight:
			{
				origin = WBase::getGlobPosOfParent() + Vec2(WBase::getSizeOfParent().x, 0);
			}
			break;

			case Alignment::BottomRight:
			{
				origin = WBase::getGlobPosOfParent() + Vec2(WBase::getSizeOfParent().x, WBase::getSizeOfParent().y);
			}
			break;

			case Alignment::MidRight:
			{
				origin = WBase::getGlobPosOfParent() + Vec2(WBase::getSizeOfParent().x, WBase::getSizeOfParent().y/2);
			}
			break;

			case Alignment::BottomMid:
			{
				origin = WBase::getGlobPosOfParent() + Vec2(WBase::getSizeOfParent().x/2, WBase::getSizeOfParent().y);
			}
			break;

			case Alignment::TopMid:
			{
				origin = WBase::getGlobPosOfParent() + Vec2(WBase::getSizeOfParent().x/2, 0);
			}
			break;
			
			case Alignment::MidMid:
			{
				origin = WBase::getGlobPosOfParent() + Vec2(WBase::getSizeOfParent().x/2, WBase::getSizeOfParent().y / 2);
			}
			break;

			default:
				ESP_LOGI("crt_Text", "alignment %d is not implemented yet",(int)WBase::getAlignment());
				break;
			}

			WBase::getDisplay()->drawString(WBase::getName(), origin + WBase::getLocPosPix(), WBase::getAlignment(), _font, _fontScale, _color);
		}

		void hideText()
		{
			// something with a bg color?
		}
	};
};
