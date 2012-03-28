/** 
 * @file llnotificationmanager.cpp
 * @brief Class implements a brige between the old and a new notification sistems
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
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



#include "llviewerprecompiledheaders.h" // must be first include


#include "llnotificationmanager.h"

#include "llnearbychathandler.h"
#include "llnotifications.h"

#include <boost/bind.hpp>

using namespace LLNotificationsUI;

//--------------------------------------------------------------------------
LLNotificationManager::LLNotificationManager()
{
	init();
}

//--------------------------------------------------------------------------
LLNotificationManager::~LLNotificationManager()
{
}

//--------------------------------------------------------------------------
void LLNotificationManager::init()
{
	new LLIMHandler();
	new LLScriptHandler();
	new LLTipHandler();
	new LLGroupHandler();
	new LLAlertHandler("Alerts", "alert", false);
	new LLAlertHandler("AlertModal", "alertmodal", true);
	new LLOfferHandler();
	new LLHintHandler();
	new LLBrowserNotification();
	new LLOutboxNotification();
	
	mChatHandler = boost::shared_ptr<LLNearbyChatHandler>(new LLNearbyChatHandler());
}

//--------------------------------------------------------------------------
void LLNotificationManager::onChat(const LLChat& msg, const LLSD &args)
{
	if(mChatHandler)
		mChatHandler->processChat(msg, args);
}

