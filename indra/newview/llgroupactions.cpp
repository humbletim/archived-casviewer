/** 
 * @file llgroupactions.cpp
 * @brief Group-related actions (join, leave, new, delete, etc)
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

#include "llgroupactions.h"

#include "message.h"

#include "llagent.h"
#include "llslurl.h"
#include "llcommandhandler.h"
#include "llfloaterreg.h"
#include "llgroupmgr.h"
#include "llimview.h" // for gIMMgr
#include "llnotificationsutil.h"
#include "llsidetray.h"
#include "llstatusbar.h"	// can_afford_transaction()
#include "llimfloater.h"
//-TT  ShowGroupFloaters
#include "llpanelgroup.h"
//-TT
// [RLVa:KB] - Checked: 2011-03-28 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
#include "rlvhandler.h"
// [/RLVa:KB]

//
// Globals
//

class LLGroupHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLGroupHandler() : LLCommandHandler("group", UNTRUSTED_THROTTLE) { }
	bool handle(const LLSD& tokens, const LLSD& query_map,
				LLMediaCtrl* web)
	{
		if (!LLUI::sSettingGroups["config"]->getBOOL("EnableGroupInfo"))
		{
			LLNotificationsUtil::add("NoGroupInfo", LLSD(), LLSD(), std::string("SwitchToStandardSkinAndQuit"));
			return true;
		}

		if (tokens.size() < 1)
		{
			return false;
		}

		if (tokens[0].asString() == "create")
		{
			LLGroupActions::createGroup();
			return true;
		}

		if (tokens.size() < 2)
		{
			return false;
		}

		if (tokens[0].asString() == "list")
		{
			if (tokens[1].asString() == "show")
			{
				LLSD params;
				params["people_panel_tab_name"] = "groups_panel";
				LLSideTray::getInstance()->showPanel("panel_people", params);
				return true;
			}
            return false;
		}

		LLUUID group_id;
		if (!group_id.set(tokens[0], FALSE))
		{
			return false;
		}

		if (tokens[1].asString() == "about")
		{
			if (group_id.isNull())
				return true;

			LLGroupActions::show(group_id);

			return true;
		}
		if (tokens[1].asString() == "inspect")
		{
			if (group_id.isNull())
				return true;
			LLGroupActions::inspect(group_id);
			return true;
		}
		return false;
	}
};
LLGroupHandler gGroupHandler;

// static
void LLGroupActions::search()
{
	LLFloaterReg::showInstance("search", LLSD().with("category", "groups"));
}

// static
void LLGroupActions::startCall(const LLUUID& group_id)
{
	// create a new group voice session
	LLGroupData gdata;

	if (!gAgent.getGroupData(group_id, gdata))
	{
		llwarns << "Error getting group data" << llendl;
		return;
	}

// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
	if ( (rlv_handler_t::isEnabled()) && (!gRlvHandler.canStartIM(group_id)) && (!gIMMgr->hasSession(group_id)) )
	{
		make_ui_sound("UISndInvalidOp");
		RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_STARTIM, LLSD().with("RECIPIENT", LLSLURL("group", group_id, "about").getSLURLString()));
		return;
	}
// [/RLVa:KB]

	LLUUID session_id = gIMMgr->addSession(gdata.mName, IM_SESSION_GROUP_START, group_id, true);
	if (session_id == LLUUID::null)
	{
		llwarns << "Error adding session" << llendl;
		return;
	}

	// start the call
	gIMMgr->autoStartCallOnStartup(session_id);

	make_ui_sound("UISndStartIM");
}

// static
void LLGroupActions::join(const LLUUID& group_id)
{
	if (!gAgent.canJoinGroups())
	{
		LLNotificationsUtil::add("JoinedTooManyGroups");
		return;
	}

	LLGroupMgrGroupData* gdatap = 
		LLGroupMgr::getInstance()->getGroupData(group_id);

	if (gdatap)
	{
		S32 cost = gdatap->mMembershipFee;
		LLSD args;
		args["COST"] = llformat("%d", cost);
		args["NAME"] = gdatap->mName;
		LLSD payload;
		payload["group_id"] = group_id;

		if (can_afford_transaction(cost))
		{
			if(cost > 0)
				LLNotificationsUtil::add("JoinGroupCanAfford", args, payload, onJoinGroup);
			else
				LLNotificationsUtil::add("JoinGroupNoCost", args, payload, onJoinGroup);
				
		}
		else
		{
			LLNotificationsUtil::add("JoinGroupCannotAfford", args, payload);
		}
	}
	else
	{
		llwarns << "LLGroupMgr::getInstance()->getGroupData(" << group_id 
			<< ") was NULL" << llendl;
	}
}

// static
bool LLGroupActions::onJoinGroup(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (option == 1)
	{
		// user clicked cancel
		return false;
	}

	LLGroupMgr::getInstance()->
		sendGroupMemberJoin(notification["payload"]["group_id"].asUUID());
	return false;
}

// static
void LLGroupActions::leave(const LLUUID& group_id)
{
//	if (group_id.isNull())
//		return;
// [RLVa:KB] - Checked: 2011-03-28 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
	if ( (group_id.isNull()) || ((gAgent.getGroupID() == group_id) && (gRlvHandler.hasBehaviour(RLV_BHVR_SETGROUP))) )
		return;
// [/RLVa:KB]

	S32 count = gAgent.mGroups.count();
	S32 i;
	for (i = 0; i < count; ++i)
	{
		if(gAgent.mGroups.get(i).mID == group_id)
			break;
	}
	if (i < count)
	{
		LLSD args;
		args["GROUP"] = gAgent.mGroups.get(i).mName;
		LLSD payload;
		payload["group_id"] = group_id;
		LLNotificationsUtil::add("GroupLeaveConfirmMember", args, payload, onLeaveGroup);
	}
}

// static
void LLGroupActions::activate(const LLUUID& group_id)
{
// [RLVa:KB] - Checked: 2011-03-28 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SETGROUP))
		return;
// [/RLVa:KB]

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ActivateGroup);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_GroupID, group_id);
	gAgent.sendReliableMessage();
}

/*
static bool isGroupUIVisible()
{
	static LLPanel* panel = 0;
	if(!panel)
		panel = LLSideTray::getInstance()->getPanel("panel_group_info_sidetray");
	if(!panel)
		return false;
	return panel->isInVisibleChain();
}
*/

//-TT - Patch : ShowGroupFloaters
static bool isGroupVisible(const LLUUID& group_id)
{
	static LLPanelGroup* panel = 0;
	LLSideTray *sidetray = LLSideTray::getInstance();
	
	if (!gSavedSettings.getBOOL("ShowGroupFloaters")) 
	{
		//AO reworked logic for only updated profiles that are already visible.
		panel = sidetray->getPanel<LLPanelGroup>("panel_group_info_sidetray");
		if(!panel)
			return false;
		else if (panel->getID() != group_id)
			return false;
		if (sidetray->isTabAttached("sidebar_people"))
		{
			return panel->isInVisibleChain();
		}
		else
		{
			return sidetray->isFloaterPanelVisible("panel_group_info_sidetray");
		}
	}
	else
	{
		bool vis = LLFloaterReg::instanceVisible("floater_group_view", LLSD().with("group_id", group_id));
		llinfos << "checking visibility for group ID: " << group_id << " visible =" << vis << llendl;
		return vis;

		//LLFloater *floater = LLFloaterReg::getInstance("floater_group_view", LLSD().with("group_id", group_id));
		//panel = floater->findChild<LLPanelGroup>("panel_group_info_sidetray");
		//return panel->isInVisibleChain();
	}
	//AO reworked logic for only updated profiles that are already visible.
    //panel = sidetray->getPanel<LLPanelGroup>("panel_group_info_sidetray");
	//if(!panel)
	//	return false;
 //   if (panel->getID() != group_id)
	//	return false;
	//if (sidetray->isTabAttached("sidebar_people"))
	//{
	//	return panel->isInVisibleChain();
	//}
	//else
	//{
	//	return sidetray->isFloaterPanelVisible("panel_group_info_sidetray");
	//}
}
//-TT

// static 
void LLGroupActions::inspect(const LLUUID& group_id)
{
	LLFloaterReg::showInstance("inspect_group", LLSD().with("group_id", group_id));
}

// static
void LLGroupActions::show(const LLUUID& group_id)
{
	if (group_id.isNull())
		return;

//-TT - Patch : ShowGroupFloaters
	if (!gSavedSettings.getBOOL("ShowGroupFloaters")) 
	{
//-TT
		LLSD params;
		params["group_id"] = group_id;
		params["open_tab_name"] = "panel_group_info_sidetray";

		LLSideTray::getInstance()->showPanel("panel_group_info_sidetray", params);
//-TT - Patch : ShowGroupFloaters
	}
	else
	{
		LLFloaterReg::showInstance("floater_group_view", LLSD().with("group_id", group_id));
	}
//-TT}
}

// static
void LLGroupActions::show(const LLUUID& group_id, const std::string& tab_name)
{
	if (group_id.isNull())
		return;

	LLSD params;
	params["group_id"] = group_id;
	params["open_tab_name"] = tab_name;

	LLSideTray::getInstance()->showPanel("panel_group_info_sidetray", params);
}

void LLGroupActions::refresh_notices()
{
	//if(!isGroupUIVisible())
	//	return;

	if(!isGroupVisible(LLUUID::null))
		return;

	LLSD params;
	params["group_id"] = LLUUID::null;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "refresh_notices";

//-TT - Patch : ShowGroupFloaters
	if (!gSavedSettings.getBOOL("ShowGroupFloaters")) 
	{
//-TT
		// AO: We don't change modals on people unless they manually request this
		//LLSideTray::getInstance()->showPanel("panel_group_info_sidetray", params);
//-TT - Patch : ShowGroupFloaters
	}
	else
	{
		LLFloaterReg::showInstance("floater_group_view", params); //LLSD().with("group_id", LLUUID::null));
	}
//-TT
}

//static 
void LLGroupActions::refresh(const LLUUID& group_id)
{
	//if(!isGroupUIVisible())
	//	return;

	if(!isGroupVisible(group_id))
		return;

	LLSD params;
	params["group_id"] = group_id;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "refresh";

//-TT - Patch : ShowGroupFloaters
	llinfos << "refreshing group ID: " << group_id << llendl;
	if (!gSavedSettings.getBOOL("ShowGroupFloaters")) 
	{
//-TT
		LLSideTray::getInstance()->showPanel("panel_group_info_sidetray", params);
//-TT - Patch : ShowGroupFloaters
	}
	else
	{
		LLFloaterReg::showInstance("floater_group_view", LLSD().with("group_id", group_id));//params); //LLSD().with("group_id", group_id));
		//LLFloater *floater = LLFloaterReg::getInstance("floater_group_view", LLSD().with("group_id", group_id));
		//panel = floater->findChild<LLPanelGroup>("panel_group_info_sidetray");
	}
//-TT
}

//static 
void LLGroupActions::createGroup()
{
	LLSD params;
	params["group_id"] = LLUUID::null;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "create";

////-TT - Patch : ShowGroupFloaters
//	if (!gSavedSettings.getBOOL("ShowGroupFloaters")) 
//	{
////-TT
		params["open_tab_name"] = "panel_group_info_sidetray";
 
		LLSideTray::getInstance()->showPanel("panel_group_info_sidetray", params);
////-TT - Patch : ShowGroupFloaters
//	}
//	else
//	{
//		LLFloaterReg::showInstance("floater_group_view", params);  //LLSD().with("group_id", group_id));
//	}
////-TT
}
//static
void LLGroupActions::closeGroup(const LLUUID& group_id)
{
	//if(!isGroupUIVisible())
	//	return;

	if(!isGroupVisible(group_id))
		return;

	LLSD params;
	params["group_id"] = group_id;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "close";

//-TT - Patch : ShowGroupFloaters
	if (!gSavedSettings.getBOOL("ShowGroupFloaters")) 
	{
//-TT
		LLSideTray::getInstance()->showPanel("panel_group_info_sidetray", params);
//-TT - Patch : ShowGroupFloaters
	}
	else
	{
		LLFloaterReg::showInstance("floater_group_view", params);  //LLSD().with("group_id", group_id));
	}
//-TT}
}

// static
void LLGroupActions::startIM(const LLUUID& group_id)
{
	if (group_id.isNull())
		return;

// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
	if ( (rlv_handler_t::isEnabled()) && (!gRlvHandler.canStartIM(group_id)) && (!gIMMgr->hasSession(group_id)) )
	{
		make_ui_sound("UISndInvalidOp");
		RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_STARTIM, LLSD().with("RECIPIENT", LLSLURL("group", group_id, "about").getSLURLString()));
		return;
	}
// [/RLVa:KB]

	LLGroupData group_data;
	if (gAgent.getGroupData(group_id, group_data))
	{
		LLUUID session_id = gIMMgr->addSession(
			group_data.mName,
			IM_SESSION_GROUP_START,
			group_id);
		if (session_id != LLUUID::null)
		{
			LLIMFloater::show(session_id);
		}
		make_ui_sound("UISndStartIM");
	}
	else
	{
		// this should never happen, as starting a group IM session
		// relies on you belonging to the group and hence having the group data
		make_ui_sound("UISndInvalidOp");
	}
}

// static
void LLGroupActions::endIM(const LLUUID& group_id)
{
	if (group_id.isNull())
		return;
	
	LLUUID session_id = gIMMgr->computeSessionID(IM_SESSION_GROUP_START, group_id);
	if (session_id != LLUUID::null)
	{
		gIMMgr->leaveSession(session_id);
	}
}

// static
bool LLGroupActions::isInGroup(const LLUUID& group_id)
{
	// *TODO: Move all the LLAgent group stuff into another class, such as
	// this one.
	return gAgent.isInGroup(group_id);
}

// static
bool LLGroupActions::isAvatarMemberOfGroup(const LLUUID& group_id, const LLUUID& avatar_id)
{
	if(group_id.isNull() || avatar_id.isNull())
	{
		return false;
	}

	LLGroupMgrGroupData* group_data = LLGroupMgr::getInstance()->getGroupData(group_id);
	if(!group_data)
	{
		return false;
	}

	if(group_data->mMembers.end() == group_data->mMembers.find(avatar_id))
	{
		return false;
	}

	return true;
}

//-- Private methods ----------------------------------------------------------

// static
bool LLGroupActions::onLeaveGroup(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLUUID group_id = notification["payload"]["group_id"].asUUID();
	if(option == 0)
	{
//-TT - Patch : ShowGroupFloaters
		if (gSavedSettings.getBOOL("ShowGroupFloaters")) 
		{
			LLFloaterReg::destroyInstance("floater_group_view", LLSD().with("group_id", group_id));
		}
//-TT}
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_LeaveGroupRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_GroupData);
		msg->addUUIDFast(_PREHASH_GroupID, group_id);
		gAgent.sendReliableMessage();
	}
	return false;
}
