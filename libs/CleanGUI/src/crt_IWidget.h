// by Marius Versteegen, 2024

#pragma once
#include "crt_typesAndConstants.h"

// Zegt dat show, enable en tekenen drie losse assen zijn (zie IWidget
// hieronder). Bedoeld voor code die tegen zowel de oude als de nieuwe API moet
// compileren - het voorbeeld ShowEnableSemantics doet dat, en meet daarmee het
// verschil. Nieuwe code hoeft er niet naar te kijken.
#define CLEANGUI_HAS_DRAW_ENABLE_SPLIT 1

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
		// Draws just this widget, nothing else. Panel and Text implement it;
		// a plain Widget has nothing of its own to draw.
		virtual void paint() = 0;

		// Draws this widget - and, if asked, its children - right now, over its
		// whole rect. Touches no flag at all: a widget does not become drawable
		// or touchable by being redrawn.
		//
		// That is the whole point of it existing next to show(). A widget that
		// is being animated, or a page that is being refreshed, must not have
		// its touch handling switched back on as a side effect of being drawn.
		//
		// A non-drawable widget draws nothing, and neither do its children: the
		// subtree is cut off here.
		virtual void redraw(bool bIncludeChildren) = 0;

		// Redraws this widget and its children, but only those that overlap the
		// given region. Combine with IDisplayDraw::setClipRect to repaint just a
		// part of the screen - for instance the strip a widget vacated when it
		// moved, which is how whatever was underneath comes back.
		virtual void redrawRegion(const Vec2& topLeftPix, const Vec2& sizePix) = 0;

	// The three axes. They are deliberately independent: "do I belong to the
	// picture", "am I touchable", and "draw now" are three different questions,
	// and answering one of them used to answer the other two.
	//
	//   makeDrawable / makeNonDrawable   the flag. Draws nothing, erases nothing.
	//   enable / disable                 touch handling. Draws nothing.
	//   redraw                           draws. Touches no flag.
	//
	// show() and hide() are the two combinations that are wanted often enough to
	// have a name; they live on Widget itself, because they are nothing but
	// makeDrawable + redraw and makeNonDrawable + repaintRegion.
		virtual void makeDrawable(bool bIncludeChildren) = 0;

		// false: the widget still takes part in the tree and in input handling,
		// but never sends drawing commands to the display.
		// Mind that this draws nothing by itself - neither the widget nor what
		// it covered. Repaint the region it occupied afterwards, or use hide().
		// Almost always pass bIncludeChildren = true: a button whose panel is
		// not drawable but whose label still is, would draw a floating label.
		virtual void makeNonDrawable(bool bIncludeChildren) = 0;
		virtual bool isDrawable() = 0;

		virtual void enable(bool bIncludeChildren) = 0;
		virtual void disable(bool bIncludeChildren) = 0;
		virtual bool isEnabled() = 0;
	};
};
