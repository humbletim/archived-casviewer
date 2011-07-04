/** 
 * @file fscontactsfloater.h
 * @brief 
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, The Phoenix Viewer Project, Inc.
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
 * The Phoenix Viewer Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * $/LicenseInfo$
 */
 
 
#ifndef LL_FSCONTACTSFLOATER_H
#define LL_FSCONTACTSFLOATER_H

#include <map>
#include <vector>

#include "llfloater.h"
#include "lleventtimer.h"
#include "llcallingcard.h"

class LLAvatarList;
class LLAvatarName;
class LLAvatarTracker;
class LLFriendObserver;
class LLScrollListCtrl;
class LLGroupList;
class LLRelationship;
class LLPanel;
class LLTabContainer;

class FSFloaterContacts : public LLFloater, public LLEventTimer
{
public:
	FSFloaterContacts(const LLSD& seed);
	virtual ~FSFloaterContacts();
	
	/** 
	 * @brief This method either creates or brings to the front the
	 * current instantiation of this floater. There is only once since
	 * you can currently only look at your local friends.
	 */
	virtual BOOL tick();
	
	/** 
	 * @brief This method is called in response to the LLAvatarTracker
	 * sending out a changed() message.
	 */
	void updateFriends(U32 changed_mask);

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& key);

	static FSFloaterContacts* getInstance();
	static FSFloaterContacts* findInstance();

	void					sortFriendList();
	
	// void					updateFriendList();

	LLPanel*				mFriendsTab;
	// LLAvatarList*			mFriendList;
	LLScrollListCtrl*			mFriendsList;
	LLPanel*				mGroupsTab;
	LLGroupList*			mGroupList;

private:
	std::string				getActiveTabName() const;
	LLUUID					getCurrentItemID() const;
	void					getCurrentItemIDs(uuid_vec_t& selected_uuids) const;
	void					onAvatarListDoubleClicked(LLUICtrl* ctrl);

	enum FRIENDS_COLUMN_ORDER
	{
		LIST_ONLINE_STATUS,
		LIST_FRIEND_NAME,
		LIST_VISIBLE_ONLINE,
		LIST_VISIBLE_MAP,
		LIST_EDIT_MINE,
		LIST_VISIBLE_MAP_THEIRS,
		LIST_EDIT_THEIRS,
		LIST_FRIEND_UPDATE_GEN
	};

	typedef std::map<LLUUID, S32> rights_map_t;
	void					refreshNames(U32 changed_mask);
	BOOL					refreshNamesSync(const LLAvatarTracker::buddy_map_t & all_buddies);
	BOOL					refreshNamesPresence(const LLAvatarTracker::buddy_map_t & all_buddies);
	void					refreshRightsChangeList();
	void					refreshUI();
	void					onSelectName();
	void					applyRightsToFriends();
	BOOL					addFriend(const LLUUID& agent_id);	
	BOOL					updateFriendItem(const LLUUID& agent_id, const LLRelationship* relationship);

	typedef enum 
	{
		GRANT,
		REVOKE
	} EGrantRevoke;
	void					confirmModifyRights(rights_map_t& ids, EGrantRevoke command);
	void					sendRightsGrant(rights_map_t& ids);
	bool					modifyRightsConfirmation(const LLSD& notification, const LLSD& response, rights_map_t* rights);


	bool					isItemsFreeOfFriends(const uuid_vec_t& uuids);
	
	// misc callbacks
	static void				onAvatarPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names);
	
	// friend buttons
	void					onViewProfileButtonClicked();
	void					onImButtonClicked();
	void					onTeleportButtonClicked();
	void					onPayButtonClicked();
	void					onDeleteFriendButtonClicked();
	void					onAddFriendWizButtonClicked();
	
	// group buttons
	void					onGroupChatButtonClicked();
	void					onGroupInfoButtonClicked();
	void					onGroupActivateButtonClicked();
	void					onGroupLeaveButtonClicked();
	void					onGroupCreateButtonClicked();
	void					onGroupSearchButtonClicked();
	void					updateButtons();

	LLTabContainer*			mTabContainer;

	LLFriendObserver*		mObserver;
	BOOL					mAllowRightsChange;
	S32						mNumRightsChanged;
};


#endif // LL_FSCONTACTSFLOATER_H
