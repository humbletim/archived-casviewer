/** 
 * @file fsfloaterim.cpp
 * @brief FSFloaterIM class definition
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

// Original file: llimfloater.cpp

#include "llviewerprecompiledheaders.h"

#include "fsfloaterim.h"

#include "llnotificationsutil.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llchannelmanager.h"
#include "llchiclet.h"
#include "llchicletbar.h"
#include "llfloaterabout.h"		// for sysinfo button -Zi
#include "llfloaterreg.h"
#include "fsfloaterimcontainer.h" // to replace separate IM Floaters with multifloater container
#include "llhttpclient.h"
#include "llinventoryfunctions.h"
#include "lllayoutstack.h"
#include "llchatentry.h"
#include "lllogchat.h"
#include "fspanelimcontrolpanel.h"
#include "llscreenchannel.h"
#include "llsyswellwindow.h"
#include "lltrans.h"
#include "fschathistory.h"
#include "llnotifications.h"
#include "llviewerwindow.h"
#include "llvoicechannel.h"
#include "lltransientfloatermgr.h"
#include "llinventorymodel.h"
#include "llrootview.h"
#include "llspeakers.h"
#include "llviewerchat.h"
#include "llautoreplace.h"
// [RLVa:KB] - Checked: 2010-04-09 (RLVa-1.2.0e)
#include "rlvhandler.h"
#include "rlvactions.h"	// <FS:CR> CHUI merge
// [/RLVa:KB]

//AO: For moving callbacks from control panel into this class
#include "llavataractions.h"
#include "llgroupactions.h"
//TL: for support group chat prefix
#include "fsdata.h"
#include "llversioninfo.h"
#include "llcheckboxctrl.h"

#include "llnotificationtemplate.h"		// <FS:Zi> Viewer version popup
#include "fscommon.h"
#include "fsfloaternearbychat.h"
#include "llviewerregion.h"

const F32 ME_TYPING_TIMEOUT = 4.0f;
const F32 OTHER_TYPING_TIMEOUT = 9.0f;

floater_showed_signal_t FSFloaterIM::sIMFloaterShowedSignal;

FSFloaterIM::FSFloaterIM(const LLUUID& session_id)
  : LLTransientDockableFloater(NULL, true, session_id),
	mControlPanel(NULL),
	mSessionID(session_id),
	mLastMessageIndex(-1),
	mDialog(IM_NOTHING_SPECIAL),
	mChatHistory(NULL),
	mInputEditor(NULL),
	mSavedTitle(),
	mTypingStart(),
	mShouldSendTypingState(false),
	mMeTyping(false),
	mOtherTyping(false),
	mTypingTimer(),
	mTypingTimeoutTimer(),
//	mPositioned(false),			// dead code -Zi
	mSessionInitialized(false),
	mChatLayoutPanel(NULL),
	mInputPanels(NULL),
	mChatLayoutPanelHeight(0),
	mAvatarNameCacheConnection(),
	mVoiceChannel(NULL),
	mMeTypingTimer(),
	mOtherTypingTimer()
{
	LLIMModel::LLIMSession* im_session = LLIMModel::getInstance()->findIMSession(mSessionID);
	if (im_session)
	{
		mSessionInitialized = im_session->mSessionInitialized;
		
		mDialog = im_session->mType;
		switch(mDialog){
		case IM_NOTHING_SPECIAL:
		case IM_SESSION_P2P_INVITE:
			mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelIMControl, this);
			break;
		case IM_SESSION_CONFERENCE_START:
			mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelAdHocControl, this);
			break;
		case IM_SESSION_GROUP_START:
			mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelGroupControl, this);
			break;
		case IM_SESSION_INVITE:		
			if (gAgent.isInGroup(mSessionID))
			{
				mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelGroupControl, this);
			}
			else
			{
				mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelAdHocControl, this);
			}
			break;
		default: break;
		}
	}
	
	mCommitCallbackRegistrar.add("IMSession.Menu.Action", boost::bind(&FSFloaterIM::doToSelected, this, _2));
	mEnableCallbackRegistrar.add("IMSession.Menu.Enable", boost::bind(&FSFloaterIM::checkEnabled, this, _2));
	
	setOverlapsScreenChannel(true);

	LLTransientFloaterMgr::getInstance()->addControlView(LLTransientFloaterMgr::IM, this);

	// only dock when chiclets are visible, or the floater will get stuck in the top left
	// FIRE-9984 -Zi
	setDocked(!gSavedSettings.getBOOL("FSDisableIMChiclets"));
	// make sure to save position and size with chiclets disabled (torn off floater does that)
	setTornOff(gSavedSettings.getBOOL("FSDisableIMChiclets"));
}

// virtual
BOOL FSFloaterIM::focusFirstItem(BOOL prefer_text_fields, BOOL focus_flash)
{
	mInputEditor->setFocus(TRUE);
	onTabInto();
	if(focus_flash)
	{
		gFocusMgr.triggerFocusFlash();
	}
	return TRUE;
}

void FSFloaterIM::onFocusLost()
{
	LLIMModel::getInstance()->resetActiveSessionID();
	
	LLChicletBar::getInstance()->getChicletPanel()->setChicletToggleState(mSessionID, false);
}

void FSFloaterIM::onFocusReceived()
{
	LLIMModel::getInstance()->setActiveSessionID(mSessionID);

	LLChicletBar::getInstance()->getChicletPanel()->setChicletToggleState(mSessionID, true);

	if (getVisible())
	{
		LLIMModel::instance().sendNoUnreadMessages(mSessionID);
	}
}

// virtual
void FSFloaterIM::onClose(bool app_quitting)
{
	setTyping(false);

	// The source of much argument and design thrashing
	// Should the window hide or the session close when the X is clicked?
	//
	// Last change:
	// EXT-3516 X Button should end IM session, _ button should hide
	
	
	// AO: Make sure observers are removed on close
	mVoiceChannelStateChangeConnection.disconnect();
	if(LLVoiceClient::instanceExists())
	{
		LLVoiceClient::getInstance()->removeObserver((LLVoiceClientStatusObserver*)this);
	}
	
	//<FS:ND> FIRE-6077 et al: Always clean up observers when the floater dies
	LLAvatarTracker::instance().removeParticularFriendObserver(mOtherParticipantUUID, this);
	//</FS:ND> FIRE-6077 et al
	
	gIMMgr->leaveSession(mSessionID);
}

/* static */
void FSFloaterIM::newIMCallback(const LLSD& data){
	
	if (data["num_unread"].asInteger() > 0 || data["from_id"].asUUID().isNull())
	{
		LLUUID session_id = data["session_id"].asUUID();

		FSFloaterIM* floater = LLFloaterReg::findTypedInstance<FSFloaterIM>("fs_impanel", session_id);
		if (floater == NULL) return;

        // update if visible, otherwise will be updated when opened
		if (floater->getVisible())
		{
			floater->updateMessages();
		}
	}
}

void FSFloaterIM::onVisibilityChange(BOOL new_visibility)
{
	LLVoiceChannel* voice_channel = LLIMModel::getInstance()->getVoiceChannel(mSessionID);

	if (new_visibility && voice_channel &&
		voice_channel->getState() == LLVoiceChannel::STATE_CONNECTED)
	{
		LLFloaterReg::showInstance("voice_call", mSessionID);
	}
	else
	{
		LLFloaterReg::hideInstance("voice_call", mSessionID);
	}
}

void FSFloaterIM::onSendMsg( LLUICtrl* ctrl, void* userdata )
{
	FSFloaterIM* self = (FSFloaterIM*) userdata;
	self->sendMsgFromInputEditor();
	self->setTyping(false);
}

void FSFloaterIM::sendMsgFromInputEditor()
{
	if (gAgent.isGodlike()
		|| (mDialog != IM_NOTHING_SPECIAL)
		|| !mOtherParticipantUUID.isNull())
	{
		// <FS:Techwolf Lupindo> fsdata support
		if(mDialog == IM_NOTHING_SPECIAL && FSData::instance().isSupport(mOtherParticipantUUID) && FSData::instance().isAgentFlag(gAgentID, FSData::NO_SUPPORT))
		{
			return;
		}
		// </FS:Techwolf Lupindo>
		
		if (mInputEditor)
		{
			LLWString text = mInputEditor->getWText();
			LLWStringUtil::trim(text);
			LLWStringUtil::replaceChar(text,182,'\n'); // Convert paragraph symbols back into newlines.
			if(!text.empty())
			{
				// Truncate and convert to UTF8 for transport
				std::string utf8_text = wstring_to_utf8str(text);
				
				// Convert OOC and MU* style poses
				utf8_text = applyAutoCloseOoc(utf8_text);
				utf8_text = applyMuPose(utf8_text);
				
				// <FS:Techwolf Lupindo> Support group chat prefix
				static LLCachedControl<bool> chat_prefix(gSavedSettings, "FSSupportGroupChatPrefix2");
				if (chat_prefix && FSData::getInstance()->isSupportGroup(mSessionID))
				{
					
					// <FS:PP> FIRE-7075: Skin indicator
					static LLCachedControl<std::string> FSInternalSkinCurrent(gSavedSettings, "FSInternalSkinCurrent");
					std::string skinIndicator(FSInternalSkinCurrent);
					LLStringUtil::toLower(skinIndicator);
					if (skinIndicator == "starlight cui")
					{
						skinIndicator = "sc"; // Separate "s" (StarLight) from "sc" (StarLight CUI)
					}
					else
					{
						skinIndicator = skinIndicator.substr(0, 1); // "FS 4.4.1f os", "FS 4.4.1v", "FS 4.4.1a", "FS 4.4.1s os", "FS 4.4.1m os" etc.
					}
					// </FS:PP>

#if !defined(ND_BUILD64BIT_ARCH)
					std::string strFSTag = "(CAS ";
#else
					std::string strFSTag = "(CAS64 ";
#endif
					if (utf8_text.find("/me ") == 0 || utf8_text.find("/me'") == 0)
					{
						utf8_text.insert(4,(strFSTag + LLVersionInfo::getShortVersion() + skinIndicator +
#ifdef OPENSIM
											" os" +
#endif
											") "));
					}
					else
					{
						utf8_text.insert(0,(strFSTag + LLVersionInfo::getShortVersion() + skinIndicator +
#ifdef OPENSIM
											" os" +
#endif
											") "));
					}
				}
				
				// <FS:Techwolf Lupindo> Allow user to send system info.
				if (mDialog == IM_NOTHING_SPECIAL && utf8_text.find("/sysinfo") == 0)
				{
					LLSD system_info = FSData::getSystemInfo();
					utf8_text = system_info["Part1"].asString() + system_info["Part2"].asString();
				}
				// </FS:Techwolf Lupindo>
				
				sendMsg(utf8_text);
				
				mInputEditor->setText(LLStringUtil::null);
			}
		}
	}
	else
	{
		LL_INFOS("FSFloaterIM") << "Cannot send IM to everyone unless you're a god." << LL_ENDL;
	}
}

void FSFloaterIM::sendMsg(const std::string& msg)
{
	//	const std::string utf8_text = utf8str_truncate(msg, MAX_MSG_BUF_SIZE - 1);
	// [RLVa:KB] - Checked: 2010-11-30 (RLVa-1.3.0)
	// <FS:CR> Don't truncate our messages, they're broken up as part of FIRE-787
	//std::string utf8_text = utf8str_truncate(msg, MAX_MSG_BUF_SIZE - 1);
	std::string utf8_text = msg;
	// </FS:CR>
	
	if ( (RlvActions::hasBehaviour(RLV_BHVR_SENDIM)) || (RlvActions::hasBehaviour(RLV_BHVR_SENDIMTO)) )
	{
		const LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
		RLV_ASSERT(pIMSession);
		
		bool fRlvFilter = !pIMSession;
		if (pIMSession)
		{
			switch (pIMSession->mSessionType)
			{
				case LLIMModel::LLIMSession::P2P_SESSION:	// One-on-one IM
					fRlvFilter = !RlvActions::canSendIM(mOtherParticipantUUID);
					break;
				case LLIMModel::LLIMSession::GROUP_SESSION:	// Group chat
					fRlvFilter = !RlvActions::canSendIM(mSessionID);
					break;
				case LLIMModel::LLIMSession::ADHOC_SESSION:	// Conference chat: allow if all participants can be sent an IM
				{
					if (!pIMSession->mSpeakers)
					{
						fRlvFilter = true;
						break;
					}
					
					LLSpeakerMgr::speaker_list_t speakers;
					pIMSession->mSpeakers->getSpeakerList(&speakers, TRUE);
					for (LLSpeakerMgr::speaker_list_t::const_iterator itSpeaker = speakers.begin();
						 itSpeaker != speakers.end(); ++itSpeaker)
					{
						const LLSpeaker* pSpeaker = *itSpeaker;
						if ( (gAgent.getID() != pSpeaker->mID) && (!RlvActions::canSendIM(pSpeaker->mID)) )
						{
							fRlvFilter = true;
							break;
						}
					}
				}
					break;
				default:
					fRlvFilter = true;
					break;
			}
		}
		
		if (fRlvFilter)
		{
			utf8_text = RlvStrings::getString(RLV_STRING_BLOCKED_SENDIM);
		}
	}
	// [/RLVa:KB]
	
	if (mSessionInitialized)
	{
		LLIMModel::sendMessage(utf8_text, mSessionID, mOtherParticipantUUID, mDialog);
	}
	else
	{
		//queue up the message to send once the session is initialized
		mQueuedMsgsForInit.append(utf8_text);
	}
	
	updateMessages();
}

FSFloaterIM::~FSFloaterIM()
{
	LLTransientFloaterMgr::getInstance()->removeControlView(LLTransientFloaterMgr::IM, (LLView*)this);
	mVoiceChannelStateChangeConnection.disconnect();
	if(LLVoiceClient::instanceExists())
	{
		LLVoiceClient::getInstance()->removeObserver((LLVoiceClientStatusObserver*)this);
	}
	
	LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
	if ((pIMSession) && (pIMSession->mSessionType == LLIMModel::LLIMSession::P2P_SESSION))
	{
		LLAvatarTracker::instance().removeParticularFriendObserver(mOtherParticipantUUID, this);
	}
	
	// Clean up any stray name cache connections
	if (mAvatarNameCacheConnection.connected())
	{
		mAvatarNameCacheConnection.disconnect();
	}
}

void FSFloaterIM::onVoiceChannelStateChanged(const LLVoiceChannel::EState& old_state, const LLVoiceChannel::EState& new_state)
{
	LL_DEBUGS("FSFloaterIM") << "FSFloaterIM::onVoiceChannelStateChanged" << LL_ENDL;
	updateButtons(new_state >= LLVoiceChannel::STATE_CALL_STARTED);
}

void FSFloaterIM::doToSelected(const LLSD& userdata)
{
	const std::string command = userdata.asString();
	if (command == "offer_tp")
		LLAvatarActions::offerTeleport(mOtherParticipantUUID);
	else if (command == "request_tp")
		LLAvatarActions::teleportRequest(mOtherParticipantUUID);
	else if (command == "share")
		LLAvatarActions::share(mOtherParticipantUUID);
	else if (command == "pay")
		LLAvatarActions::pay(mOtherParticipantUUID);
	else if (command == "show_profile")
		LLAvatarActions::showProfile(mOtherParticipantUUID);
	else if (command == "group_info")
		LLGroupActions::show(mSessionID);
	else if (command == "call")
		gIMMgr->startCall(mSessionID);
	else if (command == "end_call")
		gIMMgr->endCall(mSessionID);
	else if (command == "volume")
		LLFloaterReg::showInstance("fs_voice_controls");
	else if (command == "add_friend")
		LLAvatarActions::requestFriendshipDialog(mOtherParticipantUUID);
	else if (command == "history")
	{
		if (gSavedSettings.getBOOL("FSUseBuiltInHistory"))
		{
			LLFloaterReg::showInstance("preview_conversation", mSessionID, true);
		}
		else
		{
			gViewerWindow->getWindow()->openFile(LLLogChat::makeLogFileName(LLIMModel::instance().getHistoryFileName(mSessionID)));
		}
	}
	else
		LL_WARNS("FSFloaterIM") << "Unhandled command '" << command << "'. Ignoring." << LL_ENDL;
}

bool FSFloaterIM::checkEnabled(const LLSD& userdata)
{
	const std::string command = userdata.asString();
	if (command == "enable_offer_tp")
	{
		return LLAvatarActions::canOfferTeleport(mOtherParticipantUUID);
	}
	return false;
}

// support sysinfo button -Zi
void FSFloaterIM::onSysinfoButtonClicked()
{
	LLSD system_info = FSData::getSystemInfo();
	LLSD args;
	args["SYSINFO"] = system_info["Part1"].asString() + system_info["Part2"].asString();
	args["Part1"] = system_info["Part1"];
	args["Part2"] = system_info["Part2"];
	LLNotificationsUtil::add("SendSysinfoToIM",args,LLSD(),boost::bind(&FSFloaterIM::onSendSysinfo,this,_1,_2));
}

BOOL FSFloaterIM::onSendSysinfo(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification,response);

	if (option == 0)
	{
		std::string part1 = notification["substitutions"]["Part1"];
		std::string part2 = notification["substitutions"]["Part2"];
		if (mSessionInitialized)
		{
			LLIMModel::sendMessage(part1, mSessionID, mOtherParticipantUUID, mDialog);
			LLIMModel::sendMessage(part2, mSessionID, mOtherParticipantUUID, mDialog);
		}
		else
		{
			//queue up the message to send once the session is initialized
			mQueuedMsgsForInit.append(part1);
			mQueuedMsgsForInit.append(part2);
		}
		return TRUE;
	}
	return FALSE;
}

void FSFloaterIM::onSysinfoButtonVisibilityChanged(const LLSD& yes)
{
	getChild<LLUICtrl>("send_sysinfo_btn_panel")->setVisible(yes.asBoolean() /* && mIsSupportIM */);
}
// support sysinfo button -Zi

void FSFloaterIM::onChange(EStatusType status, const std::string &channelURI, bool proximal)
{
	if(status == STATUS_JOINING || status == STATUS_LEFT_CHANNEL)
	{
		return;
	}
	
	updateCallButton();
}

void FSFloaterIM::updateCallButton()
{
	// hide/show call button
	bool voice_enabled = LLVoiceClient::getInstance()->voiceEnabled() && LLVoiceClient::getInstance()->isVoiceWorking();
	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(mSessionID);
	
	if (!session) 
	{
		getChild<LLButton>("call_btn")->setEnabled(false);
		return;
	}
	
	bool session_initialized = session->mSessionInitialized;
	bool callback_enabled = session->mCallBackEnabled;

	BOOL enable_connect = session_initialized
	&& voice_enabled
	&& callback_enabled;

	getChild<LLButton>("call_btn")->setEnabled(enable_connect);
}

void FSFloaterIM::updateButtons(bool is_call_started)
{
	LL_DEBUGS("FSFloaterIM") << "FSFloaterIM::updateButtons" << LL_ENDL;
	getChildView("end_call_btn_panel")->setVisible( is_call_started);
	getChildView("voice_ctrls_btn_panel")->setVisible( is_call_started);
	getChildView("call_btn_panel")->setVisible( ! is_call_started);
	updateCallButton();
}

void FSFloaterIM::changed(U32 mask)
{
	LL_DEBUGS("FSFloaterIM") << "FSFloaterIM::changed(U32 mask)" << LL_ENDL;
	
	if(LLAvatarActions::isFriend(mOtherParticipantUUID))
	{
		bool is_online = LLAvatarTracker::instance().isBuddyOnline(mOtherParticipantUUID);
		getChild<LLButton>("teleport_btn")->setEnabled(is_online);
		getChild<LLButton>("call_btn")->setEnabled(is_online);
	}
	else
	{
		// If friendship dissolved, enable buttons by default because we don't
		// know about their online status anymore
		getChild<LLButton>("teleport_btn")->setEnabled(TRUE);
		getChild<LLButton>("call_btn")->setEnabled(TRUE);
	}
}

// </AO> Callbacks for llimcontrol panel, merged into this floater

//virtual
BOOL FSFloaterIM::postBuild()
{
	const LLUUID& other_party_id = LLIMModel::getInstance()->getOtherParticipantID(mSessionID);
	if (other_party_id.notNull())
	{
		mOtherParticipantUUID = other_party_id;
	}

	mControlPanel->setSessionId(mSessionID);
	
	// AO: always hide the control panel to start.
	LL_DEBUGS("FSFloaterIM") << "mControlPanel->getParent()" << mControlPanel->getParent() << LL_ENDL;
	mControlPanel->getParent()->setVisible(false);

	LL_DEBUGS("FSFloaterIM") << "buttons setup in IM start" << LL_ENDL;

	LLButton* button = getChild<LLButton>("slide_left_btn");
	button->setVisible(mControlPanel->getParent()->getVisible());
	button->setClickedCallback(boost::bind(&FSFloaterIM::onSlide, this));

	button = getChild<LLButton>("slide_right_btn");
	button->setVisible(!mControlPanel->getParent()->getVisible());
	button->setClickedCallback(boost::bind(&FSFloaterIM::onSlide, this));

	// support sysinfo button -Zi
	mSysinfoButton = getChild<LLButton>("send_sysinfo_btn");
	onSysinfoButtonVisibilityChanged(FALSE);
	
	// type-specfic controls
	LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
	if (pIMSession)
	{
		switch (pIMSession->mSessionType)
		{
			case LLIMModel::LLIMSession::P2P_SESSION:	// One-on-one IM
			{
				LL_DEBUGS("FSFloaterIM") << "LLIMModel::LLIMSession::P2P_SESSION" << LL_ENDL;
				getChild<LLLayoutPanel>("slide_panel")->setVisible(false);
				getChild<LLLayoutPanel>("gprofile_panel")->setVisible(false);
				getChild<LLLayoutPanel>("end_call_btn_panel")->setVisible(false);
				getChild<LLLayoutPanel>("voice_ctrls_btn_panel")->setVisible(false);
				
				LL_DEBUGS("FSFloaterIM") << "adding FSFloaterIM removing/adding particularfriendobserver" << LL_ENDL;
				LLAvatarTracker::instance().removeParticularFriendObserver(mOtherParticipantUUID, this);
				LLAvatarTracker::instance().addParticularFriendObserver(mOtherParticipantUUID, this);
				
				// Disable "Add friend" button for friends.
				LL_DEBUGS("FSFloaterIM") << "add_friend_btn check start" << LL_ENDL;
				getChild<LLButton>("add_friend_btn")->setEnabled(!LLAvatarActions::isFriend(mOtherParticipantUUID));
				
				// Disable "Teleport" button if friend is offline
				if(LLAvatarActions::isFriend(mOtherParticipantUUID))
				{
					LL_DEBUGS("FSFloaterIM") << "LLAvatarActions::isFriend - tp button" << LL_ENDL;
					getChild<LLButton>("teleport_btn")->setEnabled(LLAvatarTracker::instance().isBuddyOnline(mOtherParticipantUUID));
				}

				// support sysinfo button -Zi
				mSysinfoButton->setClickedCallback(boost::bind(&FSFloaterIM::onSysinfoButtonClicked, this));
				// this needs to be extended to fsdata awareness, once we have it. -Zi
				// mIsSupportIM=fsdata(partnerUUID).isSupport(); // pseudocode something like this
				onSysinfoButtonVisibilityChanged(gSavedSettings.getBOOL("SysinfoButtonInIM"));
				gSavedSettings.getControl("SysinfoButtonInIM")->getCommitSignal()->connect(boost::bind(&FSFloaterIM::onSysinfoButtonVisibilityChanged, this, _2));
				// support sysinfo button -Zi

				break;
			}
			case LLIMModel::LLIMSession::GROUP_SESSION:	// Group chat
			{
				LL_DEBUGS("FSFloaterIM") << "LLIMModel::LLIMSession::GROUP_SESSION start" << LL_ENDL;
				getChild<LLLayoutPanel>("profile_panel")->setVisible(false);
				getChild<LLLayoutPanel>("friend_panel")->setVisible(false);
				getChild<LLLayoutPanel>("tp_panel")->setVisible(false);
				getChild<LLLayoutPanel>("share_panel")->setVisible(false);
				getChild<LLLayoutPanel>("pay_panel")->setVisible(false);
				getChild<LLLayoutPanel>("end_call_btn_panel")->setVisible(false);
				getChild<LLLayoutPanel>("voice_ctrls_btn_panel")->setVisible(false);
				
				LL_DEBUGS("FSFloaterIM") << "LLIMModel::LLIMSession::GROUP_SESSION end" << LL_ENDL;
				break;
			}
			case LLIMModel::LLIMSession::ADHOC_SESSION:	// Conference chat
			{
				LL_DEBUGS("FSFloaterIM") << "LLIMModel::LLIMSession::ADHOC_SESSION  start" << LL_ENDL;
				getChild<LLLayoutPanel>("profile_panel")->setVisible(false);
				getChild<LLLayoutPanel>("gprofile_panel")->setVisible(false);
				getChild<LLLayoutPanel>("friend_panel")->setVisible(false);
				getChild<LLLayoutPanel>("tp_panel")->setVisible(false);
				getChild<LLLayoutPanel>("share_panel")->setVisible(false);
				getChild<LLLayoutPanel>("pay_panel")->setVisible(false);
				getChild<LLLayoutPanel>("end_call_btn_panel")->setVisible(false);
				getChild<LLLayoutPanel>("voice_ctrls_btn_panel")->setVisible(false);
				LL_DEBUGS("FSFloaterIM") << "LLIMModel::LLIMSession::ADHOC_SESSION end" << LL_ENDL;
				break;
			}
			default:
				LL_DEBUGS("FSFloaterIM") << "default buttons start" << LL_ENDL;
				getChild<LLLayoutPanel>("end_call_btn_panel")->setVisible(false);
				getChild<LLLayoutPanel>("voice_ctrls_btn_panel")->setVisible(false);		
				LL_DEBUGS("FSFloaterIM") << "default buttons end" << LL_ENDL;
				break;
		}
	}
	mVoiceChannel = LLIMModel::getInstance()->getVoiceChannel(mSessionID);
	if(mVoiceChannel)
	{
		LL_DEBUGS("FSFloaterIM") << "voice_channel start" << LL_ENDL;
		mVoiceChannelStateChangeConnection = mVoiceChannel->setStateChangedCallback(boost::bind(&FSFloaterIM::onVoiceChannelStateChanged, this, _1, _2));
		
		//call (either p2p, group or ad-hoc) can be already in started state
		updateButtons(mVoiceChannel->getState() >= LLVoiceChannel::STATE_CALL_STARTED);
		LL_DEBUGS("FSFloaterIM") << "voice_channel end" << LL_ENDL;
	}
	LLVoiceClient::getInstance()->addObserver((LLVoiceClientStatusObserver*)this);
	
	// </AO>
	
	mInputEditor = getChild<LLChatEntry>("chat_editor");
	mChatHistory = getChild<FSChatHistory>("chat_history");
	mChatLayoutPanel = getChild<LLLayoutPanel>("chat_layout_panel");
	mInputPanels = getChild<LLLayoutStack>("input_panels");
	mChatLayoutPanelHeight = mChatLayoutPanel->getRect().getHeight();
	mInputEditorPad = mChatLayoutPanelHeight - mInputEditor->getRect().getHeight();
	
	mInputEditor->setAutoreplaceCallback(boost::bind(&LLAutoReplace::autoreplaceCallback, LLAutoReplace::getInstance(), _1, _2, _3, _4, _5));
	mInputEditor->setFocusReceivedCallback(boost::bind(&FSFloaterIM::onInputEditorFocusReceived, this));
	mInputEditor->setFocusLostCallback(boost::bind(&FSFloaterIM::onInputEditorFocusLost, this));
	mInputEditor->setKeystrokeCallback(boost::bind(&FSFloaterIM::onInputEditorKeystroke, this));
	mInputEditor->setTextExpandedCallback(boost::bind(&FSFloaterIM::reshapeChatLayoutPanel, this));
	mInputEditor->setCommitOnFocusLost(FALSE);
	mInputEditor->setPassDelete(TRUE);
	mInputEditor->setFont(LLViewerChat::getChatFont());
	mInputEditor->enableSingleLineMode(gSavedSettings.getBOOL("FSUseSingleLineChatEntry"));

	childSetCommitCallback("chat_editor", onSendMsg, this);

	BOOL isFSSupportGroup = FSData::getInstance()->isSupportGroup(mSessionID);
	getChild<LLUICtrl>("support_panel")->setVisible(isFSSupportGroup);

	// <FS:Zi> Viewer version popup
	if (isFSSupportGroup)
	{
		// check if the dialog was set to ignore
		LLNotificationTemplatePtr templatep = LLNotifications::instance().getTemplate("FirstJoinSupportGroup");
		if (!templatep.get()->mForm->getIgnored())
		{
			// if not, give the user a choice, whether to enable the version prefix or not
			LLSD args;
			LLNotificationsUtil::add("FirstJoinSupportGroup", args, LLSD(),boost::bind(&FSFloaterIM::enableViewerVersionCallback, this, _1, _2));
		}
	}
	// </FS:Zi> Viewer version popup

	// only dock when chiclets are visible, or the floater will get stuck in the top left
	// FIRE-9984 -Zi
	setDocked(!gSavedSettings.getBOOL("FSDisableIMChiclets"));

	mTypingStart = LLTrans::getString("IM_typing_start_string");

	// Disable input editor if session cannot accept text
	LLIMModel::LLIMSession* im_session =
		LLIMModel::instance().findIMSession(mSessionID);
	if( im_session && !im_session->mTextIMPossible )
	{
		mInputEditor->setEnabled(FALSE);
		mInputEditor->setLabel(LLTrans::getString("IM_unavailable_text_label"));
	}

	if ( im_session && im_session->isP2PSessionType())
	{
		fetchAvatarName(im_session->mOtherParticipantID);
	}
	else
	{
		std::string session_name(LLIMModel::instance().getName(mSessionID));
		updateSessionName(session_name, session_name);
	}
	
	//*TODO if session is not initialized yet, add some sort of a warning message like "starting session...blablabla"
	//see LLFloaterIMPanel for how it is done (IB)

	// don't call dockable floater functions when chiclets are disabled, it will dock the floater
	// FIRE-9984 -Zi
	if(isChatMultiTab() || gSavedSettings.getBOOL("FSDisableIMChiclets"))
	{
		return LLFloater::postBuild();
	}
	else
	{
		return LLDockableFloater::postBuild();
	}
}

void FSFloaterIM::updateSessionName(const std::string& ui_title,
									const std::string& ui_label)
{
	// <FS:Ansariel> FIRE-7874: Name is missing on tab if announcing incoming IMs is enabled and sender's name is not in name cache
	mSavedTitle = ui_title;

	mInputEditor->setLabel(LLTrans::getString("IM_to_label") + " " + ui_label);
	setTitle(ui_title);	
}

void FSFloaterIM::fetchAvatarName(LLUUID& agent_id)
{
	if (agent_id.notNull())
	{
		if (mAvatarNameCacheConnection.connected())
		{
			mAvatarNameCacheConnection.disconnect();
		}
		mAvatarNameCacheConnection = LLAvatarNameCache::get(agent_id,
															boost::bind(&FSFloaterIM::onAvatarNameCache, this, _1, _2));
	}
}

void FSFloaterIM::onAvatarNameCache(const LLUUID& agent_id,
									const LLAvatarName& av_name)
{
	mAvatarNameCacheConnection.disconnect();
	// <FS:Ansariel> FIRE-8658: Let the user decide how the name should be displayed
	// Use display name only for labels, as the extended name will be in the
	// floater title
	//std::string ui_title = av_name.getCompleteName();
	//updateSessionName(ui_title, av_name.getDisplayName());
	//mTypingStart.setArg("[NAME]", ui_title);

	std::string name = av_name.getCompleteName();
	if (LLAvatarName::useDisplayNames())
	{
		switch (gSavedSettings.getS32("FSIMTabNameFormat"))
		{
			// Display name
			case 0:
				name = av_name.getDisplayName();
				break;
			// Username
			case 1:
				name = av_name.getUserNameForDisplay();
				break;
			// Display name (username)
			case 2:
				// Do nothing - we already set the complete name as default
				break;
			// Username (display name)
			case 3:
				if (av_name.isDisplayNameDefault())
				{
					name = av_name.getUserNameForDisplay();
				}
				else
				{
					name = av_name.getUserNameForDisplay() + " (" + av_name.getDisplayName() + ")";
				}
				break;
			default:
				// Do nothing - we already set the complete name as default
				break;
		}
	}

	updateSessionName(name, name);
	mTypingStart.setArg("[NAME]", name);
	LL_DEBUGS("FSFloaterIM") << "Setting IM tab name to '" << name << "'" << LL_ENDL;
	// </FS:Ansariel>
}

// virtual
void FSFloaterIM::draw()
{
	if (mMeTyping)
	{
		// Send an additional Start Typing packet every ME_TYPING_TIMEOUT seconds
		if (mMeTypingTimer.getElapsedTimeF32() > ME_TYPING_TIMEOUT && false == mShouldSendTypingState && mDialog == IM_NOTHING_SPECIAL)
		{
			LL_DEBUGS("TypingMsgs") << "Send additional Start Typing packet" << LL_ENDL;
			LLIMModel::instance().sendTypingState(mSessionID, mOtherParticipantUUID, TRUE);
			mMeTypingTimer.reset();
		}

		// Time out if user hasn't typed for a while.
		if (mTypingTimeoutTimer.getElapsedTimeF32() > LLAgent::TYPING_TIMEOUT_SECS)
		{
			setTyping(false);
			LL_DEBUGS("TypingMsgs") << "Send stop typing due to timeout" << LL_ENDL;
		}
	}

	// Clear <name is typing> message if no data received for OTHER_TYPING_TIMEOUT seconds
	if (mOtherTyping && mOtherTypingTimer.getElapsedTimeF32() > OTHER_TYPING_TIMEOUT)
	{
		LL_DEBUGS("TypingMsgs") << "Received: is typing cleared due to timeout" << LL_ENDL;
		removeTypingIndicator();
		mOtherTyping = false;
	}

	LLTransientDockableFloater::draw();
}


// static
void* FSFloaterIM::createPanelIMControl(void* userdata)
{
	FSFloaterIM *self = (FSFloaterIM*)userdata;
	self->mControlPanel = new FSPanelIMControlPanel();
	self->mControlPanel->setXMLFilename("panel_fs_im_control_panel.xml");
	return self->mControlPanel;
}


// static
void* FSFloaterIM::createPanelGroupControl(void* userdata)
{
	FSFloaterIM *self = (FSFloaterIM*)userdata;
	self->mControlPanel = new FSPanelGroupControlPanel(self->mSessionID);
	self->mControlPanel->setXMLFilename("panel_fs_group_control_panel.xml");
	return self->mControlPanel;
}

// static
void* FSFloaterIM::createPanelAdHocControl(void* userdata)
{
	FSFloaterIM *self = (FSFloaterIM*)userdata;
	self->mControlPanel = new FSPanelAdHocControlPanel(self->mSessionID);
	self->mControlPanel->setXMLFilename("panel_fs_adhoc_control_panel.xml");
	return self->mControlPanel;
}

void FSFloaterIM::onSlide()
{
	mControlPanel->getParent()->setVisible(!mControlPanel->getParent()->getVisible());

	gSavedSettings.setBOOL("IMShowControlPanel", mControlPanel->getParent()->getVisible());

	getChild<LLButton>("slide_left_btn")->setVisible(mControlPanel->getParent()->getVisible());
	getChild<LLButton>("slide_right_btn")->setVisible(!mControlPanel->getParent()->getVisible());
}

//static
FSFloaterIM* FSFloaterIM::show(const LLUUID& session_id)
{
	closeHiddenIMToasts();

	if (!gIMMgr->hasSession(session_id)) return NULL;

	if(!isChatMultiTab())
	{
		//hide all
		LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("fs_impanel");
		for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin();
			 iter != inst_list.end(); ++iter)
		{
			FSFloaterIM* floater = dynamic_cast<FSFloaterIM*>(*iter);
			if (floater && floater->isDocked())
			{
				floater->setVisible(false);
			}
		}
	}

	bool exist = findInstance(session_id);

	FSFloaterIM* floater = getInstance(session_id);
	if (!floater) return NULL;

	if(isChatMultiTab())
	{
		FSFloaterIMContainer* floater_container = FSFloaterIMContainer::getInstance();

		// do not add existed floaters to avoid adding torn off instances
		if (!exist)
		{
			//		LLTabContainer::eInsertionPoint i_pt = user_initiated ? LLTabContainer::RIGHT_OF_CURRENT : LLTabContainer::END;
			// TODO: mantipov: use LLTabContainer::RIGHT_OF_CURRENT if it exists
			LLTabContainer::eInsertionPoint i_pt = LLTabContainer::END;
			
			if (floater_container)
			{
				floater_container->addFloater(floater, TRUE, i_pt);
			}
		}

		floater->openFloater(floater->getKey());
		floater->setFocus(TRUE);
	}
	else
	{
		// Docking may move chat window, hide it before moving, or user will see how window "jumps"
		floater->setVisible(false);

		// only dock when chiclets are visible, or the floater will get stuck in the top left
		// FIRE-9984 -Zi
		if (floater->getDockControl() == NULL && !gSavedSettings.getBOOL("FSDisableIMChiclets"))
		{
			LLChiclet* chiclet =
					LLChicletBar::getInstance()->getChicletPanel()->findChiclet<LLChiclet>(
							session_id);
			if (chiclet == NULL)
			{
				LL_ERRS() << "Dock chiclet for FSFloaterIM doesn't exists" << LL_ENDL;
			}
			else
			{
				LLChicletBar::getInstance()->getChicletPanel()->scrollToChiclet(chiclet);
			}

			// <FS:Ansariel> Group notices, IMs and chiclets position
			//floater->setDockControl(new LLDockControl(chiclet, floater, floater->getDockTongue(),
			//		LLDockControl::BOTTOM));
			if (gSavedSettings.getBOOL("InternalShowGroupNoticesTopRight"))
			{
				floater->setDockControl(new LLDockControl(chiclet, floater, floater->getDockTongue(),
						LLDockControl::BOTTOM));
			}
			else
			{
				floater->setDockControl(new LLDockControl(chiclet, floater, floater->getDockTongue(),
						LLDockControl::TOP));
			}
			// </FS:Ansariel> Group notices, IMs and chiclets position
		}

		// window is positioned, now we can show it.
		floater->setVisible(TRUE);
	}

	return floater;
}

void FSFloaterIM::setDocked(bool docked, bool pop_on_undock)
{
	// update notification channel state
	LLNotificationsUI::LLScreenChannel* channel = static_cast<LLNotificationsUI::LLScreenChannel*>
		(LLNotificationsUI::LLChannelManager::getInstance()->
											findChannelByID(LLUUID(gSavedSettings.getString("NotificationChannelUUID"))));
	
	if(!isChatMultiTab())
	{
		LLTransientDockableFloater::setDocked(docked, pop_on_undock);
	}

	// update notification channel state
	if(channel)
	{
		channel->updateShowToastsState();
		channel->redrawToasts();
	}
}

void FSFloaterIM::setVisible(BOOL visible)
{
	LLNotificationsUI::LLScreenChannel* channel = static_cast<LLNotificationsUI::LLScreenChannel*>
		(LLNotificationsUI::LLChannelManager::getInstance()->
											findChannelByID(LLUUID(gSavedSettings.getString("NotificationChannelUUID"))));
	LLTransientDockableFloater::setVisible(visible);

	// update notification channel state
	if(channel)
	{
		channel->updateShowToastsState();
		channel->redrawToasts();
	}

	BOOL is_minimized = visible && isChatMultiTab()
		? FSFloaterIMContainer::getInstance()->isMinimized()
		: !visible;

	if (!is_minimized && mChatHistory && mInputEditor)
	{
		//only if floater was construced and initialized from xml
		updateMessages();
		FSFloaterIMContainer* im_container = FSFloaterIMContainer::getInstance();
		
		//prevent stealing focus when opening a background IM tab (EXT-5387, checking focus for EXT-6781)
		// If this is docked, is the selected tab, and the im container has focus, put focus in the input ctrl -KC
		bool is_active = im_container->getActiveFloater() == this && im_container->hasFocus();
		if (!isChatMultiTab() || is_active || hasFocus())
		{
			mInputEditor->setFocus(TRUE);
		}
	}

	if(!visible)
	{
		LLIMChiclet* chiclet = LLChicletBar::getInstance()->getChicletPanel()->findChiclet<LLIMChiclet>(mSessionID);
		if(chiclet)
		{
			chiclet->setToggleState(false);
		}
	}
	
	if (visible && isInVisibleChain())
	{
		sIMFloaterShowedSignal(mSessionID);
	}
}

BOOL FSFloaterIM::getVisible()
{
	if(isChatMultiTab())
	{
		FSFloaterIMContainer* im_container = FSFloaterIMContainer::getInstance();
		
		// Treat inactive floater as invisible.
		bool is_active = im_container->getActiveFloater() == this;
	
		//torn off floater is always inactive
		if (!is_active && getHost() != im_container)
		{
			return LLTransientDockableFloater::getVisible();
		}

		// getVisible() returns TRUE when Tabbed IM window is minimized.
		return is_active && !im_container->isMinimized() && im_container->getVisible();
	}
	else
	{
		return LLTransientDockableFloater::getVisible();
	}
}

//static
bool FSFloaterIM::toggle(const LLUUID& session_id)
{
	if(!isChatMultiTab())
	{
		FSFloaterIM* floater = LLFloaterReg::findTypedInstance<FSFloaterIM>("fs_impanel", session_id);
		if (floater && floater->getVisible() && floater->hasFocus())
		{
			// clicking on chiclet to close floater just hides it to maintain existing
			// scroll/text entry state
			floater->setVisible(false);
			return false;
		}
		else if(floater && (!floater->isDocked() || (floater->getVisible() && !floater->hasFocus())))
		{
			floater->setVisible(TRUE);
			floater->setFocus(TRUE);
			return true;
		}
	}

	// ensure the list of messages is updated when floater is made visible
	show(session_id);
	return true;
}

//static
FSFloaterIM* FSFloaterIM::findInstance(const LLUUID& session_id)
{
	return LLFloaterReg::findTypedInstance<FSFloaterIM>("fs_impanel", session_id);
}

FSFloaterIM* FSFloaterIM::getInstance(const LLUUID& session_id)
{
	return LLFloaterReg::getTypedInstance<FSFloaterIM>("fs_impanel", session_id);
}

void FSFloaterIM::sessionInitReplyReceived(const LLUUID& im_session_id)
{
	mSessionInitialized = true;

	//will be different only for an ad-hoc im session
	if (mSessionID != im_session_id)
	{
		mSessionID = im_session_id;
		setKey(im_session_id);
		mControlPanel->setSessionId(im_session_id);
	}

	// updating "Call" button from group/ad-hoc control panel here to enable it without placing into draw() (EXT-4796)
	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(im_session_id);
	if (session)
	{
		if ((session->isGroupSessionType() && gAgent.isInGroup(im_session_id)) || session->isAdHocSessionType())
		{
			updateCallButton();
		}
	}
	
	//*TODO here we should remove "starting session..." warning message if we added it in postBuild() (IB)


	//need to send delayed messaged collected while waiting for session initialization
	if (!mQueuedMsgsForInit.size()) return;
	LLSD::array_iterator iter;
	for ( iter = mQueuedMsgsForInit.beginArray();
		iter != mQueuedMsgsForInit.endArray();
		++iter)
	{
		LLIMModel::sendMessage(iter->asString(), mSessionID,
			mOtherParticipantUUID, mDialog);
	}
}

void FSFloaterIM::updateMessages()
{
	//<FS:HG> FS-1734 seperate name and text styles for moderator
	bool highlight_mods_chat = gSavedSettings.getBOOL("FSHighlightGroupMods");


	std::list<LLSD> messages;

	// we shouldn't reset unread message counters if IM floater doesn't have focus
    LLIMModel::instance().getMessages(mSessionID, messages, mLastMessageIndex + 1, hasFocus());

	if (messages.size())
	{
		LLSD chat_args;
		chat_args["use_plain_text_chat_history"] = gSavedSettings.getBOOL("PlainTextChatHistory");
		chat_args["show_names_for_p2p_conv"] = gSavedSettings.getBOOL("IMShowNamesForP2PConv");
		chat_args["show_time"] = gSavedSettings.getBOOL("FSShowTimestampsIM");
		
		LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
		RLV_ASSERT(pIMSession);

		chat_args["is_p2p"] = pIMSession->isP2PSessionType();

		std::ostringstream message;
		std::list<LLSD>::const_reverse_iterator iter = messages.rbegin();
		std::list<LLSD>::const_reverse_iterator iter_end = messages.rend();
		for (; iter != iter_end; ++iter)
		{
			LLSD msg = *iter;

			std::string time = msg["time"].asString();
			LLUUID from_id = msg["from_id"].asUUID();
			std::string from = msg["from"].asString();
			std::string message = msg["message"].asString();
			bool is_history = msg["is_history"].asBoolean();

			LLChat chat;
			chat.mFromID = from_id;
			chat.mSessionID = mSessionID;
			chat.mFromName = from;
			chat.mTimeStr = time;
			chat.mChatStyle = is_history ? CHAT_STYLE_HISTORY : chat.mChatStyle;
			
			// Bold group moderators' chat -KC
			//<FS:HG> FS-1734 seperate name and text styles for moderator
			//if (!is_history && bold_mods_chat && pIMSession && pIMSession->mSpeakers)
			if (!is_history && highlight_mods_chat && pIMSession && pIMSession->mSpeakers)
			{
				LLPointer<LLSpeaker> speakerp = pIMSession->mSpeakers->findSpeaker(from_id);
				if (speakerp && speakerp->mIsModerator)
				{
					chat.mChatStyle = CHAT_STYLE_MODERATOR;
				}
			}

			// process offer notification
			if (msg.has("notification_id"))
			{
				chat.mNotifId = msg["notification_id"].asUUID();
				// if notification exists - embed it
				if (LLNotificationsUtil::find(chat.mNotifId) != NULL)
				{
					// remove embedded notification from channel
					LLNotificationsUI::LLScreenChannel* channel = static_cast<LLNotificationsUI::LLScreenChannel*>
							(LLNotificationsUI::LLChannelManager::getInstance()->
																findChannelByID(LLUUID(gSavedSettings.getString("NotificationChannelUUID"))));
					if (getVisible())
					{
						// toast will be automatically closed since it is not storable toast
						channel->hideToast(chat.mNotifId);
					}
				}
				// if notification doesn't exist - try to use next message which should be log entry
				else
				{
					continue;
				}
			}
			//process text message
			else
			{
				chat.mText = message;
			}
			
			mChatHistory->appendMessage(chat, chat_args);
			mLastMessageIndex = msg["index"].asInteger();

			// if it is a notification - next message is a notification history log, so skip it
			if (chat.mNotifId.notNull() && LLNotificationsUtil::find(chat.mNotifId) != NULL)
			{
				if (++iter == iter_end)
				{
					break;
				}
				else
				{
					mLastMessageIndex++;
				}
			}
		}
	}
}

void FSFloaterIM::reloadMessages(bool clean_messages/* = false*/)
{
	if (clean_messages)
	{
		LLIMModel::LLIMSession * sessionp = LLIMModel::instance().findIMSession(mSessionID);

		if (NULL != sessionp)
		{
			sessionp->loadHistory();
		}
	}

	mChatHistory->clear();
	mLastMessageIndex = -1;
	updateMessages();
}

void FSFloaterIM::onInputEditorFocusReceived()
{
	// Allow enabling the FSFloaterIM input editor only if session can accept text
	LLIMModel::LLIMSession* im_session =
		LLIMModel::instance().findIMSession(mSessionID);
	//TODO: While disabled lllineeditor can receive focus we need to check if it is enabled (EK)
	if (im_session && im_session->mTextIMPossible && mInputEditor->getEnabled())
	{
		//in disconnected state IM input editor should be disabled
		mInputEditor->setEnabled(!gDisconnected);
	}
}

void FSFloaterIM::onInputEditorFocusLost()
{
	setTyping(false);
}

void FSFloaterIM::onInputEditorKeystroke()
{
	std::string text = mInputEditor->getText();
	if (!text.empty())
	{
		setTyping(true);
	}
	else
	{
		// Deleting all text counts as stopping typing.
		setTyping(false);
	}
}

void FSFloaterIM::setTyping(bool typing)
{
	if ( typing )
	{
		// Started or proceeded typing, reset the typing timeout timer
		mTypingTimeoutTimer.reset();
	}

	if ( mMeTyping != typing )
	{
		// Typing state is changed
		mMeTyping = typing;
		// So, should send current state
		mShouldSendTypingState = true;
		// In case typing is started, send state after some delay
		mTypingTimer.reset();
	}

	// Don't want to send typing indicators to multiple people, potentially too
	// much network traffic. Only send in person-to-person IMs.
	// <FS:Techwolf Lupindo> fsdata support
	//if ( mShouldSendTypingState && mDialog == IM_NOTHING_SPECIAL )
	if ( mShouldSendTypingState && mDialog == IM_NOTHING_SPECIAL && !(FSData::instance().isSupport(mOtherParticipantUUID) && FSData::instance().isAgentFlag(gAgentID, FSData::NO_SUPPORT)))
	// </FS:Techwolf Lupindo>
	{
		if ( mMeTyping )
		{
			if ( mTypingTimer.getElapsedTimeF32() > 1.f )
			{
				// Still typing, send 'start typing' notification
				LLIMModel::instance().sendTypingState(mSessionID, mOtherParticipantUUID, TRUE);
				mShouldSendTypingState = false;
				mMeTypingTimer.reset();
			}
		}
		else
		{
			// Send 'stop typing' notification immediately
			LLIMModel::instance().sendTypingState(mSessionID, mOtherParticipantUUID, FALSE);
			mShouldSendTypingState = false;
		}
	}

	LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
	if (speaker_mgr)
		speaker_mgr->setSpeakerTyping(gAgent.getID(), FALSE);

}

void FSFloaterIM::processIMTyping(const LLIMInfo* im_info, BOOL typing)
{
	if ( typing )
	{
		// other user started typing
		addTypingIndicator(im_info);
		mOtherTypingTimer.reset();
	}
	else
	{
		// other user stopped typing
		removeTypingIndicator(im_info);
	}
}

void FSFloaterIM::processAgentListUpdates(const LLSD& body)
{
	if ( !body.isMap() ) return;

	if ( body.has("agent_updates") && body["agent_updates"].isMap() )
	{
		LLSD agent_data = body["agent_updates"].get(gAgentID.asString());
		if (agent_data.isMap() && agent_data.has("info"))
		{
			LLSD agent_info = agent_data["info"];

			if (agent_info.has("mutes"))
			{
				BOOL moderator_muted_text = agent_info["mutes"]["text"].asBoolean(); 
				mInputEditor->setEnabled(!moderator_muted_text);
				std::string label;
				if (moderator_muted_text)
					label = LLTrans::getString("IM_muted_text_label");
				else
					label = LLTrans::getString("IM_to_label") + " " + LLIMModel::instance().getName(mSessionID);
				mInputEditor->setLabel(label);

				if (moderator_muted_text)
					LLNotificationsUtil::add("TextChatIsMutedByModerator");
			}
		}
	}
}

void FSFloaterIM::sendParticipantsAddedNotification(const uuid_vec_t& uuids)
{
	std::string names_string;
	LLAvatarActions::buildResidentsString(uuids, names_string);
	LLStringUtil::format_map_t args;
	args["[NAME]"] = names_string;
	
	sendMsg(getString(uuids.size() > 1 ? "multiple_participants_added" : "participant_added", args));
}

void FSFloaterIM::updateChatHistoryStyle()
{
	mChatHistory->clear();
	mLastMessageIndex = -1;
	updateMessages();
}

void FSFloaterIM::processChatHistoryStyleUpdate(const LLSD& newvalue)
{
	LLFontGL* font = LLViewerChat::getChatFont();
	LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("fs_impanel");
	for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin();
		 iter != inst_list.end(); ++iter)
	{
		FSFloaterIM* floater = dynamic_cast<FSFloaterIM*>(*iter);
		if (floater)
		{
			floater->updateChatHistoryStyle();
			floater->mInputEditor->setFont(font);
		}
	}
}

void FSFloaterIM::processSessionUpdate(const LLSD& session_update)
{
	// *TODO : verify following code when moderated mode will be implemented
	if ( false && session_update.has("moderated_mode") &&
		 session_update["moderated_mode"].has("voice") )
	{
		BOOL voice_moderated = session_update["moderated_mode"]["voice"];
		const std::string session_label = LLIMModel::instance().getName(mSessionID);

		if (voice_moderated)
		{
			setTitle(session_label + std::string(" ") + LLTrans::getString("IM_moderated_chat_label"));
		}
		else
		{
			setTitle(session_label);
		}

		// *TODO : uncomment this when/if LLPanelActiveSpeakers panel will be added
		//update the speakers dropdown too
		//mSpeakerPanel->setVoiceModerationCtrlMode(voice_moderated);
	}
}

BOOL FSFloaterIM::handleDragAndDrop(S32 x, S32 y, MASK mask,
						   BOOL drop, EDragAndDropType cargo_type,
						   void *cargo_data, EAcceptance *accept,
						   std::string& tooltip_msg)
{

	if (mDialog == IM_NOTHING_SPECIAL)
	{
		LLToolDragAndDrop::handleGiveDragAndDrop(mOtherParticipantUUID, mSessionID, drop,
												 cargo_type, cargo_data, accept);
	}

	// handle case for dropping calling cards (and folders of calling cards) onto invitation panel for invites
	else if (isInviteAllowed())
	{
		*accept = ACCEPT_NO;

		if (cargo_type == DAD_CALLINGCARD)
		{
			if (dropCallingCard((LLInventoryItem*)cargo_data, drop))
			{
				*accept = ACCEPT_YES_MULTI;
			}
		}
		else if (cargo_type == DAD_CATEGORY)
		{
			if (dropCategory((LLInventoryCategory*)cargo_data, drop))
			{
				*accept = ACCEPT_YES_MULTI;
			}
		}
	}
	return TRUE;
}

BOOL FSFloaterIM::dropCallingCard(LLInventoryItem* item, BOOL drop)
{
	BOOL rv = isInviteAllowed();
	if(rv && item && item->getCreatorUUID().notNull())
	{
		if(drop)
		{
			uuid_vec_t ids;
			ids.push_back(item->getCreatorUUID());
			inviteToSession(ids);
		}
	}
	else
	{
		// set to false if creator uuid is null.
		rv = FALSE;
	}
	return rv;
}

BOOL FSFloaterIM::dropCategory(LLInventoryCategory* category, BOOL drop)
{
	BOOL rv = isInviteAllowed();
	if(rv && category)
	{
		LLInventoryModel::cat_array_t cats;
		LLInventoryModel::item_array_t items;
		LLUniqueBuddyCollector buddies;
		gInventory.collectDescendentsIf(category->getUUID(),
										cats,
										items,
										LLInventoryModel::EXCLUDE_TRASH,
										buddies);
		S32 count = items.size();
		if(count == 0)
		{
			rv = FALSE;
		}
		else if(drop)
		{
			uuid_vec_t ids;
			ids.reserve(count);
			for(S32 i = 0; i < count; ++i)
			{
				ids.push_back(items.at(i)->getCreatorUUID());
			}
			inviteToSession(ids);
		}
	}
	return rv;
}

BOOL FSFloaterIM::isInviteAllowed() const
{

	return ((IM_SESSION_CONFERENCE_START == mDialog) ||
			(IM_SESSION_INVITE == mDialog && !gAgent.isInGroup(mSessionID)));
}

class LLSessionInviteResponder : public LLHTTPClient::Responder
{
public:
	LLSessionInviteResponder(const LLUUID& session_id)
	{
		mSessionID = session_id;
	}

	void errorWithContent(U32 statusNum, const std::string& reason, const LLSD& content)
	{
		LL_WARNS("FSFloaterIM") << "Error inviting all agents to session [status:"
								<< statusNum << "]: " << content << LL_ENDL;
		//TODO: throw something back to the viewer here?
	}

private:
	LLUUID mSessionID;
};

BOOL FSFloaterIM::inviteToSession(const uuid_vec_t& ids)
{
	LLViewerRegion* region = gAgent.getRegion();
	bool is_region_exist = region != NULL;

	if (is_region_exist)
	{
		S32 count = ids.size();

		if( isInviteAllowed() && (count > 0) )
		{
			LL_DEBUGS("FSFloaterIM") << "FSFloaterIM::inviteToSession() - inviting participants" << LL_ENDL;

			std::string url = region->getCapability("ChatSessionRequest");

			LLSD data;
			data["params"] = LLSD::emptyArray();
			for (int i = 0; i < count; i++)
			{
				data["params"].append(ids[i]);
			}
			data["method"] = "invite";
			data["session-id"] = mSessionID;
			LLHTTPClient::post(url,	data,new LLSessionInviteResponder(mSessionID));
		}
		else
		{
			LL_DEBUGS("FSFloaterIM") << "LLFloaterIMSession::inviteToSession -"
									 << " no need to invite agents for "
									 << mDialog << LL_ENDL;
			// successful add, because everyone that needed to get added
			// was added.
		}
	}

	return is_region_exist;
}

void FSFloaterIM::addTypingIndicator(const LLIMInfo* im_info)
{
	// We may have lost a "stop-typing" packet, don't add it twice
	if ( im_info && !mOtherTyping )
	{
		mOtherTyping = true;

		// Save and set new title
		mSavedTitle = getTitle();
		setTitle((gSavedSettings.getBOOL("FSTypingChevronPrefix") ? "> " : "") + mTypingStart.getString());

		// Update speaker
		LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
		if ( speaker_mgr )
		{
			speaker_mgr->setSpeakerTyping(im_info->mFromID, TRUE);
		}
	}
}

void FSFloaterIM::removeTypingIndicator(const LLIMInfo* im_info)
{
	if ( mOtherTyping )
	{
		mOtherTyping = false;

		// Revert the title to saved one
		setTitle(mSavedTitle);

		if ( im_info )
		{
			// Update speaker
			LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
			if ( speaker_mgr )
			{
				speaker_mgr->setSpeakerTyping(im_info->mFromID, FALSE);
			}
		}
		// Ansariel: Transplant of STORM-1975; Typing notifications are only sent in P2P sessions,
		//           so we can use mOtherParticipantUUID here and don't need LLIMInfo (Kitty said
		//           it's dangerous to use the stored LLIMInfo in LL's version!)
		else if (mDialog == IM_NOTHING_SPECIAL && mOtherParticipantUUID.notNull())
		{
			// Update speaker
			LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
			if ( speaker_mgr )
			{
				speaker_mgr->setSpeakerTyping(mOtherParticipantUUID, FALSE);
			}
		}
	}
}

// static
void FSFloaterIM::closeHiddenIMToasts()
{
	class IMToastMatcher: public LLNotificationsUI::LLScreenChannel::Matcher
	{
	public:
		bool matches(const LLNotificationPtr notification) const
		{
			// "notifytoast" type of notifications is reserved for IM notifications
			return "notifytoast" == notification->getType();
		}
	};

	LLNotificationsUI::LLScreenChannel* channel = LLNotificationsUI::LLChannelManager::getNotificationScreenChannel();
	if (channel != NULL)
	{
		channel->closeHiddenToasts(IMToastMatcher());
	}
}
// static
void FSFloaterIM::confirmLeaveCallCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	const LLSD& payload = notification["payload"];
	LLUUID session_id = payload["session_id"];

	LLFloater* im_floater = LLFloaterReg::findInstance("fs_impanel", session_id);
	if (option == 0 && im_floater != NULL)
	{
		im_floater->closeFloater();
	}

	return;
}

// static
bool FSFloaterIM::isChatMultiTab()
{
	// Restart is required in order to change chat window type.
	static bool is_single_window = gSavedSettings.getS32("FSChatWindow") == 1;
	return is_single_window;
}

// static
void FSFloaterIM::initIMFloater()
{
	// This is called on viewer start up
	// init chat window type before user changed it in preferences
	isChatMultiTab();
}

//static
void FSFloaterIM::sRemoveTypingIndicator(const LLSD& data)
{
	LLUUID session_id = data["session_id"];
	if (session_id.isNull()) return;

	LLUUID from_id = data["from_id"];
	if (gAgentID == from_id || LLUUID::null == from_id) return;

	FSFloaterIM* floater = FSFloaterIM::findInstance(session_id);
	if (!floater) return;

	if (IM_NOTHING_SPECIAL != floater->mDialog) return;

	floater->removeTypingIndicator();
}

void FSFloaterIM::onNewIMReceived( const LLUUID& session_id )
{

	if (isChatMultiTab())
	{
		FSFloaterIMContainer* im_box = FSFloaterIMContainer::getInstance();
		if (!im_box) return;

		if (FSFloaterIM::findInstance(session_id)) return;

		FSFloaterIM* new_tab = FSFloaterIM::getInstance(session_id);

		im_box->addFloater(new_tab, FALSE, LLTabContainer::END);
	}

}

void	FSFloaterIM::onClickCloseBtn(bool app_quitting)
{
	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(
				mSessionID);

	if (session == NULL)
	{
		LL_WARNS("FSFloaterIM") << "Empty session." << LL_ENDL;
		return;
	}

	bool is_call_with_chat = session->isGroupSessionType()
			|| session->isAdHocSessionType() || session->isP2PSessionType();

	LLVoiceChannel* voice_channel = LLIMModel::getInstance()->getVoiceChannel(mSessionID);

	if (is_call_with_chat && voice_channel != NULL && voice_channel->isActive())
	{
		LLSD payload;
		payload["session_id"] = mSessionID;
		LLNotificationsUtil::add("ConfirmLeaveCall", LLSD(), payload, confirmLeaveCallCallback);
		return;
	}

	LLFloater::onClickCloseBtn();
}

// <FS:Zi> Viewer version popup
BOOL FSFloaterIM::enableViewerVersionCallback(const LLSD& notification,const LLSD& response)
{
	S32 option=LLNotificationsUtil::getSelectedOption(notification,response);

	BOOL result=FALSE;
	if(option==0)		// "yes"
	{
		result=TRUE;
	}

	gSavedSettings.setBOOL("FSSupportGroupChatPrefix2",result);
	return result;
}
// </FS:Zi>

// <FS:Ansariel> FIRE-3248: Disable add friend button on IM floater if friendship request accepted
void FSFloaterIM::setEnableAddFriendButton(BOOL enabled)
{
	getChild<LLButton>("add_friend_btn")->setEnabled(enabled);
}
// </FS:Ansariel>

// <FS:CR> FIRE-11734
//static
void FSFloaterIM::clearAllOpenHistories()
{
	LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("fs_impanel");
	for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin();
		 iter != inst_list.end(); ++iter)
	{
		FSFloaterIM* floater = dynamic_cast<FSFloaterIM*>(*iter);
		if (floater)
		{
			floater->reloadMessages(true);
		}
	}
	
	FSFloaterNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat", LLSD());
	if (nearby_chat)
	{
		nearby_chat->reloadMessages(true);
	}
}

void FSFloaterIM::initIMSession(const LLUUID& session_id)
{
	// Change the floater key to bind it to a new session.
	setKey(session_id);
	
	mSessionID = session_id;
	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(mSessionID);
	
	if (session)
	{
		mSessionInitialized = session->mSessionInitialized;
		mDialog = session->mType;
	}
}

void FSFloaterIM::reshapeChatLayoutPanel()
{
	mChatLayoutPanel->reshape(mChatLayoutPanel->getRect().getWidth(), mInputEditor->getRect().getHeight() + mInputEditorPad, FALSE);
}

boost::signals2::connection FSFloaterIM::setIMFloaterShowedCallback(const floater_showed_signal_t::slot_type& cb)
{
	return FSFloaterIM::sIMFloaterShowedSignal.connect(cb);
}
