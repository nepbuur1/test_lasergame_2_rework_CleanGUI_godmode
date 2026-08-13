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
		bool _bEnabled;
		bool _bDrawable;

	protected: // Protected, to support easy enumeration by derived classes.
		// Note the (MaxNofChildren > 0) in every loop over _arChildren below.
		// It costs nothing - the template parameter makes it a compile time
		// constant - but without it, gcc sees an index into a zero-length array in
		// every Widget<0> instantiation and stops with -Werror=array-bounds.
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
			_bEnabled(true), _bDrawable(bDrawable), _nofChildren(0)
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
			for (int i = 0; (MaxNofChildren > 0) && (i < _nofChildren); i++)
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
			_coordTypeSize = coordTypeSize;		// was silently ignored, so a widget
												// built in promillage read pixels as
												// promillage and came out tiny.
			
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

		// Draws this widget, and its children if asked, over its whole rect.
		//
		// Touches no flag: not _bDrawable, not _bEnabled. That is what separates
		// it from show(), and it is the reason it exists. A page that refreshes
		// itself, or a widget that is being animated, must not have its touch
		// handling switched back on just because it was drawn - and before this
		// existed there was no other way to draw, so it always was.
		//
		// Not drawable means: draw nothing, and cut the subtree off here. Same
		// rule as redrawRegion, so a widget cannot be drawn by one route and
		// skipped by the other.
		//
		// Does not clip. That is deliberate and carries no risk: drawing outside
		// the screen is routine in these apps (a keyboard parked at
		// y = screenHeight), the adapter always has a viewport, and every
		// TFT_eSPI primitive starts by testing it.
		/*override*/ virtual void redraw(bool bIncludeChildren)
		{
			if (!_bDrawable)
			{
				return;
			}

			paint();

			if (bIncludeChildren)
			{
				for (int i = 0; (MaxNofChildren > 0) && (i < _nofChildren); i++)
				{
					_arChildren[i]->redraw(true);
				}
			}
		}

		// The two combinations that are wanted often enough to have a name.
		// Neither is virtual, and neither needs to be: everything they do
		// happens through calls that already dispatch by themselves.
		//
		// Note what show() does NOT do any more: it does not enable. Nor does
		// hide() disable. Those used to come along for free, which meant there
		// was no way to redraw a page without resurrecting every button on it -
		// and no way to hide one without silently dropping a press that was in
		// progress. If a page must also become touchable, say so: enable(true).
		void show(bool bIncludeChildren)
		{
			makeDrawable(bIncludeChildren);
			redraw(bIncludeChildren);
		}

		// Undraws it: what was underneath comes back, because the repaint runs
		// from the root down. Not a fill with this widget's own background
		// colour - that guesses at what belongs there, and guesses wrong as soon
		// as anything else has drawn in that spot.
		//
		// Mind that this therefore DOES send drawing commands. A widget that
		// must disappear without any drawing at all - because another task is
		// already painting over that area - wants makeNonDrawable() instead.
		void hide(bool bIncludeChildren)
		{
			Vec2 topLeftPix = getGlobPosPix();
			Vec2 sizePix = _sizePix;

			makeNonDrawable(bIncludeChildren);
			repaintRegion(topLeftPix, sizePix);
		}

		// A plain Widget has nothing of its own to draw; Panel and Text override this.
		/*override*/ virtual void paint()
		{
		}

		// Does paint() fill every pixel of this widget's own rect?
		// If it does, this widget can be redrawn on its own and whatever is behind
		// it does not matter. If it does not - a transparent image, rounded
		// corners, text - then what is behind it has to be repainted first, and
		// that has to go through the root. See moveTo().
		// False by default: a plain Widget draws nothing at all, and guessing
		// "covered" wrongly leaves debris on the screen, while guessing
		// "not covered" wrongly only costs a little time.
		virtual bool coversOwnRect()
		{
			return false;
		}

		/*override*/ void redrawRegion(const Vec2& topLeftPix, const Vec2& sizePix)
		{
			// Cut off whole subtrees that are not part of the picture. This used
			// to test _bVisible, and that never fired: one page.show(true) set it
			// on the entire tree, so a subtree that had been switched off was
			// still walked node by node, each one deciding for itself that it had
			// nothing to draw. _bDrawable says the same thing honestly and says
			// it at the root of the subtree.
			if (!_bDrawable)
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

			for (int i = 0; (MaxNofChildren > 0) && (i < _nofChildren); i++)
			{
				_arChildren[i]->redrawRegion(topLeftPix, sizePix);
			}
		}

		// Moves this widget, and repairs the screen where it used to be.
		//
		// Deliberately NOT hide() + setLocPos() + show(): that erases the whole
		// widget before redrawing it (which is what flickers), and it would
		// repaint the vacated spot twice over.
		void moveTo(const Vec2& newLocPos, CoordType coordTypeLocPos)
		{
			Vec2 oldTopLeftPix = getGlobPosPix();
			Vec2 oldSizePix = _sizePix;

			setLocPos(newLocPos, coordTypeLocPos, _alignment);   // moves, draws nothing

			if (!_bDrawable || (_display == nullptr))
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
				repaintRegion(Vec2(ox0, oy0), oldSizePix);
			}
			else
			{
				int32_t yTop = (ny0 < oy1) ? ny0 : oy1;			// min
				int32_t yBottom = (ny1 > oy0) ? ny1 : oy0;		// max
				repaintRegion(Vec2(ox0, oy0), Vec2(ox1 - ox0, yTop - oy0));			// above
				repaintRegion(Vec2(ox0, yBottom), Vec2(ox1 - ox0, oy1 - yBottom));	// below
				int32_t xLeft = (nx0 < ox1) ? nx0 : ox1;
				int32_t xRight = (nx1 > ox0) ? nx1 : ox0;
				repaintRegion(Vec2(ox0, yTop), Vec2(xLeft - ox0, yBottom - yTop));		// left
				repaintRegion(Vec2(xRight, yTop), Vec2(ox1 - xRight, yBottom - yTop));	// right
			}

			// And draw myself at the new position.
			if (coversOwnRect())
			{
				// Whatever this widget covers, it covers: no need to go through
				// the root, so the background is not touched at all.
				Vec2 topLeftPix = newTopLeftPix;
				Vec2 sizePix = _sizePix;
				if (clipToRoot(topLeftPix, sizePix))
				{
					_display->setClipRect(topLeftPix, sizePix);
					redrawRegion(topLeftPix, sizePix);
					_display->clearClipRect();
				}
			}
			else
			{
				// This widget leaves part of its own rect untouched - a
				// transparent image is the obvious case. Drawing only itself
				// would leave the previous frame showing through those pixels,
				// which for a moving sprite means a trail of itself. So the whole
				// tree paints this rect, bottom up.
				repaintRegion(newTopLeftPix, _sizePix);
			}
		}

	public:
		// Lets the whole tree repaint one region of the screen, clipped to it, so
		// that whatever belongs there comes back rather than being guessed at with
		// a fill colour. Widgets outside the region are skipped entirely.
		//
		// This is what makes a partial update honest, and it is worth reaching for
		// whenever only part of the screen changes: a full screen redraw moves
		// 153.600 bytes over the SPI bus and costs ~31 ms, no matter how little
		// actually differs.
		//
		// The region is in absolute screen pixels. Which widget you call it on
		// does not matter - it always redraws through the root.
		void repaintRegion(const Vec2& topLeftPixIn, const Vec2& sizePixIn)
		{
			// A widget that was never added to a tree has no display. That used
			// to be unreachable here; since hide() goes through this function it
			// no longer is, and a page that is hidden before it is built would
			// dereference a null pointer.
			if (_display == nullptr)
			{
				return;
			}

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

	private:
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
		// false: widget takes part in the tree and input handling,
		// but never sends drawing commands to the display.
		/*override*/ bool isDrawable()
		{
			return _bDrawable;
		}

		// Says that this widget belongs to the picture again. Draws nothing -
		// it is a flag, not an instruction. redraw() or show() puts it on the
		// screen.
		/*override*/ void makeDrawable(bool bIncludeChildren)
		{
			_bDrawable = true;
			if (bIncludeChildren)
			{
				for (int i = 0; (MaxNofChildren > 0) && (i < _nofChildren); i++)
				{
					_arChildren[i]->makeDrawable(true);
				}
			}
		}

		// Says that it does not. Use it for a widget that is only of use part of
		// the time: it then stays out of redraw(), out of repaintRegion() and out
		// of the tree's drawing altogether, without being removed from it.
		//
		// Erases nothing - neither this widget nor what it covered. Repaint the
		// region it occupied afterwards, or use hide(), which is exactly this
		// plus that repaint.
		//
		// Almost always pass bIncludeChildren = true. Passing false switches off
		// only this node, and since a non-drawable node cuts its whole subtree
		// out of the drawing anyway, the children stay behind marked drawable
		// while nothing of them appears.
		/*override*/ void makeNonDrawable(bool bIncludeChildren)
		{
			_bDrawable = false;
			if (bIncludeChildren)
			{
				for (int i = 0; (MaxNofChildren > 0) && (i < _nofChildren); i++)
				{
					_arChildren[i]->makeNonDrawable(true);
				}
			}
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
				for (int i = 0; (MaxNofChildren > 0) && (i < _nofChildren); i++)
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
				for (int i = 0; (MaxNofChildren > 0) && (i < _nofChildren); i++)
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
			for (int i = 0; (MaxNofChildren > 0) && (i < _nofChildren); i++)
			{
				_arChildren[i]->setGlobPosOfParent(newGlobPosPix);
			}
		}

		void updateNewSizeToChildren()
		{
			for (int i = 0; (MaxNofChildren > 0) && (i < _nofChildren); i++)
			{
				_arChildren[i]->setSizeOfParent(_sizePix);
			}
		}
	};
};
