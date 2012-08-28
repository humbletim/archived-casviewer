/**
 * @file llfloaterconversationpreview.cpp
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
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

#include "llconversationlog.h"
#include "llfloaterconversationpreview.h"
#include "llimview.h"
#include "lllineeditor.h"

LLFloaterConversationPreview::LLFloaterConversationPreview(const LLSD& session_id)
:	LLFloater(session_id),
	mChatHistory(NULL),
	mSessionID(session_id.asUUID()),
	mCurrentPage(0),
	mPageSize(gSavedSettings.getS32("ConversationHistoryPageSize"))
{}

BOOL LLFloaterConversationPreview::postBuild()
{
	mChatHistory = getChild<LLChatHistory>("chat_history");
	getChild<LLUICtrl>("more_history")->setCommitCallback(boost::bind(&LLFloaterConversationPreview::onMoreHistoryBtnClick, this));

	const LLConversation* conv = LLConversationLog::instance().getConversation(mSessionID);
	if (conv)
	{
		std::string name = conv->getConversationName();
		LLStringUtil::format_map_t args;
		args["[NAME]"] = name;
		std::string title = getString("Title", args);
		setTitle(title);

		getChild<LLLineEditor>("description")->setValue(name);
	}

	std::string file = conv->getHistoryFileName();
	LLLogChat::loadChatHistory(file, mMessages, true);

	mCurrentPage = mMessages.size() / mPageSize;

	return LLFloater::postBuild();
}

void LLFloaterConversationPreview::draw()
{
	LLFloater::draw();
}

void LLFloaterConversationPreview::onOpen(const LLSD& session_id)
{
	showHistory();
}

void LLFloaterConversationPreview::showHistory()
{
	if (!mMessages.size())
	{
		return;
	}

	mChatHistory->clear();

	std::ostringstream message;
	std::list<LLSD>::const_iterator iter = mMessages.begin();

	int delta = 0;
	if (mCurrentPage)
	{
		double num_of_pages = (double)mMessages.size() / mPageSize;
		delta = (ceil(num_of_pages) - num_of_pages) * mPageSize;
	}

	std::advance(iter, (mCurrentPage * mPageSize) - delta);

	for (int msg_num = 0; (iter != mMessages.end() && msg_num < mPageSize); ++iter, ++msg_num)
	{
		LLSD msg = *iter;

		std::string time	= msg["time"].asString();
		LLUUID from_id		= msg["from_id"].asUUID();
		std::string from	= msg["from"].asString();
		std::string message	= msg["message"].asString();
		bool is_history		= msg["is_history"].asBoolean();

		LLChat chat;
		chat.mFromID = from_id;
		chat.mSessionID = mSessionID;
		chat.mFromName = from;
		chat.mTimeStr = time;
		chat.mChatStyle = is_history ? CHAT_STYLE_HISTORY : chat.mChatStyle;
		chat.mText = message;

		mChatHistory->appendMessage(chat);
	}

}

void LLFloaterConversationPreview::onMoreHistoryBtnClick()
{
	if (--mCurrentPage < 0)
	{
		return;
	}

	showHistory();
}
