/** 
 * @file llconversationmodel.cpp
 * @brief Implementation of conversations list
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

#include "llconversationmodel.h"

//
// Conversation items : common behaviors
//

LLConversationItem::LLConversationItem(std::string display_name, const LLUUID& uuid, LLFolderViewModelInterface& root_view_model) :
	LLFolderViewModelItemCommon(root_view_model),
	mName(display_name),
	mUUID(uuid),
	mNeedsRefresh(true),
	mConvType(CONV_UNKNOWN)
{
}

LLConversationItem::LLConversationItem(const LLUUID& uuid, LLFolderViewModelInterface& root_view_model) :
	LLFolderViewModelItemCommon(root_view_model),
	mName(""),
	mUUID(uuid),
	mNeedsRefresh(true),
	mConvType(CONV_UNKNOWN)
{
}

LLConversationItem::LLConversationItem(LLFolderViewModelInterface& root_view_model) :
	LLFolderViewModelItemCommon(root_view_model),
	mName(""),
	mUUID(),
	mNeedsRefresh(true),
	mConvType(CONV_UNKNOWN)
{
}

// Virtual action callbacks
void LLConversationItem::performAction(LLInventoryModel* model, std::string action)
{
}

void LLConversationItem::openItem( void )
{
}

void LLConversationItem::closeItem( void )
{
}

void LLConversationItem::previewItem( void )
{
}

void LLConversationItem::showProperties(void)
{
}

//
// LLConversationItemSession
// 

LLConversationItemSession::LLConversationItemSession(std::string display_name, const LLUUID& uuid, LLFolderViewModelInterface& root_view_model) :
	LLConversationItem(display_name,uuid,root_view_model),
	mIsLoaded(false)
{
	mConvType = CONV_SESSION_UNKNOWN;
}

LLConversationItemSession::LLConversationItemSession(const LLUUID& uuid, LLFolderViewModelInterface& root_view_model) :
	LLConversationItem(uuid,root_view_model)
{
	mConvType = CONV_SESSION_UNKNOWN;
}

bool LLConversationItemSession::hasChildren() const
{
	return getChildrenCount() > 0;
}

void LLConversationItemSession::addParticipant(LLConversationItemParticipant* participant)
{
	addChild(participant);
	mIsLoaded = true;
	mNeedsRefresh = true;
}

void LLConversationItemSession::removeParticipant(LLConversationItemParticipant* participant)
{
	removeChild(participant);
	mNeedsRefresh = true;
}

void LLConversationItemSession::removeParticipant(const LLUUID& participant_id)
{
	LLConversationItemParticipant* participant = findParticipant(participant_id);
	if (participant)
	{
		removeParticipant(participant);
	}
}

void LLConversationItemSession::clearParticipants()
{
	clearChildren();
	mIsLoaded = false;
	mNeedsRefresh = true;
}

LLConversationItemParticipant* LLConversationItemSession::findParticipant(const LLUUID& participant_id)
{
	// This is *not* a general tree parsing algorithm. It assumes that a session contains only 
	// items (LLConversationItemParticipant) that have themselve no children.
	LLConversationItemParticipant* participant = NULL;
	child_list_t::iterator iter;
	for (iter = mChildren.begin(); iter != mChildren.end(); iter++)
	{
		participant = dynamic_cast<LLConversationItemParticipant*>(*iter);
		if (participant->hasSameValue(participant_id))
		{
			break;
		}
	}
	return (iter == mChildren.end() ? NULL : participant);
}

void LLConversationItemSession::setParticipantIsMuted(const LLUUID& participant_id, bool is_muted)
{
	LLConversationItemParticipant* participant = findParticipant(participant_id);
	if (participant)
	{
		participant->setIsMuted(is_muted);
	}
}

void LLConversationItemSession::setParticipantIsModerator(const LLUUID& participant_id, bool is_moderator)
{
	LLConversationItemParticipant* participant = findParticipant(participant_id);
	if (participant)
	{
		participant->setIsModerator(is_moderator);
	}
}

void LLConversationItemSession::dumpDebugData()
{
	llinfos << "Merov debug : session, uuid = " << mUUID << ", name = " << mName << ", is loaded = " << mIsLoaded << llendl;
	LLConversationItemParticipant* participant = NULL;
	child_list_t::iterator iter;
	for (iter = mChildren.begin(); iter != mChildren.end(); iter++)
	{
		participant = dynamic_cast<LLConversationItemParticipant*>(*iter);
		participant->dumpDebugData();
	}
}

//
// LLConversationItemParticipant
// 

LLConversationItemParticipant::LLConversationItemParticipant(std::string display_name, const LLUUID& uuid, LLFolderViewModelInterface& root_view_model) :
	LLConversationItem(display_name,uuid,root_view_model),
	mIsMuted(false),
	mIsModerator(false)
{
	mConvType = CONV_PARTICIPANT;
}

LLConversationItemParticipant::LLConversationItemParticipant(const LLUUID& uuid, LLFolderViewModelInterface& root_view_model) :
	LLConversationItem(uuid,root_view_model)
{
	mConvType = CONV_PARTICIPANT;
}

void LLConversationItemParticipant::onAvatarNameCache(const LLAvatarName& av_name)
{
	mName = av_name.mDisplayName;
	// *TODO : we should also store that one, to be used in the tooltip : av_name.mUsername
	mNeedsRefresh = true;
	if (mParent)
	{
		mParent->requestSort();
	}
}

void LLConversationItemParticipant::dumpDebugData()
{
	llinfos << "Merov debug : participant, uuid = " << mUUID << ", name = " << mName << ", muted = " << mIsMuted << ", moderator = " << mIsModerator << llendl;
}

//
// LLConversationSort
// 

bool LLConversationSort::operator()(const LLConversationItem* const& a, const LLConversationItem* const& b) const
{
	LLConversationItem::EConversationType type_a = a->getType();
	LLConversationItem::EConversationType type_b = b->getType();

	if ((type_a == LLConversationItem::CONV_PARTICIPANT) && (type_b == LLConversationItem::CONV_PARTICIPANT))
	{
		// If both are participants
		U32 sort_order = getSortOrderParticipants();
		if (sort_order == LLConversationFilter::SO_DATE)
		{
			F32 time_a = 0.0;
			F32 time_b = 0.0;
			if (a->getTime(time_a) && b->getTime(time_b))
			{
				return (time_a > time_b);
			}
		}
		else if (sort_order == LLConversationFilter::SO_DISTANCE)
		{
			F32 dist_a = 0.0;
			F32 dist_b = 0.0;
			if (a->getDistanceToAgent(dist_a) && b->getDistanceToAgent(dist_b))
			{
				return (dist_a > dist_b);
			}
		}
	}
	else if ((type_a > LLConversationItem::CONV_PARTICIPANT) && (type_b > LLConversationItem::CONV_PARTICIPANT))
	{
		// If both are sessions
		U32 sort_order = getSortOrderSessions();
		if (sort_order == LLConversationFilter::SO_DATE)
		{
			F32 time_a = 0.0;
			F32 time_b = 0.0;
			if (a->getTime(time_a) && b->getTime(time_b))
			{
				return (time_a > time_b);
			}
		}
		else if (sort_order == LLConversationFilter::SO_SESSION_TYPE)
		{
			return (type_a < type_b);
		}
	}
	else
	{
		// If one is a participant and the other a session, the session is always "less" than the participant
		// so we simply compare the type
		// Notes: as a consequence, CONV_UNKNOWN (which should never get created...) always come first
		return (type_a < type_b);
	}
	// By default, in all other possible cases (including sort order of type LLConversationFilter::SO_NAME of course), sort by name
	S32 compare = LLStringUtil::compareDict(a->getDisplayName(), b->getDisplayName());
	return (compare < 0);
}

//
// LLConversationViewModel
//

void LLConversationViewModel::sort(LLFolderViewFolder* folder) 
{
	base_t::sort(folder);
}

// EOF
