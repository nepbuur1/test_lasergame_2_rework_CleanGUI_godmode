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

		// Opaque drawing: the glyph cells are painted with _colBg instead of being
		// left transparent, and whatever the text does not cover within
		// _paddingSizePix is painted with that same colour. That makes replacing the text a single pass - no erase
		// first, so no moment where the area is blank, so no flicker.
		// Off by default, so existing widgets keep drawing exactly as before.
		bool _bOpaque;
		uint32_t _colBg;
		int32_t _opaqueMarginPix;	// houdt het gereserveerde vak van de randen af
	public:
		using WBase = Widget<MaxNofChildren>;

		Text(const Vec2& locPos, CoordType coordTypeLocPos,
			  Alignment alignment,
			  const char* str, uint8_t font, uint32_t color, uint8_t fontScale,
			  bool bDrawable = true) :
			  WBase(str, locPos, coordTypeLocPos, Vec2(0,0), CoordType::Pixels, alignment, bDrawable),
			  /*str(str), */_font(font), _fontScale(fontScale), _color(color), _bSizeUpToDate(false),
			  _bOpaque(false), _colBg(0), _opaqueMarginPix(0)
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

		// Makes this Text draw itself opaquely: the glyph cells and the rest of
		// the reserved area are painted with colBg. Use it for text that changes
		// while it is on screen - a label can then be replaced without first
		// erasing it, which is what removes the flicker.
		// colBg should match whatever is behind the text - typically the colFg of
		// the panel it sits on. The area reserved for the text is derived from
		// the parent's rect at draw time (see calcPaddingSizePix), so it keeps
		// following the text if its position changes afterwards.
		// marginPix keeps the reserved area away from the parent's edges. Needed
		// when the parent draws something there that must survive, such as a
		// bevel - otherwise the blanking would paint straight over it.
		void setOpaque(uint32_t colBg, int32_t marginPix = 0)
		{
			_bOpaque = true;
			_colBg = colBg;
			_opaqueMarginPix = marginPix;
		}

		void setTransparent()
		{
			_bOpaque = false;
		}

		bool isOpaque()
		{
			return _bOpaque;
		}

		// Redraws just this text, without touching whatever is behind it.
		// Only flicker-free if this Text is opaque - otherwise the previous text
		// is still there and the two will overlap.
		void redraw()
		{
			showText();
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
		/*override*/ void paint()
		{
			showText();
		}

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
			if (!WBase::isDrawable())
			{
				return;
			}

			drawTextOrPadding(WBase::getName());
		}

		// Issues the actual draw call for str, at this Text's own position.
		// Split off from showText so that hideText can reuse the very same
		// geometry by passing an empty string.
		void drawTextOrPadding(const char* str)
		{
			Vec2 pos = calcOriginPix() + WBase::getLocPosPix();

			if (_bOpaque)
			{
				// Het datum een marge naar binnen. De adapter leidt de
				// gereserveerde box uit dit punt af, dus zonder deze verschuiving
				// begint die box op de rand van de ouder - en dan veegt de
				// glyphcel precies de bevel weg die de marge moest beschermen.
				// Verticaal ging dat al goed: calcPaddingSizePix() kort de hoogte
				// aan twee kanten in en de box ligt om het datum gecentreerd, dus
				// daar ontstaat de inspringing vanzelf.
				pos += Vec2(calcOpaqueOffsetXPix(), 0);

				WBase::getDisplay()->drawStringOpaque(str, pos, WBase::getAlignment(),
					_font, _fontScale, _color, _colBg, calcPaddingSizePix());
			}
			else
			{
				WBase::getDisplay()->drawString(str, pos, WBase::getAlignment(), _font, _fontScale, _color);
			}
		}

		// The area this Text may paint in: from its own origin to the edge of its
		// parent, as high as the parent. Nothing is drawn outside it, so a widget
		// can never smear over its neighbours - and a previous, longer text is
		// blanked all the way to the parent's edge.
		// Hoeveel het tekstdatum naar binnen schuift bij opake tekst. Alleen voor
		// links- en rechtsuitgelijnde tekst: bij gecentreerde ligt de box al
		// symmetrisch om het datum, en dan zou schuiven hem juist scheeftrekken.
		int32_t calcOpaqueOffsetXPix()
		{
			switch (WBase::getAlignment())
			{
			case Alignment::TopLeft:
			case Alignment::MidLeft:
			case Alignment::BottomLeft:
				return _opaqueMarginPix;

			case Alignment::TopRight:
			case Alignment::MidRight:
			case Alignment::BottomRight:
				return -_opaqueMarginPix;

			default:
				return 0;
			}
		}

		Vec2 calcPaddingSizePix()
		{
			Vec2 parentSizePix = WBase::getSizeOfParent();
			Vec2 locPosPix = WBase::getLocPosPix();
			int32_t m = _opaqueMarginPix;

			switch (WBase::getAlignment())
			{
			// 2*m, net als bij de hoogte: het datum is zelf al m naar binnen
			// geschoven (zie calcOpaqueOffsetXPix), dus zonder de tweede m zou de
			// box aan de overkant weer over de bevel heen lopen.
			case Alignment::TopLeft:
			case Alignment::MidLeft:
			case Alignment::BottomLeft:
				return Vec2(parentSizePix.x - locPosPix.x - 2 * m, parentSizePix.y - 2 * m);

			case Alignment::TopRight:
			case Alignment::MidRight:
			case Alignment::BottomRight:
				return Vec2(parentSizePix.x + locPosPix.x - 2 * m, parentSizePix.y - 2 * m);

			default:	// centred: keep it symmetrical around the shifted origin.
				return Vec2(parentSizePix.x - 2 * ((locPosPix.x < 0) ? -locPosPix.x : locPosPix.x) - 2 * m, parentSizePix.y - 2 * m);
			}
		}

		Vec2 calcOriginPix()
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

			return origin;
		}

		void hideText()
		{
			if (!WBase::isDrawable())
			{
				return;
			}

			if (!_bOpaque)
			{
				// A transparent Text has no idea what colour is behind it, so it
				// cannot paint itself away. It disappears only because the panel
				// it sits on repaints over it - which is why a Text without a
				// Panel underneath cannot be hidden at all.
				return;
			}

			// Draw an empty string over the reserved width: nothing is written,
			// and the whole padded area is blanked with _colBg.
			drawTextOrPadding("");
		}
	};
};
