/** 
 * @file llinspectremoteobject.cpp
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "llfloaterreg.h"
#include "llinspectremoteobject.h"
#include "llinspect.h"
#include "llmutelist.h"
#include "llpanelblockedlist.h"
#include "llslurl.h"
#include "lltrans.h"
#include "llui.h"
#include "lluictrl.h"
#include "llurlaction.h"
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.0f)
#include "rlvhandler.h"
// [/RLVa:KB]

//////////////////////////////////////////////////////////////////////////////
// LLInspectRemoteObject
//////////////////////////////////////////////////////////////////////////////

// Remote Object Inspector, a small information window used to
// display information about potentially-remote objects. Used
// to display details about objects sending messages to the user.
class LLInspectRemoteObject : public LLInspect
{
	friend class LLFloaterReg;
	
public:
	LLInspectRemoteObject(const LLSD& object_id);
	virtual ~LLInspectRemoteObject() {};

	/*virtual*/ BOOL postBuild(void);
	/*virtual*/ void onOpen(const LLSD& avatar_id);

	void onClickMap();
	void onClickBlock();
	void onClickClose();
	
private:
	void update();
	static void nameCallback(const LLUUID& id, const std::string& first, const std::string& last, BOOL is_group, void* data);
	
private:
	LLUUID		 mObjectID;
	LLUUID		 mOwnerID;
	std::string  mOwner;
	std::string  mSLurl;
	std::string  mName;
	bool         mGroupOwned;
};

LLInspectRemoteObject::LLInspectRemoteObject(const LLSD& sd) :
	LLInspect(LLSD()),
	mObjectID(NULL),
	mOwnerID(NULL),
	mOwner(""),
	mSLurl(""),
	mName(""),
	mGroupOwned(false)
{
}

/*virtual*/
BOOL LLInspectRemoteObject::postBuild(void)
{
	// hook up the inspector's buttons
	getChild<LLUICtrl>("map_btn")->setCommitCallback(
		boost::bind(&LLInspectRemoteObject::onClickMap, this));
	getChild<LLUICtrl>("block_btn")->setCommitCallback(
		boost::bind(&LLInspectRemoteObject::onClickBlock, this));
	getChild<LLUICtrl>("close_btn")->setCommitCallback(
		boost::bind(&LLInspectRemoteObject::onClickClose, this));

	return TRUE;
}

/*virtual*/
void LLInspectRemoteObject::onOpen(const LLSD& data)
{
	// Start animation
	LLInspect::onOpen(data);

	// Extract appropriate object information from input LLSD
	// (Eventually, it might be nice to query server for details
	// rather than require caller to pass in the information.)
	mObjectID   = data["object_id"].asUUID();
	mName       = data["name"].asString();
	mOwnerID    = data["owner_id"].asUUID();
	mGroupOwned = data["group_owned"].asBoolean();
	mSLurl      = data["slurl"].asString();

	// work out the owner's name
//	mOwner = "";
//	if (gCacheName)
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
	mOwner = (data.has("owner_name")) ? data["owner_name"].asString() : "";
	if ( (gCacheName) && (mOwnerID.notNull()) )
// [/RLVa:KB]
	{
		gCacheName->get(mOwnerID, mGroupOwned, nameCallback, this);
	}

	// update the inspector with the current object state
	update();

	// Position the inspector relative to the mouse cursor
	// Similar to how tooltips are positioned
	// See LLToolTipMgr::createToolTip
	if (data.has("pos"))
	{
		LLUI::positionViewNearMouse(this, data["pos"]["x"].asInteger(), data["pos"]["y"].asInteger());
	}
	else
	{
		LLUI::positionViewNearMouse(this);
	}
}

void LLInspectRemoteObject::onClickMap()
{
	std::string url = "secondlife://" + mSLurl;
	LLUrlAction::showLocationOnMap(url);
	closeFloater();
}

void LLInspectRemoteObject::onClickBlock()
{
	LLMute::EType mute_type = mGroupOwned ? LLMute::GROUP : LLMute::AGENT;
	LLMute mute(mOwnerID, mOwner, mute_type);
	LLMuteList::getInstance()->add(mute);
	LLPanelBlockedList::showPanelAndSelect(mute.mID);
	closeFloater();
}

void LLInspectRemoteObject::onClickClose()
{
	closeFloater();
}

//static 
void LLInspectRemoteObject::nameCallback(const LLUUID& id, const std::string& first, const std::string& last, BOOL is_group, void* data)
{
	LLInspectRemoteObject *self = (LLInspectRemoteObject*)data;
	self->mOwner = first;
	if (!last.empty())
	{
		self->mOwner += " " + last;
	}
	self->update();
}

void LLInspectRemoteObject::update()
{
	// show the object name as the inspector's title
	// (don't hyperlink URLs in object names)
	getChild<LLUICtrl>("object_name")->setValue("<nolink>" + mName + "</nolink>");

	// show the object's owner - click it to show profile
	std::string owner = mOwner;
	if (! mOwnerID.isNull())
	{
		if (mGroupOwned)
		{
			owner = LLSLURL("group", mOwnerID, "about").getSLURLString();
		}
		else
		{
			owner = LLSLURL("agent", mOwnerID, "about").getSLURLString();
		}
	}
//	else
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
	else if (mOwner.empty()) // If "objectim" was subject to @shownames then we passed an anonimized owner name so use that if available
// [/RLVa:KB]
	{
		owner = LLTrans::getString("Unknown");
	}
	getChild<LLUICtrl>("object_owner")->setValue(owner);

	// display the object's SLurl - click it to teleport
	std::string url;
	if (! mSLurl.empty())
	{
		url = "secondlife:///app/teleport/" + mSLurl;
	}
	getChild<LLUICtrl>("object_slurl")->setValue(url);

	// disable the Map button if we don't have a SLurl
	getChild<LLUICtrl>("map_btn")->setEnabled(! mSLurl.empty());

	// disable the Block button if we don't have the owner ID
	getChild<LLUICtrl>("block_btn")->setEnabled(! mOwnerID.isNull());

// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
	if ( (rlv_handler_t::isEnabled()) && (RlvStrings::getString(RLV_STRING_HIDDEN_REGION) == mSLurl) )
	{
		getChild<LLUICtrl>("object_slurl")->setValue(mSLurl);
		getChild<LLUICtrl>("map_btn")->setEnabled(false);
	}
// [/RLVa:KB]
}

//////////////////////////////////////////////////////////////////////////////
// LLInspectRemoteObjectUtil
//////////////////////////////////////////////////////////////////////////////
void LLInspectRemoteObjectUtil::registerFloater()
{
	LLFloaterReg::add("inspect_remote_object", "inspect_remote_object.xml",
					  &LLFloaterReg::build<LLInspectRemoteObject>);
}
