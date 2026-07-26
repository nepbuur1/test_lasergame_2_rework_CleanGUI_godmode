// by Marius Versteegen, 2024

#pragma once
#include <assert.h>
#include <crt_FreeRTOS.h>
#include "crt_typesAndConstants.h"
#include "crt_Image.h"
#include "crt_IDisplay.h"
#include "crt_Widget.h"

namespace crt
{
	template <size_t MaxNofChildren> 
	class Panel : public Widget<MaxNofChildren>
	{
	private:
		uint32_t _colFg, _colBg;
		int32_t _cornerRadius;
		int32_t _cornerRadiusPix;

		int32_t _cornerRadiusWhenCornerRadiusPixWasCalculated;

		// nullptr: the panel draws itself as a plain (rounded) rectangle.
		// Otherwise it blits this image instead. The Image is NOT copied - the
		// caller guarantees its lifetime (trivially so for const data in flash).
		const Image* _pImage;
	public:
		using WBase = Widget<MaxNofChildren>;

		Panel(const char* strName, const Vec2& locPos, CoordType coordTypeLocPos,
			  const Vec2& size, int32_t cornerRadius, CoordType coordTypeSize,
			  Alignment alignment, uint32_t colFg, uint32_t colBg, bool bDrawable = true) :
			  WBase(strName, locPos, coordTypeLocPos, size, coordTypeSize, alignment, bDrawable),
			  _colFg(colFg), _colBg(colBg), _cornerRadius(cornerRadius),
			  _cornerRadiusPix(-1),
			  _cornerRadiusWhenCornerRadiusPixWasCalculated(-1),
			  _pImage(nullptr)
		{
		}

		// Same, but the panel shows an image. The image is aligned within the
		// panel using the alignment parameter, and clipped to the panel.
		Panel(const char* strName, const Vec2& locPos, CoordType coordTypeLocPos,
			  const Vec2& size, int32_t cornerRadius, CoordType coordTypeSize,
			  Alignment alignment, uint32_t colFg, uint32_t colBg,
			  const Image& image, bool bDrawable = true) :
			  WBase(strName, locPos, coordTypeLocPos, size, coordTypeSize, alignment, bDrawable),
			  _colFg(colFg), _colBg(colBg), _cornerRadius(cornerRadius),
			  _cornerRadiusPix(-1),
			  _cornerRadiusWhenCornerRadiusPixWasCalculated(-1),
			  _pImage(&image)
		{
		}

		Panel() : Panel("", Vec2(0,0), CoordType::Pixels, Vec2(0,0), 0, CoordType::Pixels, Alignment::TopLeft, 0, 0)
		{
		}

		// convenience function to set most properties at once.
		void setPanelProps(const char* strName, const Vec2& locPos, CoordType coordTypeLocPos,
			  const Vec2& size, int32_t cornerRadius, CoordType coordTypeSize, 
			  Alignment alignment, uint32_t colFg, uint32_t colBg)
		{
			WBase::setWidgetProps(strName, locPos, coordTypeLocPos, size, coordTypeSize, alignment);
			_colFg = colFg;
			_colBg = colBg;
			_cornerRadius = cornerRadius;
		}

		// Pass nullptr to turn the panel back into a plain coloured rectangle.
		void setImage(const Image* pImage)
		{
			_pImage = pImage;
		}

		const Image* getImage()
		{
			return _pImage;
		}

		/* override */ WidgetType getWidgetType()
		{
			return WidgetType::Panel;
		}

		// show immediately draws it. hide immediately undraws it.
		// note: _widgetProps only serves as a node in the display-tree.
		//       It conains the props and links to children.
		//       The actual (un-)display functionality is to be implemented
		//       by the specific widget class (like TouchScreenButton),
		//       which is derived from IWidget as well.
		/*override*/ void show(bool bIncludeChildren)
		{
			showPanel();
			Widget<MaxNofChildren>::show(bIncludeChildren); // set the boolean bVisible to remember.
		}

		// show immediately draws it. hide immediately undraws it.
		/*override*/ void hide(bool bIncludeChildren)
		{
			hidePanel();
			WBase::hide(bIncludeChildren); // sets the boolean bVisible to false to remember.
		}

	private:
		void updateCornerRadiusPixIfNeeded()
		{
			if (_cornerRadius != _cornerRadiusWhenCornerRadiusPixWasCalculated)
			{
				// apply same scaling as for size to sizePix:
				if (WBase::getSize().x != 0)
				{
					int64_t tempRadiusPix = (int64_t)_cornerRadius * (int64_t)WBase::getSizePix().x / (int64_t)WBase::getSize().x;
					_cornerRadiusPix = int32_t(tempRadiusPix);
				}
				else
				{
					ESP_LOGI("Zero size of panel?", "div by zero avoided");
				}
			}
		}

		// An image is drawn on top of the panel background. Painting that
		// background first is wasted bus traffic when the image covers it
		// anyway - and it is plain wrong for a transparent image, because there
		// it is exactly the pixels underneath (drawn by the parent) that should
		// stay visible.
		// Note: an opaque image that covers the whole panel therefore also
		// suppresses the rounded corners - a rectangular blit cannot honour
		// them. Give such a panel a transparent image if you want rounded
		// corners, or make the panel bigger than the image.
		bool needsBackground(const Vec2& sizePix)
		{
			if (_pImage == nullptr)
			{
				return true;
			}
			if (_pImage->bHasTransparency)
			{
				return false;
			}
			// Opaque image: only paint the background if the image leaves part
			// of the panel uncovered.
			return (_pImage->width < sizePix.x) || (_pImage->height < sizePix.y);
		}

		void showPanel()
		{
			if (!WBase::isDrawable())
			{
				return;
			}

			updateCornerRadiusPixIfNeeded();

			Vec2 pos = WBase::getGlobPosOfParent() + WBase::getLocPosPix();
			Vec2 size = WBase::getSizePix();
			//ESP_LOGI("Panel", "showPanel called: pos(%ld,%ld) size(%ld,%ld) colFg=0x%08lX cornerRadius=%ld",
			//	(long)pos.x, (long)pos.y, (long)size.x, (long)size.y, (unsigned long)_colFg, (long)_cornerRadiusPix);

			if (needsBackground(size))
			{
				if (_cornerRadiusPix > 0)
				{
					WBase::getDisplay()->fillSmoothRoundRect(pos, size, _cornerRadiusPix, _colFg, _colBg);
				}
				else
				{
					WBase::getDisplay()->fillRect(pos, size, _colFg);
				}
			}

			if (_pImage != nullptr)
			{
				WBase::getDisplay()->drawImage(pos, size, *_pImage, WBase::getAlignment());
			}
			//ESP_LOGI("Panel shown", "globparent Y = %d", _widgetProps.getGlobPosOfParent().y);
			//ESP_LOGI("Panel shown", "locpospix Y = %d", _widgetProps.getLocPosPix().y);
			//ESP_LOGI("Panel shown", "locpos Y = %d", _widgetProps.getLocPos().y);
		}

		void hidePanel()
		{
			if (!WBase::isDrawable())
			{
				return;
			}

			WBase::getDisplay()->fillRect(WBase::getGlobPosOfParent() + WBase::getLocPosPix(), WBase::getSizePix(), _colBg);
		}
	};
};
