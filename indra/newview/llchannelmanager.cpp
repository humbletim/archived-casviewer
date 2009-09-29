/** 
 * @file llchannelmanager.cpp
 * @brief This class rules screen notification channels.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h" // must be first include

#include "llchannelmanager.h"

#include "llappviewer.h"
#include "llviewercontrol.h"
#include "llimview.h"
#include "llbottomtray.h"
#include "llviewerwindow.h"

#include <algorithm>

using namespace LLNotificationsUI;

//--------------------------------------------------------------------------
LLChannelManager::LLChannelManager()
{
	LLAppViewer::instance()->setOnLoginCompletedCallback(boost::bind(&LLChannelManager::onLoginCompleted, this));
	mChannelList.clear();
	mStartUpChannel = NULL;
	
	if(!gViewerWindow)
	{
		llerrs << "LLChannelManager::LLChannelManager() - viwer window is not initialized yet" << llendl;
	}
}

//--------------------------------------------------------------------------
LLChannelManager::~LLChannelManager()
{
	for(std::vector<ChannelElem>::iterator it = mChannelList.begin(); it !=  mChannelList.end(); ++it)
	{
		delete (*it).channel;
	}

	mChannelList.clear();
}

//--------------------------------------------------------------------------
LLScreenChannel* LLChannelManager::createNotificationChannel()
{
	//  creating params for a channel
	LLChannelManager::Params p;
	p.id = LLUUID(gSavedSettings.getString("NotificationChannelUUID"));
	p.channel_align = CA_RIGHT;

	// Getting a Channel for our notifications
	return LLChannelManager::getInstance()->getChannel(p);
}

//--------------------------------------------------------------------------
void LLChannelManager::onLoginCompleted()
{
	S32 away_notifications = 0;

	// calc a number of all offline notifications
	for(std::vector<ChannelElem>::iterator it = mChannelList.begin(); it !=  mChannelList.end(); ++it)
	{
		// don't calc notifications for Nearby Chat
		if((*it).channel->getChannelID() == LLUUID(gSavedSettings.getString("NearByChatChannelUUID")))
		{
			continue;
		}

		// don't calc notifications for channels that always show their notifications
		if(!(*it).channel->getDisplayToastsAlways())
		{
			away_notifications +=(*it).channel->getNumberOfHiddenToasts();
		}
	}

	away_notifications += gIMMgr->getNumberOfUnreadIM();

	if(!away_notifications)
	{
		onStartUpToastClose();
		return;
	}
	
	// create a channel for the StartUp Toast
	LLChannelManager::Params p;
	p.id = LLUUID(gSavedSettings.getString("StartUpChannelUUID"));
	p.channel_align = CA_RIGHT;
	mStartUpChannel = getChannel(p);

	if(!mStartUpChannel)
	{
		onStartUpToastClose();
		return;
	}

	// init channel's position and size
	S32 channel_right_bound = gViewerWindow->getWorldViewRect().mRight - gSavedSettings.getS32("NotificationChannelRightMargin"); 
	S32 channel_width = gSavedSettings.getS32("NotifyBoxWidth");
	mStartUpChannel->init(channel_right_bound - channel_width, channel_right_bound);
	mStartUpChannel->setShowToasts(true);

	mStartUpChannel->setCommitCallback(boost::bind(&LLChannelManager::onStartUpToastClose, this));
	mStartUpChannel->createStartUpToast(away_notifications, gSavedSettings.getS32("ChannelBottomPanelMargin"), gSavedSettings.getS32("StartUpToastTime"));
}

//--------------------------------------------------------------------------
void LLChannelManager::onStartUpToastClose()
{
	if(mStartUpChannel)
	{
		mStartUpChannel->setVisible(FALSE);
		mStartUpChannel->closeStartUpToast();
		removeChannelByID(LLUUID(gSavedSettings.getString("StartUpChannelUUID")));
		delete mStartUpChannel;
		mStartUpChannel = NULL;
	}

	// set StartUp Toast Flag to allow all other channels to show incoming toasts
	LLScreenChannel::setStartUpToastShown();

	// force NEARBY CHAT CHANNEL to repost all toasts if present
	LLScreenChannel* nearby_channel = findChannelByID(LLUUID(gSavedSettings.getString("NearByChatChannelUUID")));
	nearby_channel->loadStoredToastsToChannel();
	nearby_channel->setCanStoreToasts(false);
}

//--------------------------------------------------------------------------
LLScreenChannel* LLChannelManager::getChannel(LLChannelManager::Params& p)
{
	LLScreenChannel* new_channel = NULL;

	new_channel = findChannelByID(p.id);

	if(new_channel)
		return new_channel;

	new_channel = new LLScreenChannel(p.id); 

	if(!new_channel)
	{
		llerrs << "LLChannelManager::getChannel(LLChannelManager::Params& p) - can't create a channel!" << llendl;		
	}
	else
	{
		new_channel->setToastAlignment(p.toast_align);
		new_channel->setChannelAlignment(p.channel_align);
	new_channel->setDisplayToastsAlways(p.display_toasts_always);

	ChannelElem new_elem;
	new_elem.id = p.id;
	new_elem.channel = new_channel;

		mChannelList.push_back(new_elem); 
	}

	return new_channel;
}

//--------------------------------------------------------------------------
LLScreenChannel* LLChannelManager::findChannelByID(const LLUUID id)
{
	std::vector<ChannelElem>::iterator it = find(mChannelList.begin(), mChannelList.end(), id); 
	if(it != mChannelList.end())
	{
		return (*it).channel;
	}

	return NULL;
}

//--------------------------------------------------------------------------
void LLChannelManager::removeChannelByID(const LLUUID id)
{
	std::vector<ChannelElem>::iterator it = find(mChannelList.begin(), mChannelList.end(), id); 
	if(it != mChannelList.end())
	{
		mChannelList.erase(it);
	}
}

//--------------------------------------------------------------------------


