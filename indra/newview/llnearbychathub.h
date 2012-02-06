 /** 
 * @file llnearbychat.h
 * @brief Nearby chat central class for handling multiple chat input controls
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2012, Zi Ree @ Second Life
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

#ifndef LL_LLNEARBYCHAT_H
#define LL_LLNEARBYCHAT_H

#include "llsingleton.h"
#include "llviewerchat.h"

class LLNearbyChatControl;

class LLNearbyChat : public LLSingleton<LLNearbyChat>
{
	friend class LLSingleton<LLNearbyChat>;

private:
	LLNearbyChat();
	~LLNearbyChat();

	void sendMsg();

	static S32 sLastSpecialChatChannel;
	LLNearbyChatControl* mDefaultChatBar;

public:
	void registerChatBar(LLNearbyChatControl* chatBar);
	void showDefaultChatBar() const;

	void sendChat(LLWString text,EChatType type);
	LLWString stripChannelNumber(const LLWString &mesg, S32* channel);
	EChatType processChatTypeTriggers(EChatType type, std::string &str);
	void sendChatFromViewer(const std::string& utf8text, EChatType type, BOOL animate);
	void sendChatFromViewer(const LLWString& wtext, EChatType type, BOOL animate);

	void setFocusedInputEditor(LLNearbyChatControl* inputEditor,BOOL focus);

	BOOL chatIsEmpty() const;

	LLNearbyChatControl* mFocusedInputEditor;
};

#endif
