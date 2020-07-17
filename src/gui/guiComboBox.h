// Copyright (C) 2002-2012 Nikolaus Gebhardt, Modified by Mustapha Tachouct
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef GUICOMBOBOX_HEADER
#define GUICOMBOBOX_HEADER

#include "irrlichttypes_extrabloated.h"
#include <IGUIComboBox.h>
#include <IGUIStaticText.h>
#include "guiListBox.h"
#include "guiButton.h"
#include <irrString.h>
#include <irrArray.h>

using namespace irr;
using namespace irr::gui;

class IGUIButton;
class GUIListBox;
class ISimpleTextureSource;

//! Single line edit box for editing simple text.
class GUIComboBox : public IGUIComboBox
{
public:

	//! constructor
	GUIComboBox(IGUIEnvironment *environment, IGUIElement *parent,
		s32 id, core::rect<s32> rectangle, ISimpleTextureSource *tsrc);

	//! destructor
	virtual ~GUIComboBox();

	//! Returns amount of items in box
	virtual u32 getItemCount() const;

	//! returns string of an item. the idx may be a value from 0 to itemCount-1
	virtual const wchar_t* getItem(u32 idx) const;

	//! Returns item data of an item. the idx may be a value from 0 to itemCount-1
	virtual u32 getItemData(u32 idx) const;

	//! Returns index based on item data
	virtual s32 getIndexForItemData(u32 data) const;

	//! adds an item and returns the index of it
	virtual u32 addItem(const wchar_t *text, u32 data = 0);

	//! Removes an item from the combo box.
	virtual void removeItem(u32 id);

	//! deletes all items in the combo box
	virtual void clear();

	//! returns the text of the currently selected item
	virtual const wchar_t* getText() const;

	//! returns id of selected item. returns -1 if no item is selected.
	virtual s32 getSelected() const;

	//! sets the selected item. Set this to -1 if no item should be selected
	virtual void setSelected(s32 idx);

	//! sets the text alignment of the text part
	virtual void setTextAlignment(EGUI_ALIGNMENT horizontal, EGUI_ALIGNMENT vertical);

	//! Set the maximal number of rows for the selection listbox
	virtual void setMaxSelectionRows(u32 max);

	//! Get the maximimal number of rows for the selection listbox
	virtual u32 getMaxSelectionRows() const;

	//! called if an event happened.
	virtual bool OnEvent(const SEvent &event);

	//! draws the element and its children
	virtual void draw();

	//! Change the selected item color
	virtual void setSelectedItemColor(const video::SColor &color);

	//! Change the background color
	virtual void setBackgroundColor(const video::SColor &color);

	//! Change the button color
	virtual void setButtonColor(const video::SColor &color); // :PATCH:

	//! returns a color
	virtual video::SColor getColor(EGUI_DEFAULT_COLOR color) const; // :PATCH:

	//! sets a color
	virtual void setColor(EGUI_DEFAULT_COLOR which, video::SColor newColor,
			f32 shading=1.0f); // :PATCH:

	//! Writes attributes of the element.
	virtual void serializeAttributes(io::IAttributes *out, io::SAttributeReadWriteOptions *options) const;

	//! Reads attributes of the element
	virtual void deserializeAttributes(io::IAttributes *in, io::SAttributeReadWriteOptions *options);

private:

	void openCloseMenu();
	void sendSelectionChangedEvent();

	GUIButton *m_list_button;
	irr::gui::IGUIStaticText *m_selected_text;
	GUIListBox *m_listbox;
	irr::gui::IGUIElement *m_last_focus;


	struct SComboData
	{
		SComboData(const wchar_t *text, u32 _data)
			: name(text), data(_data) {}

		core::stringw name;
		u32 data;
	};
	std::vector<SComboData> m_items;

	s32 m_selected;
	EGUI_ALIGNMENT m_halign, m_valign;
	u32 m_max_selection_rows;
	bool m_has_focus;

	bool m_bg_color_used;
	video::SColor m_bg_color;

	bool m_selected_item_color_used;
	video::SColor m_selected_item_color;

	video::SColor* Colors; // :PATCH:

	ISimpleTextureSource *TSrc;
};



#endif // GUICOMBOBOX_HEADER
