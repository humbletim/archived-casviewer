/** 
 * @file llpanelblockedlist.cpp
 * @brief Container for blocked Residents & Objects list
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llpanelblockedlist.h"

// library include
#include "llavatarname.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llnotificationsutil.h"
#include "llscrolllistctrl.h"

// project include
#include "llfloateravatarpicker.h"
#include "llfloatersidepanelcontainer.h"
#include "llsidetraypanelcontainer.h"

#include "llavataractions.h"
#include "llviewercontrol.h"

static LLRegisterPanelClassWrapper<LLPanelBlockedList> t_panel_blocked_list("panel_block_list_sidetray");

//
// Constants
//
const std::string BLOCKED_PARAM_NAME = "blocked_to_select";

//-----------------------------------------------------------------------------
// LLPanelBlockedList()
//-----------------------------------------------------------------------------

LLPanelBlockedList::LLPanelBlockedList()
:	LLPanel()
{
	mCommitCallbackRegistrar.add("Block.ClickPick",			boost::bind(&LLPanelBlockedList::onPickBtnClick, this));
	mCommitCallbackRegistrar.add("Block.ClickBlockByName",	boost::bind(&LLPanelBlockedList::onBlockByNameClick, this));
	mCommitCallbackRegistrar.add("Block.ClickRemove",		boost::bind(&LLPanelBlockedList::onRemoveBtnClick, this));
	// <FS:Ansariel> Profile button
	mCommitCallbackRegistrar.add("Block.ClickProfile",		boost::bind(&LLPanelBlockedList::onProfileBtnClick, this));
}

LLPanelBlockedList::~LLPanelBlockedList()
{
	LLMuteList::getInstance()->removeObserver(this);
}

BOOL LLPanelBlockedList::postBuild()
{
	mBlockedList = getChild<LLScrollListCtrl>("blocked");
	mBlockedList->setCommitOnSelectionChange(TRUE);
	// <FS:Ansariel> Profile button
	mBlockedList->setCommitCallback(boost::bind(&LLPanelBlockedList::onSelectionChanged, this));
	mBlockedList->setDoubleClickCallback(boost::bind(&LLPanelBlockedList::onProfileBtnClick, this));
	mBlockedList->sortByColumn("item_name", TRUE);
	mBlockedList->setSearchColumn(mBlockedList->getColumn("item_name")->mIndex);

	// <FS:Zi> Make sure user can go back blocked user list if it's in a skin without
	//         sidebar <Back button
	// childSetCommitCallback("back", boost::bind(&LLPanelBlockedList::onBackBtnClick, this), NULL);
	LLSideTrayPanelContainer* parent = dynamic_cast<LLSideTrayPanelContainer*>(getParent());
	if(parent)
	{
		childSetCommitCallback("back", boost::bind(&LLPanelBlockedList::onBackBtnClick, this), NULL);
		getChild<LLUICtrl>("back_button_container")->setVisible(TRUE);
	}
	else
	{
		getChild<LLUICtrl>("back_button_container")->setVisible(FALSE);
	}
	// </FS:Zi>

	LLMuteList::getInstance()->addObserver(this);
	
	refreshBlockedList();

	return LLPanel::postBuild();
}

void LLPanelBlockedList::draw()
{
	// <FS:Ansariel> Only update if selection changes
	//updateButtons();
	LLPanel::draw();
}

void LLPanelBlockedList::onOpen(const LLSD& key)
{
	if (key.has(BLOCKED_PARAM_NAME) && key[BLOCKED_PARAM_NAME].asUUID().notNull())
	{
		selectBlocked(key[BLOCKED_PARAM_NAME].asUUID());
	}
}

void LLPanelBlockedList::selectBlocked(const LLUUID& mute_id)
{
	mBlockedList->selectByID(mute_id);
	mBlockedList->scrollToShowSelected();
}

void LLPanelBlockedList::showPanelAndSelect(const LLUUID& idToSelect)
{
	// <FS:Ansariel> Optional standalone blocklist floater
	//LLFloaterSidePanelContainer::showPanel("people", "panel_block_list_sidetray", LLSD().with(BLOCKED_PARAM_NAME, idToSelect));
	if (gSavedSettings.getBOOL("FSUseStandaloneBlocklistFloater"))
	{
		LLFloaterReg::showInstance("fs_blocklist", LLSD().with(BLOCKED_PARAM_NAME, idToSelect));
	}
	else
	{
		LLFloaterSidePanelContainer::showPanel("people", "panel_block_list_sidetray", LLSD().with(BLOCKED_PARAM_NAME, idToSelect));
	}
	// </FS:Ansariel>
}


//////////////////////////////////////////////////////////////////////////
// Private Section
//////////////////////////////////////////////////////////////////////////
void LLPanelBlockedList::refreshBlockedList()
{
	mBlockedList->deleteAllItems();

	std::vector<LLMute> mutes = LLMuteList::getInstance()->getMutes();
	std::vector<LLMute>::iterator it;
	for (it = mutes.begin(); it != mutes.end(); ++it)
	{
		LLScrollListItem::Params item_p;
		item_p.enabled(TRUE);
		item_p.value(it->mID); // link UUID of blocked item with ScrollListItem
		item_p.columns.add().column("item_name").value(it->mName);//.type("text");
		item_p.columns.add().column("item_type").value(it->getDisplayType());//.type("text").width(111);

		mBlockedList->addRow(item_p, ADD_BOTTOM);
	}
}

void LLPanelBlockedList::updateButtons()
{
	bool hasSelected = NULL != mBlockedList->getFirstSelected();
	getChildView("Unblock")->setEnabled(hasSelected);
}



void LLPanelBlockedList::onBackBtnClick()
{
	LLSideTrayPanelContainer* parent = dynamic_cast<LLSideTrayPanelContainer*>(getParent());
	if(parent)
	{
		parent->openPreviousPanel();
	}
}

void LLPanelBlockedList::onRemoveBtnClick()
{
	//std::string name = mBlockedList->getSelectedItemLabel();
	//LLUUID id = mBlockedList->getStringUUIDSelectedItem();
	//LLMute mute(id, name);
	//
	//// <FS:Ansariel> Keep scroll position
	//S32 scroll_pos = mBlockedList->getScrollPos();

	//S32 last_selected = mBlockedList->getFirstSelectedIndex();
	//if (LLMuteList::getInstance()->remove(mute))
	//{
	//	// Above removals may rebuild this dialog.
	//	
	//	if (last_selected == mBlockedList->getItemCount())
	//	{
	//		// we were on the last item, so select the last item again
	//		mBlockedList->selectNthItem(last_selected - 1);
	//	}
	//	else
	//	{
	//		// else select the item after the last item previously selected
	//		mBlockedList->selectNthItem(last_selected);
	//	}
	//	// <FS:Ansariel> Only update if selection changes
	//	onSelectionChanged();

	//	// <FS:Ansariel> Keep scroll position
	//	mBlockedList->setScrollPos(scroll_pos);
	//}
	
	// <FS:Ansariel> Allow bulk removals
	S32 scroll_pos = mBlockedList->getScrollPos();
	S32 last_selected = mBlockedList->getFirstSelectedIndex();

	// Remove observer before bulk operation or it would refresh the
	// list after each removal, sending us straight into a crash!
	LLMuteList::getInstance()->removeObserver(this);

	std::vector<LLScrollListItem*> selected_items = mBlockedList->getAllSelected();
	for (std::vector<LLScrollListItem*>::iterator it = selected_items.begin(); it != selected_items.end(); it++)
	{
		std::string name = (*it)->getColumn(0)->getValue().asString();
		LLUUID id = (*it)->getUUID();
		LLMute mute(id, name);
		LLMuteList::getInstance()->remove(mute);
	}

	LLMuteList::getInstance()->addObserver(this);
	refreshBlockedList();

	if (last_selected == mBlockedList->getItemCount())
	{
		// we were on the last item, so select the last item again
		mBlockedList->selectNthItem(last_selected - 1);
	}
	else
	{
		// else select the item after the last item previously selected
		mBlockedList->selectNthItem(last_selected);
	}
	onSelectionChanged();
	mBlockedList->setScrollPos(scroll_pos);
	// </FS:Ansariel>
}

void LLPanelBlockedList::onPickBtnClick()
{
	const BOOL allow_multiple = FALSE;
	const BOOL close_on_select = TRUE;
	// <FS:Ansariel> Standalone blocklist floater
	/*LLFloaterAvatarPicker* picker = *///LLFloaterAvatarPicker::show(boost::bind(&LLPanelBlockedList::callbackBlockPicked, this, _1, _2), allow_multiple, close_on_select);

	// *TODO: mantipov: should LLFloaterAvatarPicker be closed when panel is closed?
	// old Floater dependency is not enable in panel
	// addDependentFloater(picker);

	// <FS:Ansariel> Standalone blocklist floater
	if (gSavedSettings.getBOOL("FSUseStandaloneBlocklistFloater"))
	{
		LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(boost::bind(&LLPanelBlockedList::callbackBlockPicked, this, _1, _2), allow_multiple, close_on_select);
		LLFloater* parent = dynamic_cast<LLFloater*>(getParent());
		if (parent)
		{
			parent->addDependentFloater(picker);
		}
	}
	else
	{
		LLFloaterAvatarPicker::show(boost::bind(&LLPanelBlockedList::callbackBlockPicked, this, _1, _2), allow_multiple, close_on_select);
	}
	// </FS:Ansariel>
}

void LLPanelBlockedList::onBlockByNameClick()
{
	// <FS:Ansariel> Standalone blocklist floater
	//LLFloaterGetBlockedObjectName::show(&LLPanelBlockedList::callbackBlockByName);
	if (gSavedSettings.getBOOL("FSUseStandaloneBlocklistFloater"))
	{
		LLFloaterGetBlockedObjectName* picker = LLFloaterGetBlockedObjectName::show(&LLPanelBlockedList::callbackBlockByName);
		LLFloater* parent = dynamic_cast<LLFloater*>(getParent());
		if (parent)
		{
			parent->addDependentFloater(picker);
		}
	}
	else
	{
		LLFloaterGetBlockedObjectName::show(&LLPanelBlockedList::callbackBlockByName);
	}
	// </FS:Ansariel>
}

// <FS:Ansariel> Profile button
void LLPanelBlockedList::onSelectionChanged()
{
	updateButtons();
	LLMute mute = LLMuteList::getInstance()->getMute(mBlockedList->getStringUUIDSelectedItem());
	getChildView("Profile")->setEnabled(mBlockedList->getNumSelected() == 1 && mute.mID.notNull() && mute.mType == LLMute::AGENT);
}

void LLPanelBlockedList::onProfileBtnClick()
{
	LLMute mute = LLMuteList::getInstance()->getMute(mBlockedList->getStringUUIDSelectedItem());
	if (mBlockedList->getNumSelected() == 1 && mute.mID.notNull() && mute.mType == LLMute::AGENT)
	{
		LLAvatarActions::showProfile(mute.mID);
	}
}
// </FS:Ansariel>

void LLPanelBlockedList::callbackBlockPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names)
{
	if (names.empty() || ids.empty()) return;
	LLMute mute(ids[0], names[0].getLegacyName(), LLMute::AGENT);
	LLMuteList::getInstance()->add(mute);
	showPanelAndSelect(mute.mID);
}

//static
void LLPanelBlockedList::callbackBlockByName(const std::string& text)
{
	if (text.empty()) return;

	LLMute mute(LLUUID::null, text, LLMute::BY_NAME);
	BOOL success = LLMuteList::getInstance()->add(mute);
	if (!success)
	{
		LLNotificationsUtil::add("MuteByNameFailed");
	}
}

//////////////////////////////////////////////////////////////////////////
//			LLFloaterGetBlockedObjectName
//////////////////////////////////////////////////////////////////////////

// Constructor/Destructor
LLFloaterGetBlockedObjectName::LLFloaterGetBlockedObjectName(const LLSD& key)
: LLFloater(key)
, mGetObjectNameCallback(NULL)
{
}

// Destroys the object
LLFloaterGetBlockedObjectName::~LLFloaterGetBlockedObjectName()
{
	gFocusMgr.releaseFocusIfNeeded( this );
}

BOOL LLFloaterGetBlockedObjectName::postBuild()
{
	getChild<LLButton>("OK")->		setCommitCallback(boost::bind(&LLFloaterGetBlockedObjectName::applyBlocking, this));
	getChild<LLButton>("Cancel")->	setCommitCallback(boost::bind(&LLFloaterGetBlockedObjectName::cancelBlocking, this));
	center();

	return LLFloater::postBuild();
}

BOOL LLFloaterGetBlockedObjectName::handleKeyHere(KEY key, MASK mask)
{
	if (key == KEY_RETURN && mask == MASK_NONE)
	{
		applyBlocking();
		return TRUE;
	}
	else if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		cancelBlocking();
		return TRUE;
	}

	return LLFloater::handleKeyHere(key, mask);
}

// static
LLFloaterGetBlockedObjectName* LLFloaterGetBlockedObjectName::show(get_object_name_callback_t callback)
{
	LLFloaterGetBlockedObjectName* floater = LLFloaterReg::showTypedInstance<LLFloaterGetBlockedObjectName>("mute_object_by_name");

	floater->mGetObjectNameCallback = callback;

	// *TODO: mantipov: should LLFloaterGetBlockedObjectName be closed when panel is closed?
	// old Floater dependency is not enable in panel
	// addDependentFloater(floater);

	return floater;
}

//////////////////////////////////////////////////////////////////////////
// Private Section
void LLFloaterGetBlockedObjectName::applyBlocking()
{
	if (mGetObjectNameCallback)
	{
		const std::string& text = getChild<LLUICtrl>("object_name")->getValue().asString();
		mGetObjectNameCallback(text);
	}
	closeFloater();
}

void LLFloaterGetBlockedObjectName::cancelBlocking()
{
	closeFloater();
}

//EOF
