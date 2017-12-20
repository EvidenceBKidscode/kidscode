// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __GUI_SCROLL_BAR_H_INCLUDED__
#define __GUI_SCROLL_BAR_H_INCLUDED__

#include "IrrCompileConfig.h"
#ifdef _IRR_COMPILE_WITH_GUI_

#include "IGUIScrollBar.h"
#include "IGUIButton.h"
#include "guiSkin.h"
#include "guiButton.h"

namespace irr
{
namespace gui
{

	class GUIScrollBar : public IGUIScrollBar
	{
	public:

		//! constructor
		GUIScrollBar(IGUIEnvironment* environment, bool horizontal, 
			IGUIElement* parent, s32 id, core::rect<s32> rectangle,
			bool noclip=false);

		//! destructor
		virtual ~GUIScrollBar();

		//! called if an event happened.
		virtual bool OnEvent(const SEvent& event);

		//! draws the element and its children
		virtual void draw();

		virtual void OnPostRender(u32 timeMs);


		//! gets the maximum value of the scrollbar.
		virtual s32 getMax() const;

		//! sets the maximum value of the scrollbar.
		virtual void setMax(s32 max);

		//! gets the minimum value of the scrollbar.
		virtual s32 getMin() const;

		//! sets the minimum value of the scrollbar.
		virtual void setMin(s32 min);

		//! gets the base step value
		virtual s32 getBaseStep() const; // :PATCH:

		//! sets the base step value
		virtual void setBaseStep(s32 step); // :PATCH:
		
		//! gets the small step value
		virtual s32 getSmallStep() const;

		//! sets the small step value
		virtual void setSmallStep(s32 step);

		//! gets the large step value
		virtual s32 getLargeStep() const;

		//! sets the large step value
		virtual void setLargeStep(s32 step);

		//! gets the current position of the scrollbar
		virtual s32 getPos() const;

		//! sets the position of the scrollbar
		virtual void setPos(s32 pos);
		
		//! returns a color
		virtual video::SColor getColor(EGUI_DEFAULT_COLOR color) const; // :PATCH:

		//! sets a color
		virtual void setColor(EGUI_DEFAULT_COLOR which, video::SColor newColor,
				f32 shading=1.0f); // :PATCH:

		//! updates the rectangle
		virtual void updateAbsolutePosition();

		//! Writes attributes of the element.
		virtual void serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options) const;

		//! Reads attributes of the element
		virtual void deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options);

		//! adds a scrollbar. The returned pointer must not be dropped.
		static GUIScrollBar* addScrollBar(IGUIEnvironment *environment, 
			bool horizontal, const core::rect<s32>& rectangle, IGUIElement* parent=0, s32 id=-1);
			
	private:

		void refreshControls();
		s32 getPosFromMousePos(const core::position2di &p) const;

		gui::GUIButton* UpButton;
		gui::GUIButton* DownButton;

		core::rect<s32> SliderRect;

		bool Dragging;
		bool Horizontal;
		bool DraggedBySlider;
		bool TrayClick;
		s32 Pos;
		s32 DrawPos;
		s32 DrawHeight;
		s32 Min;
		s32 Max;
		s32 BaseStep; // :PATCH:
		s32 SmallStep;
		s32 LargeStep;
		s32 DesiredPos;
		u32 LastChange;
		video::SColor CurrentIconColor;
		video::SColor* Colors; // :PATCH:

		f32 range () const { return (f32) ( Max - Min ); }
	};

} // end namespace gui
} // end namespace irr

#endif // _IRR_COMPILE_WITH_GUI_

#endif

