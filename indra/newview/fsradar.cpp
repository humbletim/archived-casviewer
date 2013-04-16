/**
 * @file fsradar.cpp
 * @brief Firestorm radar implementation
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsradar.h"

// libs
#include <boost/algorithm/string.hpp>
#include "llavatarnamecache.h"
#include "llnotificationsutil.h"
#include "lleventtimer.h"

// newview
#include "fscommon.h"
#include "fslslbridge.h"
#include "lggcontactsets.h"
#include "llagent.h"
#include "llavataractions.h"
#include "llavatarconstants.h"		// for range constants
#include "llavatarlist.h"
#include "llgroupactions.h"
#include "llnotificationmanager.h"
#include "lltracker.h"
#include "llviewercontrol.h"		// for gSavedSettings
#include "llviewermenu.h"			// for gMenuHolder
#include "llvoavatar.h"
#include "llvoiceclient.h"
#include "llworld.h"
#include "llspeakers.h"
#include "rlvhandler.h"

using namespace boost;

#define FS_RADAR_LIST_UPDATE_INTERVAL 1

std::string formatString(std::string text, const LLStringUtil::format_map_t& args)
{
	LLStringUtil::format(text, args);
	return text;
}

/**
 * Periodically updates the nearby people list while the Nearby tab is active.
 * 
 * The period is defined by FS_NEARBY_LIST_UPDATE_INTERVAL constant.
 */
class FSRadarListUpdater : public FSRadar::Updater, public LLEventTimer
{
	LOG_CLASS(FSRadarListUpdater);

public:
	FSRadarListUpdater(callback_t cb)
	:	LLEventTimer(FS_RADAR_LIST_UPDATE_INTERVAL),
		FSRadar::Updater(cb)
	{
		update();
		mEventTimer.start(); 
	}

	/*virtual*/ BOOL tick()
	{
		update();
		return FALSE;
	}
};

//=============================================================================

FSRadar::FSRadar() :
		mNearbyList(NULL),
		mRadarAlertRequest(false),
		mRadarFrameCount(0),
		mRadarLastBulkOffsetRequestTime(0),
		mRadarLastRequestTime(0.f)
{
	// TODO: Ewwww ugly! Need to get rid of LLAvatarList -Ansariel
	mNearbyList = new LLAvatarList(LLAvatarList::Params::Params());
// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.2a) | Added: RLVa-1.2.0d
	mNearbyList->setRlvCheckShowNames(true);
// [/RLVa:KB]

	mRadarListUpdater = new FSRadarListUpdater(boost::bind(&FSRadar::updateRadarList, this));
}

FSRadar::~FSRadar()
{
	delete mRadarListUpdater;
	delete mNearbyList;
}

void FSRadar::radarAlertMsg(const LLUUID& agent_id, const LLAvatarName& av_name, const std::string& postMsg)
{
// <FS:CR> Milkshake-style radar alerts
	LLCachedControl<bool> milkshake_radar(gSavedSettings, "FSMilkshakeRadarToasts", false);
	
	if (milkshake_radar)
	{
		LLSD payload = agent_id;
		LLSD args;
		args["NAME"] = getRadarName(av_name);
		args["MESSAGE"] = postMsg;
		LLNotificationPtr notification;
		notification = LLNotificationsUtil::add("RadarAlert",
												args,
												payload.with("respond_on_mousedown", TRUE),
												boost::bind(&LLAvatarActions::zoomIn, agent_id));
	}
	else
	{
// </FS:CR>
		LLChat chat;
		chat.mText = postMsg;
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		chat.mFromName = getRadarName(av_name);
		chat.mFromID = agent_id;
		chat.mChatType = CHAT_TYPE_RADAR;
		// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
		LLSD args;
		args["type"] = LLNotificationsUI::NT_NEARBYCHAT;
		LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
	} // <FS:CR />
}

void FSRadar::updateRadarList()
{
	//AO : Warning, reworked heavily for Firestorm.
	if (!mNearbyList)
	{
		return;
	}

	//Configuration
	LLWorld* world = LLWorld::getInstance();
	LLMuteList* mutelist = LLMuteList::getInstance();

	static const F32 chat_range_say = world->getSayDistance();
	static const F32 chat_range_shout = world->getShoutDistance();

	static const std::string str_chat_entering =			LLTrans::getString("entering_chat_range");
	static const std::string str_chat_leaving =				LLTrans::getString("leaving_chat_range");
	static const std::string str_draw_distance_entering =	LLTrans::getString("entering_draw_distance");
	static const std::string str_draw_distance_leaving =	LLTrans::getString("leaving_draw_distance");
	static const std::string str_region_entering =			LLTrans::getString("entering_region");
	static const std::string str_region_entering_distance =	LLTrans::getString("entering_region_distance");
	static const std::string str_region_leaving =			LLTrans::getString("leaving_region");

	static LLCachedControl<bool> RadarReportChatRangeEnter(gSavedSettings, "RadarReportChatRangeEnter");
	static LLCachedControl<bool> RadarReportChatRangeLeave(gSavedSettings, "RadarReportChatRangeLeave");
	static LLCachedControl<bool> RadarReportDrawRangeEnter(gSavedSettings, "RadarReportDrawRangeEnter");
	static LLCachedControl<bool> RadarReportDrawRangeLeave(gSavedSettings, "RadarReportDrawRangeLeave");
	static LLCachedControl<bool> RadarReportSimRangeEnter(gSavedSettings, "RadarReportSimRangeEnter");
	static LLCachedControl<bool> RadarReportSimRangeLeave(gSavedSettings, "RadarReportSimRangeLeave");
	static LLCachedControl<bool> RadarEnterChannelAlert(gSavedSettings, "RadarEnterChannelAlert");
	static LLCachedControl<bool> RadarLeaveChannelAlert(gSavedSettings, "RadarLeaveChannelAlert");
	static LLCachedControl<F32> nearMeRange(gSavedSettings, "NearMeRange");
	static LLCachedControl<bool> limitRange(gSavedSettings, "LimitRadarByRange");
	static LLCachedControl<bool> sUseLSLBridge(gSavedSettings, "UseLSLBridge");
	static LLCachedControl<F32> RenderFarClip(gSavedSettings, "RenderFarClip");

	F32 drawRadius(RenderFarClip);
	const LLVector3d& posSelf = gAgent.getPositionGlobal();
	LLViewerRegion* reg = gAgent.getRegion();
	LLUUID regionSelf;
	if (reg)
	{
		regionSelf = reg->getRegionID();
	}
	bool alertScripts = mRadarAlertRequest; // save the current value, so it doesn't get changed out from under us by another thread
	std::vector<LLPanel*> items;
	time_t now = time(NULL);

	//STEP 0: Clear model data
	mRadarEnterAlerts.clear();
	mRadarLeaveAlerts.clear();
	mRadarOffsetRequests.clear();
	mRadarEntriesData.clear();
	mAvatarStats.clear();
	
	//STEP 1: Update our basic data model: detect Avatars & Positions in our defined range
	std::vector<LLVector3d> positions;
	uuid_vec_t avatar_ids;
	if (limitRange)
	{
		world->getAvatars(&avatar_ids, &positions, gAgent.getPositionGlobal(), nearMeRange);
	}
	else
	{
		world->getAvatars(&avatar_ids, &positions);
	}
	mNearbyList->getIDs() = avatar_ids; // copy constructor, refreshing underlying mNearbyList
	mNearbyList->setDirty(true, true); // AO: These optional arguements force updating even when we're not a visible window.
	mNearbyList->getItems(items);
	LLLocalSpeakerMgr::getInstance()->update(TRUE);
	
	//STEP 2: Transform detected model list data into more flexible multimap data structure;
	//TS: Count avatars in chat range and in the same region
	U32 inChatRange = 0;
	U32 inSameRegion = 0;
	std::vector<LLVector3d>::const_iterator
		pos_it = positions.begin(),
		pos_end = positions.end();	
	uuid_vec_t::const_iterator
		item_it = avatar_ids.begin(),
		item_end = avatar_ids.end();
	for (;pos_it != pos_end && item_it != item_end; ++pos_it, ++item_it )
	{
		//
		//2a. For each detected av, gather up all data we would want to display or use to drive alerts
		//
		
		LLUUID avId          = static_cast<LLUUID>(*item_it);
		LLAvatarListItem* av = mNearbyList->getAvatarListItem(avId);
		LLVector3d avPos     = static_cast<LLVector3d>(*pos_it);
		S32 seentime		 = 0;
		LLUUID avRegion;
		
		// Skip modelling this avatar if its basic data is either inaccessible, or it's a dummy placeholder
		LLViewerRegion *reg	 = world->getRegionFromPosGlobal(avPos);
		if (!reg || !av) // don't update this radar listing if data is inaccessible
		{
			continue;
		}

		// Try to get the avatar's viewer object - we will need it anyway later
		LLVOAvatar* avVo = (LLVOAvatar*)gObjectList.findObject(avId);

		static LLUICachedControl<bool> showdummyav("FSShowDummyAVsinRadar");
		if (!showdummyav)
		{
			if (avVo && avVo->mIsDummy)
			{
				continue;
			}
		}

		avRegion = reg->getRegionID();
		if (lastRadarSweep.count(avId) > 1) // if we detect a multiple ID situation, get lastSeenTime from our cache instead
		{
			std::pair<std::multimap<LLUUID, radarFields>::iterator, std::multimap<LLUUID, radarFields>::iterator> dupeAvs;
			dupeAvs = lastRadarSweep.equal_range(avId);
			for (std::multimap<LLUUID, radarFields>::iterator it2 = dupeAvs.first; it2 != dupeAvs.second; ++it2)
			{
				if (it2->second.lastRegion == avRegion)
				{
					seentime = (S32)difftime(now, it2->second.firstSeen);
				}
			}
		}
		else
		{
			seentime = (S32)difftime(now, av->getFirstSeen());
		}
		//av->setFirstSeen(now - (time_t)seentime); // maintain compatibility with underlying list, deprecated
		S32 hours = (S32)(seentime / 3600);
		S32 mins = (S32)((seentime - hours * 3600) / 60);
		S32 secs = (S32)((seentime - hours * 3600 - mins * 60));
		std::string avSeenStr = llformat("%d:%02d:%02d", hours, mins, secs);
		S32 avStatusFlags     = av->getAvStatus();
		std::string avFlagStr = "";
		if (avStatusFlags & AVATAR_IDENTIFIED)
		{
			avFlagStr += "$";
		}
		std::string avAgeStr = av->getAvatarAge();
		std::string avName   = getRadarName(avId);
		av->setAvatarName(avName); // maintain compatibility with underlying list; used in other locations!
		U32 lastZOffsetTime  = av->getLastZOffsetTime();
		F32 avZOffset        = av->getZOffset();
		if (avPos[VZ] == AVATAR_UNKNOWN_Z_OFFSET) // if our official z position is AVATAR_UNKNOWN_Z_OFFSET, we need a correction.
		{
			// set correction if we have it
			if (avZOffset > 0.1)
			{
				avPos[VZ] = avZOffset;
			}
			
			//schedule offset requests, if needed
			if (sUseLSLBridge && (now > (mRadarLastBulkOffsetRequestTime + FSRADAR_COARSE_OFFSET_INTERVAL)) && (now > lastZOffsetTime + FSRADAR_COARSE_OFFSET_INTERVAL))
			{
				mRadarOffsetRequests.push_back(avId);
				av->setLastZOffsetTime(now);
			}
		}	
		F32 avRange = (avPos[VZ] != AVATAR_UNKNOWN_Z_OFFSET ? dist_vec(avPos, posSelf) : AVATAR_UNKNOWN_RANGE);
		av->setRange(avRange); // maintain compatibility with underlying list; used in other locations!
		av->setPosition(avPos); // maintain compatibility with underlying list; used in other locations!
		
		//
		//2b. Process newly detected avatars
		//
		if (lastRadarSweep.count(avId) == 0)
		{
			// chat alerts
			if (RadarReportChatRangeEnter && (avRange <= chat_range_say) && avRange > AVATAR_UNKNOWN_RANGE)
			{
				LLStringUtil::format_map_t args;
				args["DISTANCE"] = llformat("%3.2f", avRange);
				std::string message = formatString(str_chat_entering, args);
				make_ui_sound("UISndRadarChatEnter"); // <FS:PP> FIRE-6069: Radar alerts sounds
				LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
			}
			if (RadarReportDrawRangeEnter && (avRange <= drawRadius) && avRange > AVATAR_UNKNOWN_RANGE)
			{
				LLStringUtil::format_map_t args;
				args["DISTANCE"] = llformat("%3.2f", avRange);
				std::string message = formatString(str_draw_distance_entering, args);
				make_ui_sound("UISndRadarDrawEnter"); // <FS:PP> FIRE-6069: Radar alerts sounds
				LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
			}
			if (RadarReportSimRangeEnter && (avRegion == regionSelf))
			{
				make_ui_sound("UISndRadarSimEnter"); // <FS:PP> FIRE-6069: Radar alerts sounds
				if (avRange != AVATAR_UNKNOWN_RANGE) // Don't report an inaccurate range in localchat, if the true range is not known.
				{
					LLStringUtil::format_map_t args;
					args["DISTANCE"] = llformat("%3.2f", avRange);
					std::string message = formatString(str_region_entering_distance, args);
					LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
				}
				else
				{
					LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_region_entering));
				}
			}
			if (RadarEnterChannelAlert || (alertScripts))
			{
				// Autodetect Phoenix chat UUID compatibility. 
				// If Leave channel alerts are not set, restrict reports to same-sim only.
				if (!RadarLeaveChannelAlert)
				{
					if (avRegion == regionSelf)
					{
						mRadarEnterAlerts.push_back(avId);
					}
				}
				else
				{
					mRadarEnterAlerts.push_back(avId);
				}
			}
		}
		
		//
		// 2c. Process previously detected avatars
		//
		else 
		{
			radarFields rf; // will hold the newest version
			// Check for range crossing alert threshholds, being careful to handle double-listings
			if (lastRadarSweep.count(avId) == 1) // normal case, check from last position
			{
				rf = lastRadarSweep.find(avId)->second;
				if (RadarReportChatRangeEnter || RadarReportChatRangeLeave)
				{
					if (RadarReportChatRangeEnter && (avRange <= chat_range_say && avRange > AVATAR_UNKNOWN_RANGE) && (rf.lastDistance > chat_range_say || rf.lastDistance == AVATAR_UNKNOWN_RANGE))
					{
						LLStringUtil::format_map_t args;
						args["DISTANCE"] = llformat("%3.2f", avRange);
						std::string message = formatString(str_chat_entering, args);
						make_ui_sound("UISndRadarChatEnter"); // <FS:PP> FIRE-6069: Radar alerts sounds
						LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
					}
					else if (RadarReportChatRangeLeave && (avRange > chat_range_say || avRange == AVATAR_UNKNOWN_RANGE) && (rf.lastDistance <= chat_range_say && rf.lastDistance > AVATAR_UNKNOWN_RANGE))
					{
						make_ui_sound("UISndRadarChatLeave"); // <FS:PP> FIRE-6069: Radar alerts sounds
						LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_chat_leaving));
					}
				}
				if (RadarReportDrawRangeEnter || RadarReportDrawRangeLeave)
				{
					if (RadarReportDrawRangeEnter && (avRange <= drawRadius && avRange > AVATAR_UNKNOWN_RANGE) && (rf.lastDistance > drawRadius || rf.lastDistance == AVATAR_UNKNOWN_RANGE))
					{
						LLStringUtil::format_map_t args;
						args["DISTANCE"] = llformat("%3.2f", avRange);
						std::string message = formatString(str_draw_distance_entering, args);
						make_ui_sound("UISndRadarDrawEnter"); // <FS:PP> FIRE-6069: Radar alerts sounds
						LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
					}
					else if (RadarReportDrawRangeLeave && (avRange > drawRadius || avRange == AVATAR_UNKNOWN_RANGE) && (rf.lastDistance <= drawRadius && rf.lastDistance > AVATAR_UNKNOWN_RANGE))
					{
						make_ui_sound("UISndRadarDrawLeave"); // <FS:PP> FIRE-6069: Radar alerts sounds
						LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_draw_distance_leaving));
					}
				}
				if (RadarReportSimRangeEnter || RadarReportSimRangeLeave )
				{
					if (RadarReportSimRangeEnter && (avRegion == regionSelf) && (avRegion != rf.lastRegion))
					{
						make_ui_sound("UISndRadarSimEnter"); // <FS:PP> FIRE-6069: Radar alerts sounds
						if (avRange != AVATAR_UNKNOWN_RANGE) // Don't report an inaccurate range in localchat, if the true range is not known.
						{
							LLStringUtil::format_map_t args;
							args["DISTANCE"] = llformat("%3.2f", avRange);
							std::string message = formatString(str_region_entering_distance, args);
							LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
						}
						else
						{
							LLAvatarNameCache::get(avId,boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_region_entering));
						}
					}
					else if (RadarReportSimRangeLeave && (rf.lastRegion == regionSelf) && (avRegion != regionSelf))
					{
						make_ui_sound("UISndRadarSimLeave"); // <FS:PP> FIRE-6069: Radar alerts sounds
						LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_region_leaving));
					}
				}
			}
			else if (lastRadarSweep.count(avId) > 1) // handle duplicates, from sim crossing oddness
			{
				// iterate through all the duplicates found, searching for the newest.
				rf.firstSeen=0;
				std::pair<std::multimap<LLUUID, radarFields>::iterator, std::multimap<LLUUID, radarFields>::iterator> dupeAvs;
				dupeAvs = lastRadarSweep.equal_range(avId);
				for (std::multimap<LLUUID, radarFields>::iterator it2 = dupeAvs.first; it2 != dupeAvs.second; ++it2)
				{
					if (it2->second.firstSeen > rf.firstSeen)
					{
						rf = it2->second;
					}
				}
				lldebugs << "Duplicates detected for " << avName <<" , most recent is " << rf.firstSeen << llendl;
				
				if (RadarReportChatRangeEnter || RadarReportChatRangeLeave)
				{
					if (RadarReportChatRangeEnter && (avRange <= chat_range_say && avRange > AVATAR_UNKNOWN_RANGE) && (rf.lastDistance > chat_range_say || rf.lastDistance == AVATAR_UNKNOWN_RANGE))
					{
						LLStringUtil::format_map_t args;
						args["DISTANCE"] = llformat("%3.2f", avRange);
						std::string message = formatString(str_chat_entering, args);
						make_ui_sound("UISndRadarChatEnter"); // <FS:PP> FIRE-6069: Radar alerts sounds
						LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
					}
					else if (RadarReportChatRangeLeave && (avRange > chat_range_say || avRange == AVATAR_UNKNOWN_RANGE) && (rf.lastDistance <= chat_range_say && rf.lastDistance > AVATAR_UNKNOWN_RANGE))
					{
						make_ui_sound("UISndRadarChatLeave"); // <FS:PP> FIRE-6069: Radar alerts sounds
						LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_chat_leaving));
					}
				}
				if (RadarReportDrawRangeEnter || RadarReportDrawRangeLeave)
				{
					if (RadarReportDrawRangeEnter && (avRange <= drawRadius && avRange > AVATAR_UNKNOWN_RANGE) && (rf.lastDistance > drawRadius || rf.lastDistance == AVATAR_UNKNOWN_RANGE))
					{
						LLStringUtil::format_map_t args;
						args["DISTANCE"] = llformat("%3.2f", avRange);
						std::string message = formatString(str_draw_distance_entering, args);
						make_ui_sound("UISndRadarDrawEnter"); // <FS:PP> FIRE-6069: Radar alerts sounds
						LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
					}
					else if (RadarReportDrawRangeLeave && (avRange > drawRadius || avRange == AVATAR_UNKNOWN_RANGE) && (rf.lastDistance <= drawRadius && rf.lastDistance > AVATAR_UNKNOWN_RANGE))
					{
						make_ui_sound("UISndRadarDrawLeave"); // <FS:PP> FIRE-6069: Radar alerts sounds
						LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_draw_distance_leaving));
					}
				}
				if (RadarReportSimRangeEnter || RadarReportSimRangeLeave)
				{
					if (RadarReportSimRangeEnter && (avRegion == regionSelf) && (avRegion != rf.lastRegion))
					{
						make_ui_sound("UISndRadarSimEnter"); // <FS:PP> FIRE-6069: Radar alerts sounds
						if (avRange != AVATAR_UNKNOWN_RANGE) // Don't report an inaccurate range in localchat, if the true range is not known.
						{
							LLStringUtil::format_map_t args;
							args["DISTANCE"] = llformat("%3.2f", avRange);
							std::string message = formatString(str_region_entering_distance, args);
							LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
						}
						else
						{
							LLAvatarNameCache::get(avId,boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_region_entering));
						}
					}
					else if (RadarReportSimRangeLeave && (rf.lastRegion == regionSelf) && (avRegion != regionSelf))
					{
						make_ui_sound("UISndRadarSimLeave"); // <FS:PP> FIRE-6069: Radar alerts sounds
						LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_region_leaving));
					}
				}
			}
			//If we were manually asked to update an external source for all existing avatars, add them to the queue.
			if (alertScripts)
			{
				mRadarEnterAlerts.push_back(avId);
			}
		}
		
		//
		//2d. Prepare data for presentation view for this avatar
		//
		if (regionSelf == avRegion)
		{
			inSameRegion++;
		}

		LLSD entry;
		LLSD entry_options;

		entry["id"] = avId;
		entry["name"] = avName;
		entry["in_region"] = (regionSelf == avRegion);
		entry["flags"] = avFlagStr;
		entry["age"] = avAgeStr;
		entry["seen"] = avSeenStr;
		entry["range"] = (avRange > AVATAR_UNKNOWN_RANGE ? llformat("%3.2f", avRange) : llformat(">%3.2f", drawRadius));

		//AO: Set any range colors / styles
		LLUIColor range_color;
		if (avRange > AVATAR_UNKNOWN_RANGE)
		{
			if (avRange <= chat_range_say)
			{
				range_color = LLUIColorTable::instance().getColor("AvatarListItemChatRange", LLColor4::red);
				inChatRange++;
			}
			else if (avRange <= chat_range_shout)
			{
				range_color = LLUIColorTable::instance().getColor("AvatarListItemShoutRange", LLColor4::white);
			}
			else 
			{
				range_color = LLUIColorTable::instance().getColor("AvatarListItemBeyondShoutRange", LLColor4::white);
			}
		}
		else 
		{
			range_color = LLUIColorTable::instance().getColor("AvatarListItemBeyondShoutRange", LLColor4::white);
		}
		entry_options["range_color"] = range_color.get().getValue();

		// Check if avatar is in draw distance and a VOAvatar instance actually exists
		if (avRange <= drawRadius && avRange > AVATAR_UNKNOWN_RANGE && avVo)
		{
			entry_options["range_style"] = LLFontGL::BOLD;
		}
		else
		{
			entry_options["range_style"] = LLFontGL::NORMAL;
		}

		// Set friends colors / styles
		LLFontGL::StyleFlags nameCellStyle = LLFontGL::NORMAL;
		const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(avId);
		if (relation)
		{
			nameCellStyle = (LLFontGL::StyleFlags)(nameCellStyle | LLFontGL::BOLD);
		}
		if (mutelist->isMuted(avId))
		{
			nameCellStyle = (LLFontGL::StyleFlags)(nameCellStyle | LLFontGL::ITALIC);
		}
		entry_options["name_style"] = nameCellStyle;

		if (LGGContactSets::getInstance()->hasFriendColorThatShouldShow(avId, LGG_CS_RADAR))
		{
			LLColor4 name_color = LGGContactSets::getInstance()->getFriendColor(avId);
			entry_options["name_color"] = name_color.getValue();
		}

		// Voice power level indicator
		LLVoiceClient* voice_client = LLVoiceClient::getInstance();
		if (voice_client->voiceEnabled() && voice_client->isVoiceWorking())
		{
			LLSpeaker* speaker = LLLocalSpeakerMgr::getInstance()->findSpeaker(avId);
			if (speaker && speaker->isInVoiceChannel())
			{
				EVoicePowerLevel power_level = voice_client->getPowerLevel(avId);
			
				switch (power_level)
				{
					case VPL_PTT_Off:
						entry["voice_level_icon"] = "VoicePTT_Off";
						break;
					case VPL_PTT_On:
						entry["voice_level_icon"] = "VoicePTT_On";
						break;
					case VPL_Level1:
						entry["voice_level_icon"] = "VoicePTT_Lvl1";
						break;
					case VPL_Level2:
						entry["voice_level_icon"] = "VoicePTT_Lvl2";
						break;
					case VPL_Level3:
						entry["voice_level_icon"] = "VoicePTT_Lvl3";
						break;
					default:
						break;
				}
			}
		}

		// Save data for our listeners
		LLSD entry_data;
		entry_data["entry"] = entry;
		entry_data["options"] = entry_options;
		mRadarEntriesData.push_back(entry_data);
	} // End STEP 2, all model/presentation row processing complete.
	
	//
	//STEP 3, process any bulk actions that require the whole model to be known first
	//
	
	//
	//3a. dispatch requests for ZOffset updates, working around minimap's inaccurate height
	//
	if (mRadarOffsetRequests.size() > 0)
	{
		std::string prefix = "getZOffsets|";
		std::string msg = "";
		U32 updatesPerRequest=0;
		while (mRadarOffsetRequests.size() > 0)
		{
			LLUUID avId = mRadarOffsetRequests.back();
			mRadarOffsetRequests.pop_back();
			msg = llformat("%s%s,", msg.c_str(), avId.asString().c_str());
			if (++updatesPerRequest > FSRADAR_MAX_OFFSET_REQUESTS)
			{
				msg = msg.substr(0, msg.size() - 1);
				FSLSLBridgeRequestResponder* responder = new FSLSLBridgeRequestRadarPosResponder();
				FSLSLBridge::instance().viewerToLSL(prefix  +msg, responder);
				//llinfos << " OFFSET REQUEST SEGMENT"<< prefix << msg << llendl;
				msg = "";
				updatesPerRequest = 0;
			}
		}
		if (updatesPerRequest > 0)
		{
			msg = msg.substr(0, msg.size() - 1);
			FSLSLBridgeRequestResponder* responder = new FSLSLBridgeRequestRadarPosResponder();
			FSLSLBridge::instance().viewerToLSL(prefix + msg, responder);
			//llinfos << " OFFSET REQUEST FINAL " << prefix << msg << llendl;
		}
		
		// clear out the dispatch queue
		mRadarOffsetRequests.clear();
		mRadarLastBulkOffsetRequestTime = now;
	}
	
	//
	//3b: process alerts for avatars that where here last frame, but gone this frame (ie, they left)
	//    as well as dispatch all earlier detected alerts for crossing range thresholds.
	//
	
	for (std::multimap<LLUUID, radarFields>::const_iterator i = lastRadarSweep.begin(); i != lastRadarSweep.end(); ++i)
	{
		LLUUID prevId = i->first;
		if (!mNearbyList->contains(prevId))
		{
			radarFields rf = i->second;
			if (RadarReportChatRangeLeave && (rf.lastDistance <= chat_range_say) && rf.lastDistance > AVATAR_UNKNOWN_RANGE)
			{
				make_ui_sound("UISndRadarChatLeave"); // <FS:PP> FIRE-6069: Radar alerts sounds
				LLAvatarNameCache::get(prevId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_chat_leaving));
			}
			if (RadarReportDrawRangeLeave && (rf.lastDistance <= drawRadius) && rf.lastDistance > AVATAR_UNKNOWN_RANGE)
			{
				make_ui_sound("UISndRadarDrawLeave"); // <FS:PP> FIRE-6069: Radar alerts sounds
				LLAvatarNameCache::get(prevId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_draw_distance_leaving));
			}
			if (RadarReportSimRangeLeave && (rf.lastRegion == regionSelf))
			{
				make_ui_sound("UISndRadarSimLeave"); // <FS:PP> FIRE-6069: Radar alerts sounds
				LLAvatarNameCache::get(prevId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_region_leaving));
			}
				
			if (RadarLeaveChannelAlert)
			{
				mRadarLeaveAlerts.push_back(prevId);
			}
		}
	}

	static LLCachedControl<S32> RadarAlertChannel(gSavedSettings, "RadarAlertChannel");
	U32 num_entering = mRadarEnterAlerts.size();
	if (num_entering > 0)
	{
		mRadarFrameCount++;
		S32 chan(RadarAlertChannel);
		U32 num_this_pass = min(FSRADAR_MAX_AVATARS_PER_ALERT, num_entering);
		std::string msg = llformat("%d,%d", mRadarFrameCount, num_this_pass);
		U32 loop = 0;
		while (loop < num_entering)
		{
			for (int i = 0; i < num_this_pass; i++)
			{
				msg = llformat("%s,%s", msg.c_str(), mRadarEnterAlerts[loop + i].asString().c_str());
			}
			LLMessageSystem* msgs = gMessageSystem;
			msgs->newMessage("ScriptDialogReply");
			msgs->nextBlock("AgentData");
			msgs->addUUID("AgentID", gAgent.getID());
			msgs->addUUID("SessionID", gAgent.getSessionID());
			msgs->nextBlock("Data");
			msgs->addUUID("ObjectID", gAgent.getID());
			msgs->addS32("ChatChannel", chan);
			msgs->addS32("ButtonIndex", 1);
			msgs->addString("ButtonLabel", msg.c_str());
			gAgent.sendReliableMessage();
			loop += num_this_pass;
			num_this_pass = min(FSRADAR_MAX_AVATARS_PER_ALERT, num_entering - loop);
			msg = llformat("%d,%d", mRadarFrameCount, num_this_pass);
		}
	}
	U32 num_leaving  = mRadarLeaveAlerts.size();
	if (num_leaving > 0)
	{
		mRadarFrameCount++;
		S32 chan(RadarAlertChannel);
		U32 num_this_pass = min(FSRADAR_MAX_AVATARS_PER_ALERT, num_leaving);
		std::string msg = llformat("%d,-%d", mRadarFrameCount, min(FSRADAR_MAX_AVATARS_PER_ALERT, num_leaving));
		U32 loop = 0;
		while (loop < num_leaving)
		{
			for (int i = 0; i < num_this_pass; i++)
			{
				msg = llformat("%s,%s", msg.c_str(), mRadarLeaveAlerts[loop + i].asString().c_str());
			}
			LLMessageSystem* msgs = gMessageSystem;
			msgs->newMessage("ScriptDialogReply");
			msgs->nextBlock("AgentData");
			msgs->addUUID("AgentID", gAgent.getID());
			msgs->addUUID("SessionID", gAgent.getSessionID());
			msgs->nextBlock("Data");
			msgs->addUUID("ObjectID", gAgent.getID());
			msgs->addS32("ChatChannel", chan);
			msgs->addS32("ButtonIndex", 1);
			msgs->addString("ButtonLabel", msg.c_str());
			gAgent.sendReliableMessage();
			loop += num_this_pass;
			num_this_pass = min(FSRADAR_MAX_AVATARS_PER_ALERT, num_leaving - loop);
			msg = llformat("%d,-%d", mRadarFrameCount, num_this_pass);
		}
	}

	// reset any active alert requests
	if (alertScripts)
	{
		mRadarAlertRequest = false;
	}

	//
	//STEP 4: Cache our current model data, so we can compare it with the next fresh group of model data for fast change detection.
	//
	
	lastRadarSweep.clear();
	for (std::vector<LLPanel*>::const_iterator itItem = items.begin(); itItem != items.end(); ++itItem)
	{
		LLAvatarListItem* av = static_cast<LLAvatarListItem*>(*itItem);
		radarFields rf;
		rf.avName = av->getAvatarName();
		rf.lastDistance = av->getRange();
		rf.firstSeen = av->getFirstSeen();
		rf.lastStatus = av->getAvStatus();
		rf.ZOffset = av->getZOffset();
		rf.lastGlobalPos = av->getPosition();
		// Ansariel: This seems to be wrong and isn't needed anywhere
		//if ((rf.ZOffset > 0) && (rf.lastGlobalPos[VZ] < 1024)) // if our position may need an offset correction, see if we have one to apply
		//{
		//	rf.lastGlobalPos[VZ] = rf.lastGlobalPos[VZ] + (1024 * rf.ZOffset);
		//}
		//rf.lastZOffsetTime = av->getLastZOffsetTime();
		if (rf.lastGlobalPos != LLVector3d(0.0f, 0.0f, 0.0f))
		{
			LLViewerRegion* lastRegion = world->getRegionFromPosGlobal(rf.lastGlobalPos);
			if (lastRegion)
			{
				rf.lastRegion = lastRegion->getRegionID();
			}
		}
		else
		{
			rf.lastRegion = LLUUID(0);
		}
		
		lastRadarSweep.insert(std::pair<LLUUID, radarFields>(av->getAvatarId(), rf));
	}

	//
	//STEP 5: Final data updates and notification of subscribers
	//

	mAvatarStats["total"] = llformat("%d", lastRadarSweep.size());
	mAvatarStats["region"] = llformat("%d", inSameRegion);
	mAvatarStats["chatrange"] = llformat("%d", inChatRange);

	checkTracking();

	// Inform our subscribers about updates
	if (!mUpdateSignal.empty())
	{
		mUpdateSignal(mRadarEntriesData, mAvatarStats);
	}
}

void FSRadar::requestRadarChannelAlertSync()
{
	F32 timeNow = gFrameTimeSeconds;
	if ((timeNow - FSRADAR_CHAT_MIN_SPACING) > mRadarLastRequestTime)
	{
		mRadarLastRequestTime = timeNow;
		mRadarAlertRequest = true;
	}
}

void FSRadar::teleportToAvatar(const LLUUID& targetAv)
// Teleports user to last scanned location of nearby avatar
// Note: currently teleportViaLocation is disrupted by enforced landing points set on a parcel.
{
	std::vector<LLPanel*> items;
	mNearbyList->getItems(items);
	for (std::vector<LLPanel*>::const_iterator itItem = items.begin(); itItem != items.end(); ++itItem)
	{
		LLAvatarListItem* av = static_cast<LLAvatarListItem*>(*itItem);
		if (av->getAvatarId() == targetAv)
		{
			LLVector3d avpos = av->getPosition();
			if (avpos.mdV[VZ] == AVATAR_UNKNOWN_Z_OFFSET)
			{
				LLNotificationsUtil::add("TeleportToAvatarNotPossible");
			}
			else
			{
				gAgent.teleportViaLocation(avpos);
			}
			return;
		}
	}
}

//static
void FSRadar::onRadarNameFmtClicked(const LLSD& userdata)
{
	std::string chosen_item = userdata.asString();
	if (chosen_item == "DN")
	{
		gSavedSettings.setU32("RadarNameFormat", FSRADAR_NAMEFORMAT_DISPLAYNAME);
	}
	else if (chosen_item == "UN")
	{
		gSavedSettings.setU32("RadarNameFormat", FSRADAR_NAMEFORMAT_USERNAME);
	}
	else if (chosen_item == "DNUN")
	{
		gSavedSettings.setU32("RadarNameFormat", FSRADAR_NAMEFORMAT_DISPLAYNAME_USERNAME);
	}
	else if (chosen_item == "UNDN")
	{
		gSavedSettings.setU32("RadarNameFormat", FSRADAR_NAMEFORMAT_USERNAME_DISPLAYNAME);
	}
}

//static
bool FSRadar::radarNameFmtCheck(const LLSD& userdata)
{
	std::string menu_item = userdata.asString();
	U32 name_format = gSavedSettings.getU32("RadarNameFormat");
	switch (name_format)
	{
		case FSRADAR_NAMEFORMAT_DISPLAYNAME:
			return (menu_item == "DN");
		case FSRADAR_NAMEFORMAT_USERNAME:
			return (menu_item == "UN");
		case FSRADAR_NAMEFORMAT_DISPLAYNAME_USERNAME:
			return (menu_item == "DNUN");
		case FSRADAR_NAMEFORMAT_USERNAME_DISPLAYNAME:
			return (menu_item == "UNDN");
		default:
			break;
	}
	return false;
}

// <FS:CR> Milkshake-style radar alerts
//static
void FSRadar::onRadarReportToClicked(const LLSD& userdata)
{
	std::string chosen_item = userdata.asString();
	if (chosen_item == "radar_toasts")
	{
		gSavedSettings.setBOOL("FSMilkshakeRadarToasts", TRUE);
	}
	else if (chosen_item == "radar_nearby_chat")
	{
		gSavedSettings.setBOOL("FSMilkshakeRadarToasts", FALSE);
	}
}

//static
bool FSRadar::radarReportToCheck(const LLSD& userdata)
{
	std::string menu_item = userdata.asString();
	bool report_to = gSavedSettings.getBOOL("FSMilkshakeRadarToasts");
	if (report_to)
	{
		return (menu_item == "radar_toasts");
	}
	else
	{
		return (menu_item == "radar_nearby_chat");
	}
}
// </FS:CR>

std::string FSRadar::getRadarName(const LLAvatarName& avname)
{
// [RLVa:KB-FS] - Checked: 2011-06-11 (RLVa-1.3.1) | Added: RLVa-1.3.1
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		return RlvStrings::getAnonym(avname);
	}
// [/RLVa:KB-FS]

	U32 fmt = gSavedSettings.getU32("RadarNameFormat");
	// if display names are enabled, allow a variety of formatting options, depending on menu selection
	if (gSavedSettings.getBOOL("UseDisplayNames"))
	{	
		if (fmt == FSRADAR_NAMEFORMAT_DISPLAYNAME)
		{
			return avname.mDisplayName;
		}
		else if (fmt == FSRADAR_NAMEFORMAT_USERNAME)
		{
			return avname.mUsername;
		}
		else if (fmt == FSRADAR_NAMEFORMAT_DISPLAYNAME_USERNAME)
		{
			std::string s1 = avname.mDisplayName;
			to_lower(s1);
			std::string s2 = avname.mUsername;
			replace_all(s2, ".", " ");
			if (s1.compare(s2) == 0)
			{
				return avname.mDisplayName;
			}
			else
			{
				return llformat("%s (%s)", avname.mDisplayName.c_str(), avname.mUsername.c_str());
			}
		}
		else if (fmt == FSRADAR_NAMEFORMAT_USERNAME_DISPLAYNAME)
		{
			std::string s1 = avname.mDisplayName;
			to_lower(s1);
			std::string s2 = avname.mUsername;
			replace_all(s2, ".", " ");
			if (s1.compare(s2) == 0)
			{
				return avname.mDisplayName;
			}
			else
			{
				return llformat("%s (%s)", avname.mUsername.c_str(), avname.mDisplayName.c_str());
			}
		}
	}
	
	// else use legacy name lookups
	return avname.mDisplayName; // will be mapped to legacyname automatically by the name cache
}

std::string FSRadar::getRadarName(const LLUUID& avId)
{
	LLAvatarName avname;

	if (LLAvatarNameCache::get(avId, &avname)) // use the synchronous call. We poll every second so there's less value in using the callback form.
	{
		return getRadarName(avname);
	}

	// name not found. Temporarily fill in with the UUID. It's more distinguishable than (loading...)
	return avId.asString();
}

void FSRadar::startTracking(const LLUUID& avatar_id)
{
	mTrackedAvatarId = avatar_id;
	updateTracking();
}

void FSRadar::checkTracking()
{
	if (LLTracker::getTrackingStatus() == LLTracker::TRACKING_LOCATION
		&& LLTracker::getTrackedLocationType() == LLTracker::LOCATION_AVATAR)
	{
		updateTracking();
	}
}

void FSRadar::updateTracking()
{
	std::multimap<LLUUID, radarFields>::const_iterator it;
	it = lastRadarSweep.find(mTrackedAvatarId);
	if (it != lastRadarSweep.end())
	{
		if (LLTracker::getTrackedPositionGlobal() != it->second.lastGlobalPos)
		{
			std::string targetName(it->second.avName);
			if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
			{
				targetName = RlvStrings::getAnonym(targetName);
			}
			LLTracker::trackLocation(it->second.lastGlobalPos, targetName, "", LLTracker::LOCATION_AVATAR);
		}
	}
	else
	{
		LLTracker::stopTracking(NULL);
	}
}

void FSRadar::zoomAvatar(const LLUUID& avatar_id, const std::string& name)
{
	LLAvatarListItem* avl_item = mNearbyList->getAvatarListItem(avatar_id);

	if (!avl_item)
	{
		return;
	}

	if (avl_item->getRange() <= gSavedSettings.getF32("RenderFarClip"))
	{
		handle_zoom_to_object(avatar_id, avl_item->getPosition());
	}
	else
	{
		LLStringUtil::format_map_t args;
		args["AVATARNAME"] = name.c_str();
		reportToNearbyChat(LLTrans::getString("camera_no_focus", args));
	}
}
