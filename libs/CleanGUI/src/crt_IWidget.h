// by Marius Versteegen, 2024

#pragma once
#include "crt_typesAndConstants.h"

namespace crt
{
	enum class WidgetType :int
	{
		None = 0, Widget = 1, PageRoot = 2, Panel = 3, Text = 4,
		TouchscreenButton = 5, TouchscreenButtonGroupPanel = 6, TouchscreenButtonGroup = 7 // TouchscreenButtonGroup is to be discarded / obsoleted.
	};

	class IWidget
	{
	public:
	// relation to world
		virtual bool addChildWidget(IWidget& widget) = 0;        // build a tree from back to forth: root is drawn first.
		virtual WidgetType getWidgetType() = 0;
		virtual void setName(const char* str) = 0;
		virtual const char* getName() = 0;
		virtual IDisplay* getDisplay() = 0;
		virtual void setDisplay (IDisplay& display) = 0;
		virtual IWidget* getParent() = 0;
		virtual void setParent(IWidget* pParent) = 0;
		virtual Vec2 getGlobPosOfParent() = 0;					  // always in pixels. always in abs
		virtual void setGlobPosOfParent(const Vec2& locPos) = 0;  // coordinates with origin top left.
		virtual Vec2 getSizeOfParent() = 0;					      // always in pixels. always in abs
		virtual void setSizeOfParent(const Vec2& sizeOfParent) = 0;     // coordinates with origin top left.
	
	// positioning
		virtual Vec2 getLocPos() = 0;                             
		virtual void setLocPos(const Vec2& locPos, CoordType coordTypeLocPos, Alignment alignment) = 0; // CoordType can be Pixels or Promillage
		virtual CoordType getCoordTypeLocPos() = 0;
		virtual Vec2 getSizePix() = 0;
		virtual Vec2 getGlobPosPix() = 0;
		virtual Vec2 getSize() = 0;
		virtual void setSize(const Vec2& size, CoordType coordTypeSize) = 0;
		virtual CoordType getCoordTypeSize() = 0;
		virtual Alignment getAlignment() = 0;

	// drawing
		// Draws just this widget, nothing else, and without touching its
		// visibility or enable state. That last part is what separates it from
		// show(): a widget that is being moved must not toggle its touch
		// handling on every step.
		virtual void paint() = 0;

		// Redraws this widget and its children, but only those that overlap the
		// given region. Combine with IDisplayDraw::setClipRect to repaint just a
		// part of the screen - for instance the strip a widget vacated when it
		// moved, which is how whatever was underneath comes back.
		virtual void redrawRegion(const Vec2& topLeftPix, const Vec2& sizePix) = 0;

	// enabling, disabling
		virtual void show(bool bIncludeChildren) = 0;		  // show/hide is about the visual aspect of a widget.
		virtual void hide(bool bIncludeChildren) = 0;        // show immediately draws it. hide immediately undraws it.
		virtual bool isShown() = 0;
		virtual bool isDrawable() = 0;   // false: widget takes part in the tree and input handling,
		                                 // but never sends drawing commands to the display.
		virtual void enable(bool bIncludeChildren) = 0;	  // enable/disable is about the functional aspect of 
		virtual void disable(bool bIncludeChildren) = 0;   // a widget: wether it continues to responde to input,
		virtual bool isEnabled() = 0;   // for instance.
	};
};
