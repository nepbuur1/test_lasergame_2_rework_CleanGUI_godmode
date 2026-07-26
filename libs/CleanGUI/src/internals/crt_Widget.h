// by Marius Versteegen, 2024

#pragma once
#include <assert.h>
#include "crt_typesAndConstants.h"
#include "crt_IWidget.h"

namespace crt
{
	template <size_t MaxNofChildren> 
	class Widget : public IWidget
	{
	private:
		// The four values below will be filled in by the 
		// parent, after adding the widget to that parent.
		IDisplay* _display;
		IWidget* _pParent;		// nullptr voor de root. Gezet door addChildWidget.
		const char* _strName;

		Vec2 _globPosOfParent;
		Vec2 _sizeOfParent;

		Vec2 _locPos;
		Vec2 _locPosPix;					// In pixels wrt origin of parent
		CoordType _coordTypeLocPos;
		Vec2 _size;
		Vec2 _sizePix;                   
		CoordType _coordTypeSize;
		Alignment _alignment;
		bool _bVisible;
		bool _bEnabled;
		bool _bDrawable;

	protected: // Protected, to support easy enumeration by derived classes.
		int _nofChildren;
		IWidget* _arChildren[MaxNofChildren];

	public:
		Widget(const char* strName, const Vec2& locPos, CoordType coordTypeLocPos,
			   const Vec2& size, CoordType coordTypeSize, Alignment alignment,
			   bool bDrawable = true) :
			_display(nullptr), _pParent(nullptr), _strName(strName),   // both are set when added to a parent.
			_globPosOfParent(0,0), _sizeOfParent(0,0),
			_locPos(locPos), _coordTypeLocPos(coordTypeLocPos),
			_size(size), _coordTypeSize(coordTypeSize), _alignment(alignment),
			_bVisible(false), _bEnabled(true), _bDrawable(bDrawable), _nofChildren(0)
		{
			for (int i = 0; i < MaxNofChildren; i++)
			{
				_arChildren[i] = nullptr;
			}

			// The data below will be automatically calculated
			// after the parent has initialised _sizeOfParent and _globPosOfParent.
			// 
			//_sizePix	  = recalcSizePix();
			//_locPosPix  = recalcLocPosPix();
		}

		// use constructor delegation to create a default constructor.
		Widget() : Widget("", Vec2(0,0), CoordType::Pixels, Vec2(0,0), CoordType::Pixels, Alignment::TopLeft)
		{
		}	
		
		// convenience function to set most properties at once.
		void setWidgetProps(const char* strName, const Vec2& locPos, CoordType coordTypeLocPos, 
			   const Vec2& size, CoordType coordTypeSize, Alignment alignment)
		{
			_strName = strName;
			_locPos = locPos;
			_coordTypeLocPos = coordTypeLocPos;
			_size = size;
			_coordTypeSize = coordTypeSize;
			_alignment = alignment;

			// The data below will be automatically calculated
			// after the parent has initialised _sizeOfParent and _globPosOfParent.
			// 
			//_sizePix	  = recalcSizePix();
			//_locPosPix  = recalcLocPosPix();
		}

		/* override */ virtual WidgetType getWidgetType()
		{
			return WidgetType::Widget;
		}

		/* override */  virtual void setName(const char* str)
		{
			_strName = str;
		}

		/* override */  virtual const char* getName()
		{
			return _strName;
		}

		///*override*/ virtual bool addChildWidgets(IWidget* arWidgets[])
		//{
		//	bool bOk = true;
		//	int nofWidgets = sizeof(arWidgets) / sizeof(IWidget&);
		//	for (int i = 0; i < nofWidgets; i++)
		//	{
		//		bOk = addChildWidget(*arWidgets[i]);
		//		if (!bOk)
		//		{
		//			break;
		//		}
		//	}
		//	return bOk;
		//}

		// build a tree from back to forth: root is drawn first.
		/*override*/ bool addChildWidget(IWidget& widget)
		{
			if (_nofChildren == MaxNofChildren)
			{
				ESP_LOGI("no place for an additional child widget", "%s  !!!!!!!!!!", this->getName());
				vTaskDelay(100);
				return false;
			}
			_arChildren[_nofChildren++] = &widget;
			widget.setParent(this);

			//ESP_LOGI("this childwidget will add another:", "%s", this->getName());

			if (_display == nullptr)
			{
				ESP_LOGI("addchildwidget", "%s  witout display?!", widget.getName());
				vTaskDelay(500);
				vTaskDelay(500);
				vTaskDelay(500);
				vTaskDelay(500);
			}
			else
			{
				;
			//	ESP_LOGI("addchildwidget", "%s   with display", widget.getName());
			//	vTaskDelay(100);
			}
			assert(_display != nullptr);
			widget.setDisplay(*_display);
			widget.setSizeOfParent(_sizePix);						    // pass down own size
			widget.setGlobPosOfParent(_globPosOfParent + _locPosPix); // pass down own globpos

			return true;
		}

		/*override*/ IWidget* getParent()
		{
			return _pParent;
		}

		/*override*/ void setParent(IWidget* pParent)
		{
			_pParent = pParent;
		}

		// The widget at the top of the tree - normally the PageRoot. Repainting a
		// region has to start there, because only then do overlapping siblings
		// get redrawn in the right painter's order.
		IWidget* getRoot()
		{
			IWidget* pRoot = this;
			while (pRoot->getParent() != nullptr)
			{
				pRoot = pRoot->getParent();
			}
			return pRoot;
		}

		/*override*/ IDisplay* getDisplay()
		{
			return _display;
		}
		
		/*override*/ virtual void setDisplay(IDisplay& display)
		{
			_display = &display;

			// Do it recursively, allowing a page to be moved from
			// one display to another.
			for (int i = 0; i < _nofChildren; i++)
			{
				_arChildren[i]->setDisplay(display);
			}
		}

		// always in pixels. always in abs.
		/*override*/ Vec2 getGlobPosOfParent()
		{
			return _globPosOfParent;
		}

		// This function is normally called by the parent widget.
		// It thus passes down its absolute coordinates in pixels wrt the top left of the screen.
		/*override*/ void setGlobPosOfParent(const Vec2& globPosOfParent)
		{
			if (_globPosOfParent != globPosOfParent)
			{
				_globPosOfParent = globPosOfParent;
				_locPosPix = recalcLocPosPix();
				updateNewGlobPosToChildren();
			}
		}

		// always in pixels
		/*override*/ Vec2 getSizeOfParent()
		{
			return _sizeOfParent;
		}

		// This function is normally called by the parent widget.
		// It thus passes down its absolute coordinates in pixels wrt the top left of the screen.
		/*override*/ virtual void setSizeOfParent(const Vec2& sizeOfParent)
		{
			if (_sizeOfParent != sizeOfParent)
			{
				_sizeOfParent = sizeOfParent;

				Vec2 newSizePix   = recalcSizePix();
				Vec2 newLocPosPix = recalcLocPosPix();

				if (newSizePix != _sizePix)
				{
					_sizePix = newSizePix;
					updateNewSizeToChildren();
				}

				if (newLocPosPix != _locPosPix)
				{
					_locPosPix = newLocPosPix;
					updateNewGlobPosToChildren();
				}
			}
		}

		// CoordType can be Pixels or Promillage
		/*override*/ Vec2 getLocPos()
		{
			return _locPos;
		}

		// CoordType can be Pixels or Promillage
		/*override*/ void setLocPos(const Vec2& locPos, const CoordType coordTypeLocPos, const Alignment alignment)
		{
			_alignment = alignment;
			_coordTypeLocPos = coordTypeLocPos;
			_locPos = locPos;

			Vec2 newLocPosPix = recalcLocPosPix();

			if (newLocPosPix != _locPosPix)
			{
				_locPosPix = newLocPosPix;
				updateNewGlobPosToChildren();
			}
		}

		// CoordType can be Pixels or Promillage
		/*override*/ Vec2 getSize()
		{
			return _size;
		}

		// CoordType can be Pixels or Promillage
		/*override*/ void setSize(const Vec2& size, const CoordType coordTypeSize)
		{
			_size = size;
			
			Vec2 newSizePix = recalcSizePix();
			
			if (newSizePix != _sizePix)
			{
				_sizePix = newSizePix;
				updateNewSizeToChildren();
			}
		}

		/*override*/ Vec2 getLocPosPix()
		{
			return _locPosPix;
		}

		/*override*/ Vec2 getSizePix()
		{
			return _sizePix;
		}

		/*override*/ Alignment getAlignment()
		{
			return _alignment;
		}

		// show immediately draws it.
		// note: WidgetProps only serves as a node in the display-tree.
		//       It conains the props and links to children.
		//       The actual (un-)display functionality is to be implemented
		//       by the specific widget class (like TouchScreenButton),
		//       which is derived from IWidget as well.
		// show implies enabling.
		/*override*/ virtual void show(bool bIncludeChildren)
		{
			_bVisible = true;
			this->enable(false);	// true would cause a needless avalanche: recursively below, the show of every child already calls enable.
			if (bIncludeChildren)
			{
				for (int i = 0; i < _nofChildren; i++)
				{
					_arChildren[i]->show(true);
				}
			}
		}

		// hide immediately undraws it.
		// hide implies disabling.
		/*override*/ virtual void hide(bool bIncludeChildren)
		{
			_bVisible = false;
			this->disable(false);	// true would cause a needless avalanche: recursively below, the show of every child already calls disable.
			if (bIncludeChildren)
			{
				for (int i = 0; i < _nofChildren; i++)
				{
					_arChildren[i]->hide(true);
				}
			}
		}

		// A plain Widget has nothing of its own to draw; Panel and Text override this.
		/*override*/ virtual void paint()
		{
		}

		/*override*/ void redrawRegion(const Vec2& topLeftPix, const Vec2& sizePix)
		{
			if (!_bVisible)
			{
				return;
			}

			// Cull whole subtrees that fall outside the region. The clip rect is
			// what keeps the remaining ones inside it pixel-wise; this only saves
			// the drawing commands.
			// Assumes children stay within their parent's rect, which is how the
			// tree is laid out.
			if (!rectsIntersect(getGlobPosPix(), _sizePix, topLeftPix, sizePix))
			{
				return;
			}

			paint();

			for (int i = 0; i < _nofChildren; i++)
			{
				_arChildren[i]->redrawRegion(topLeftPix, sizePix);
			}
		}

		// Moves this widget, and repairs the screen where it used to be.
		//
		// Deliberately NOT hide() + setLocPos() + show(): that erases the whole
		// widget before redrawing it (which is what flickers), it fills the old
		// spot with the widget's own _colBg rather than restoring what was
		// underneath, and hide/show would toggle _bVisible and _bEnabled - so the
		// touch handling would blink on and off during an animation.
		void moveTo(const Vec2& newLocPos, CoordType coordTypeLocPos)
		{
			Vec2 oldTopLeftPix = getGlobPosPix();
			Vec2 oldSizePix = _sizePix;

			setLocPos(newLocPos, coordTypeLocPos, _alignment);   // moves, draws nothing

			if (!_bVisible || (_display == nullptr))
			{
				return;
			}

			Vec2 newTopLeftPix = getGlobPosPix();

			// Everything that was covered before but is not covered now, as up to
			// four strips. For a straight horizontal or vertical slide only one of
			// them is non-empty.
			int32_t ox0 = oldTopLeftPix.x, oy0 = oldTopLeftPix.y;
			int32_t ox1 = ox0 + oldSizePix.x, oy1 = oy0 + oldSizePix.y;
			int32_t nx0 = newTopLeftPix.x, ny0 = newTopLeftPix.y;
			int32_t nx1 = nx0 + _sizePix.x, ny1 = ny0 + _sizePix.y;

			if (!rectsIntersect(oldTopLeftPix, oldSizePix, newTopLeftPix, _sizePix))
			{
				repaintBehind(Vec2(ox0, oy0), oldSizePix);
			}
			else
			{
				int32_t yTop = (ny0 < oy1) ? ny0 : oy1;			// min
				int32_t yBottom = (ny1 > oy0) ? ny1 : oy0;		// max
				repaintBehind(Vec2(ox0, oy0), Vec2(ox1 - ox0, yTop - oy0));			// above
				repaintBehind(Vec2(ox0, yBottom), Vec2(ox1 - ox0, oy1 - yBottom));	// below
				int32_t xLeft = (nx0 < ox1) ? nx0 : ox1;
				int32_t xRight = (nx1 > ox0) ? nx1 : ox0;
				repaintBehind(Vec2(ox0, yTop), Vec2(xLeft - ox0, yBottom - yTop));		// left
				repaintBehind(Vec2(xRight, yTop), Vec2(ox1 - xRight, yBottom - yTop));	// right
			}

			// And draw myself at the new position. No need to go through the root
			// here - whatever this widget covers, it covers.
			Vec2 topLeftPix = newTopLeftPix;
			Vec2 sizePix = _sizePix;
			if (clipToRoot(topLeftPix, sizePix))
			{
				_display->setClipRect(topLeftPix, sizePix);
				redrawRegion(topLeftPix, sizePix);
				_display->clearClipRect();
			}
		}

	private:
		// Lets the whole tree repaint one region, so that what was underneath
		// this widget comes back rather than being guessed at with a fill colour.
		void repaintBehind(const Vec2& topLeftPixIn, const Vec2& sizePixIn)
		{
			Vec2 topLeftPix = topLeftPixIn;
			Vec2 sizePix = sizePixIn;
			if (!clipToRoot(topLeftPix, sizePix))
			{
				return;
			}

			_display->setClipRect(topLeftPix, sizePix);
			getRoot()->redrawRegion(topLeftPix, sizePix);
			_display->clearClipRect();
		}

		// Snijdt een regio af op de rect van de root, en meldt of er iets van
		// overblijft. Wat daarbuiten valt hoeft niet getekend te worden; dat
		// scheelt tijdens het inschuiven van een widget die nog grotendeels
		// buiten beeld hangt een groot deel van de tekencommando's.
		//
		// Bewust de root en niet IDisplay::getScreenSize(). De hele layout is op
		// de root gebaseerd, dus dit is per definitie consistent met waar de
		// widgets staan - ook als de root ooit kleiner dan het scherm zou zijn.
		// (getScreenSize() gaf hier ooit de verwisselde maat, waardoor een kwart
		// van de breedte werd weggeknipt; dat is inmiddels in de adapter
		// opgelost, maar de root blijft de juiste bron.)
		bool clipToRoot(Vec2& topLeftPix, Vec2& sizePix)
		{
			if ((sizePix.x <= 0) || (sizePix.y <= 0))
			{
				return false;
			}

			IWidget* pRoot = getRoot();
			Vec2 rootTopLeftPix = pRoot->getGlobPosPix();
			Vec2 rootSizePix = pRoot->getSizePix();

			int32_t x0 = topLeftPix.x, y0 = topLeftPix.y;
			int32_t x1 = x0 + sizePix.x, y1 = y0 + sizePix.y;

			if (x0 < rootTopLeftPix.x) { x0 = rootTopLeftPix.x; }
			if (y0 < rootTopLeftPix.y) { y0 = rootTopLeftPix.y; }
			if (x1 > rootTopLeftPix.x + rootSizePix.x) { x1 = rootTopLeftPix.x + rootSizePix.x; }
			if (y1 > rootTopLeftPix.y + rootSizePix.y) { y1 = rootTopLeftPix.y + rootSizePix.y; }

			if ((x1 <= x0) || (y1 <= y0))
			{
				return false;
			}

			topLeftPix.set(x0, y0);
			sizePix.set(x1 - x0, y1 - y0);
			return true;
		}

	public:
		/*override*/ bool isShown()
		{
			return _bVisible;
		}

		// false: widget takes part in the tree and input handling,
		// but never sends drawing commands to the display.
		/*override*/ bool isDrawable()
		{
			return _bDrawable;
		}

		// enable/disable is about the functional aspect of 
		// note: WidgetProps only serves as a node in the display-tree.
		//       It conains the props and links to children.
		//       The actual enable/disable functionality of the related
		//       widget class (like TouchScreenButton) must be implemented
		//       by that widget class itself (which is derived from IWidget as well).
		/*override*/ void enable(bool bIncludeChildren)
		{
			_bEnabled = true;
			if (bIncludeChildren)
			{
				for (int i = 0; i < _nofChildren; i++)
				{
					_arChildren[i]->enable(true);
				}
			}
		}

		// a widget: wether it continues to responde to input,
		/*override*/ void disable(bool bIncludeChildren)
		{
			_bEnabled = false;
			if (bIncludeChildren)
			{
				for (int i = 0; i < _nofChildren; i++)
				{
					_arChildren[i]->disable(true);
				}
			}
		}

		// for instance.
		/*override*/ bool isEnabled()
		{
			return _bEnabled;
		}

		/*override*/ Vec2 getGlobPosPix()
		{
			Vec2 globPosPix = _globPosOfParent + _locPosPix;
			return globPosPix;
		}

		/*override*/ CoordType getCoordTypeLocPos()
		{
			return _coordTypeLocPos;
		}
		
		/*override*/  CoordType getCoordTypeSize()
		{
			return _coordTypeSize;
		}

	private:
		Vec2 recalcLocPosPix()
		{
			Vec2 newLocPosPix;
			if (_coordTypeLocPos == CoordType::Pixels)
			{
				newLocPosPix = _locPos;
			}
			else
			{
				// Note: sizeOfParent is in pix.
				// size is in Promillages, in this case.
				newLocPosPix = (_locPos * _sizeOfParent) / 1000;
			}
			return newLocPosPix;
		}

		Vec2 recalcSizePix()
		{
			Vec2 newSizePix;
			if (_coordTypeSize == CoordType::Pixels)
			{
				newSizePix = _size;
			}
			else
			{
				// Note: sizeOfParent is in pix.
				// size is in Promillages, in this case.
				newSizePix = (_size * _sizeOfParent) / 1000;
			}
			return newSizePix;
		}

		void updateNewGlobPosToChildren()
		{
			//Vec2 newGlobPosPix = _globPosOfParent + _locPosPix;
			Vec2 newGlobPosPix = getGlobPosPix();
			for (int i = 0; i < _nofChildren; i++)
			{
				_arChildren[i]->setGlobPosOfParent(newGlobPosPix);
			}
		}

		void updateNewSizeToChildren()
		{
			for (int i = 0; i < _nofChildren; i++)
			{
				_arChildren[i]->setSizeOfParent(_sizePix);
			}
		}
	};
};
