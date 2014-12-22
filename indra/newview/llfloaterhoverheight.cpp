/** 
* @file llfloaterhoverheight.cpp
* @brief Controller for self avatar hover height
* @author vir@lindenlab.com
*
* $LicenseInfo:firstyear=2014&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2014, Linden Research, Inc.
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

#include "llfloaterhoverheight.h"
#include "llsliderctrl.h"
#include "llviewercontrol.h"
#include "llsdserialize.h"
#include "llhttpclient.h"
#include "llagent.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"

LLFloaterHoverHeight::LLFloaterHoverHeight(const LLSD& key) : LLFloater(key)
{
}

void LLFloaterHoverHeight::syncFromPreferenceSetting(void *user_data)
{
	LLVector3 offset = gSavedPerAccountSettings.getVector3("AvatarPosFinalOffset");
	F32 value = offset[2];

	LLFloaterHoverHeight *self = static_cast<LLFloaterHoverHeight*>(user_data);
	LLSliderCtrl* sldrCtrl = self->getChild<LLSliderCtrl>("HoverHeightSlider");
	sldrCtrl->setValue(value,FALSE);
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->sendHoverHeight();
	}
}

BOOL LLFloaterHoverHeight::postBuild()
{
	LLSliderCtrl* sldrCtrl = getChild<LLSliderCtrl>("HoverHeightSlider");
	sldrCtrl->setSliderMouseUpCallback(boost::bind(&LLFloaterHoverHeight::onFinalCommit,this));
	sldrCtrl->setSliderEditorCommitCallback(boost::bind(&LLFloaterHoverHeight::onFinalCommit,this));
	childSetCommitCallback("HoverHeightSlider", &LLFloaterHoverHeight::onSliderMoved, NULL);

	// Initialize slider from pref setting.
	syncFromPreferenceSetting(this);
	// Update slider on future pref changes.
	if (gSavedPerAccountSettings.getControl("AvatarPosFinalOffset"))
	{
		gSavedPerAccountSettings.getControl("AvatarPosFinalOffset")->getCommitSignal()->connect(boost::bind(&syncFromPreferenceSetting, this));
	}
	else
	{
		LL_WARNS() << "Control not found for AvatarPosFinalOffset" << LL_ENDL;
	}

	return TRUE;
}

// static
void LLFloaterHoverHeight::onSliderMoved(LLUICtrl* ctrl, void* userData)
{
	LLSliderCtrl* sldrCtrl = static_cast<LLSliderCtrl*>(ctrl);
	F32 value = sldrCtrl->getValueF32();
	LLVector3 offset = gSavedSettings.getVector3("AvatarPosFinalOffset");
	offset[2] = value;
	gAgentAvatarp->mHoverOffset = offset;
}

// Do send-to-the-server work when slider drag completes, or new
// value entered as text.
void LLFloaterHoverHeight::onFinalCommit()
{
	LLSliderCtrl* sldrCtrl = getChild<LLSliderCtrl>("HoverHeightSlider");
	F32 value = sldrCtrl->getValueF32();
	LLVector3 offset = gSavedPerAccountSettings.getVector3("AvatarPosFinalOffset");
	offset[2] = value;
	gSavedPerAccountSettings.setVector3("AvatarPosFinalOffset",offset);
}


