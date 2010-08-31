/** 
 * @file llagent.h
 * @brief LLAgent class header file
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

#ifndef LL_LLAGENT_H
#define LL_LLAGENT_H

#include "indra_constants.h"
#include "llevent.h" 				// LLObservable base class
#include "llagentaccess.h"
#include "llagentconstants.h"
#include "llagentdata.h" 			// gAgentID, gAgentSessionID
#include "llcharacter.h" 			// LLAnimPauseRequest
#include "llpointer.h"
#include "lluicolor.h"
#include "llvoavatardefines.h"
#include "llslurl.h"

extern const BOOL 	ANIMATE;
extern const U8 	AGENT_STATE_TYPING;  // Typing indication
extern const U8 	AGENT_STATE_EDITING; // Set when agent has objects selected

class LLChat;
class LLViewerRegion;
class LLMotion;
class LLToolset;
class LLMessageSystem;
class LLPermissions;
class LLHost;
class LLFriendObserver;
class LLPickInfo;
class LLViewerObject;
class LLAgentDropGroupViewerNode;

//--------------------------------------------------------------------
// Types
//--------------------------------------------------------------------

enum EAnimRequest
{
	ANIM_REQUEST_START,
	ANIM_REQUEST_STOP
};

struct LLGroupData
{
	LLUUID mID;
	LLUUID mInsigniaID;
	U64 mPowers;
	BOOL mAcceptNotices;
	BOOL mListInProfile;
	S32 mContribution;
	std::string mName;
};

class LLAgentListener;

//------------------------------------------------------------------------
// LLAgent
//------------------------------------------------------------------------
class LLAgent : public LLOldEvents::LLObservable
{
	LOG_CLASS(LLAgent);

public:
	friend class LLAgentDropGroupViewerNode;

/********************************************************************************
 **                                                                            **
 **                    INITIALIZATION
 **/

	//--------------------------------------------------------------------
	// Constructors / Destructors
	//--------------------------------------------------------------------
public:
	LLAgent();
	virtual 		~LLAgent();
	void			init();
	void			cleanup();

	//--------------------------------------------------------------------
	// Login
	//--------------------------------------------------------------------
public:
	void			onAppFocusGained();
	void			setFirstLogin(BOOL b) 	{ mFirstLogin = b; }
	// Return TRUE if the database reported this login as the first for this particular user.
	BOOL 			isFirstLogin() const 	{ return mFirstLogin; }
	BOOL 			isInitialized() const 	{ return mInitialized; }
public:
	std::string		mMOTD; 					// Message of the day
private:
	BOOL			mInitialized;
	BOOL			mFirstLogin;
	boost::shared_ptr<LLAgentListener> mListener;

	//--------------------------------------------------------------------
	// Session
	//--------------------------------------------------------------------
public:
	const LLUUID&	getID() const				{ return gAgentID; }
	const LLUUID&	getSessionID() const		{ return gAgentSessionID; }
	// Note: NEVER send this value in the clear or over any weakly
	// encrypted channel (such as simple XOR masking).  If you are unsure
	// ask Aaron or MarkL.
	const LLUUID&	getSecureSessionID() const	{ return mSecureSessionID; }
public:
	LLUUID			mSecureSessionID; 			// Secure token for this login session
	
/**                    Initialization
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    IDENTITY
 **/

	//--------------------------------------------------------------------
	// Name
	//--------------------------------------------------------------------
public:
	//*TODO remove, is not used as of August 20, 2009
	void			buildFullnameAndTitle(std::string &name) const;

	//--------------------------------------------------------------------
	// Gender
	//--------------------------------------------------------------------
public:
	// On the very first login, gender isn't chosen until the user clicks
	// in a dialog.  We don't render the avatar until they choose.
	BOOL 			isGenderChosen() const 	{ return mGenderChosen; }
	void			setGenderChosen(BOOL b)	{ mGenderChosen = b; }
private:
	BOOL			mGenderChosen;

/**                    Identity
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    POSITION
 **/

  	//--------------------------------------------------------------------
 	// Position
	//--------------------------------------------------------------------
public:
	LLVector3		getPosAgentFromGlobal(const LLVector3d &pos_global) const;
	LLVector3d		getPosGlobalFromAgent(const LLVector3 &pos_agent) const;	
	const LLVector3d &getPositionGlobal() const;
	const LLVector3	&getPositionAgent();
	// Call once per frame to update position, angles (radians).
	void			updateAgentPosition(const F32 dt, const F32 yaw, const S32 mouse_x, const S32 mouse_y);	
	void			setPositionAgent(const LLVector3 &center);
protected:
	void			propagate(const F32 dt); // ! BUG ! Should roll into updateAgentPosition
private:
	mutable LLVector3d mPositionGlobal;

  	//--------------------------------------------------------------------
 	// Velocity
	//--------------------------------------------------------------------
public:
	LLVector3		getVelocity() const;
	F32				getVelocityZ() const 	{ return getVelocity().mV[VZ]; } // ! HACK !
	
  	//--------------------------------------------------------------------
	// Coordinate System
	//--------------------------------------------------------------------
public:
	const LLCoordFrame&	getFrameAgent()	const	{ return mFrameAgent; }
	void 			initOriginGlobal(const LLVector3d &origin_global); // Only to be used in ONE place
	void			resetAxes();
	void			resetAxes(const LLVector3 &look_at); // Makes reasonable left and up
	// The following three get*Axis functions return direction avatar is looking, not camera.
	const LLVector3& getAtAxis() const		{ return mFrameAgent.getAtAxis(); }
	const LLVector3& getUpAxis() const		{ return mFrameAgent.getUpAxis(); }
	const LLVector3& getLeftAxis() const	{ return mFrameAgent.getLeftAxis(); }
	LLQuaternion	getQuat() const; 		// Returns the quat that represents the rotation of the agent in the absolute frame
private:
	LLVector3d		mAgentOriginGlobal;		// Origin of agent coords from global coords
	LLCoordFrame	mFrameAgent; 			// Agent position and view, agent-region coordinates


	//--------------------------------------------------------------------
	// Home
	//--------------------------------------------------------------------
public:
	void			setStartPosition(U32 location_id); // Marks current location as start, sends information to servers
	void			setHomePosRegion(const U64& region_handle, const LLVector3& pos_region);
	BOOL			getHomePosGlobal(LLVector3d* pos_global);
private:
	BOOL 			mHaveHomePosition;
	U64				mHomeRegionHandle;
	LLVector3		mHomePosRegion;

	//--------------------------------------------------------------------
	// Region
	//--------------------------------------------------------------------
public:
	void			setRegion(LLViewerRegion *regionp);
	LLViewerRegion	*getRegion() const;
	LLHost			getRegionHost() const;
	BOOL			inPrelude();
private:
	LLViewerRegion	*mRegionp;

	//--------------------------------------------------------------------
	// History
	//--------------------------------------------------------------------
public:
	S32				getRegionsVisited() const;
	F64				getDistanceTraveled() const;	
	void			setDistanceTraveled(F64 dist) { mDistanceTraveled = dist; }
	
	const LLVector3d &getLastPositionGlobal() const { return mLastPositionGlobal; }
	void			setLastPositionGlobal(const LLVector3d &pos) { mLastPositionGlobal = pos; }
private:
	std::set<U64>	mRegionsVisited;		// Stat - what distinct regions has the avatar been to?
	F64				mDistanceTraveled;		// Stat - how far has the avatar moved?
	LLVector3d		mLastPositionGlobal;	// Used to calculate travel distance
	
/**                    Position
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    ACTIONS
 **/

	//--------------------------------------------------------------------
	// Fidget
	//--------------------------------------------------------------------
	// Trigger random fidget animations
public:
	void			fidget();
	static void		stopFidget();
private:
	LLFrameTimer	mFidgetTimer;
	LLFrameTimer	mFocusObjectFadeTimer;
	F32				mNextFidgetTime;
	S32				mCurrentFidget;

	//--------------------------------------------------------------------
	// Fly
	//--------------------------------------------------------------------
public:
	BOOL			getFlying() const;
	void			setFlying(BOOL fly);
	static void		toggleFlying();
	static bool		enableFlying();
	BOOL			canFly(); 			// Does this parcel allow you to fly?
	
	//--------------------------------------------------------------------
	// Chat
	//--------------------------------------------------------------------
public:
	void			heardChat(const LLUUID& id);
	F32				getTypingTime() 		{ return mTypingTimer.getElapsedTimeF32(); }
	LLUUID			getLastChatter() const 	{ return mLastChatterID; }
	F32				getNearChatRadius() 	{ return mNearChatRadius; }
protected:
	void 			ageChat(); 				// Helper function to prematurely age chat when agent is moving
private:
	LLFrameTimer	mChatTimer;
	LLUUID			mLastChatterID;
	F32				mNearChatRadius;
	
	//--------------------------------------------------------------------
	// Typing
	//--------------------------------------------------------------------
public:
	void			startTyping();
	void			stopTyping();
public:
	// When the agent hasn't typed anything for this duration, it leaves the 
	// typing state (for both chat and IM).
	static const F32 TYPING_TIMEOUT_SECS;
private:
	LLFrameTimer	mTypingTimer;

	//--------------------------------------------------------------------
	// AFK
	//--------------------------------------------------------------------
public:
	void			setAFK();
	void			clearAFK();
	BOOL			getAFK() const;

	//--------------------------------------------------------------------
	// Run
	//--------------------------------------------------------------------
public:
	enum EDoubleTapRunMode
	{
		DOUBLETAP_NONE,
		DOUBLETAP_FORWARD,
		DOUBLETAP_BACKWARD,
		DOUBLETAP_SLIDELEFT,
		DOUBLETAP_SLIDERIGHT
	};

	void			setAlwaysRun() 			{ mbAlwaysRun = true; }
	void			clearAlwaysRun() 		{ mbAlwaysRun = false; }
	void			setRunning() 			{ mbRunning = true; }
	void			clearRunning() 			{ mbRunning = false; }
	void 			sendWalkRun(bool running);
	bool			getAlwaysRun() const 	{ return mbAlwaysRun; }
	bool			getRunning() const 		{ return mbRunning; }
public:
	LLFrameTimer 	mDoubleTapRunTimer;
	EDoubleTapRunMode mDoubleTapRunMode;
private:
	bool 			mbAlwaysRun; 			// Should the avatar run by default rather than walk?
	bool 			mbRunning;				// Is the avatar trying to run right now?

	//--------------------------------------------------------------------
	// Sit and stand
	//--------------------------------------------------------------------
public:
	void			standUp();
	/// @brief ground-sit at agent's current position
	void			sitDown();

	//--------------------------------------------------------------------
	// Busy
	//--------------------------------------------------------------------
public:
	void			setBusy();
	void			clearBusy();
	BOOL			getBusy() const;
private:
	BOOL			mIsBusy;

	//--------------------------------------------------------------------
	// Jump
	//--------------------------------------------------------------------
public:
	BOOL			getJump() const	{ return mbJump; }
private:
	BOOL 			mbJump;

	//--------------------------------------------------------------------
	// Grab
	//--------------------------------------------------------------------
public:
	BOOL 			leftButtonGrabbed() const;
	BOOL 			rotateGrabbed() const;
	BOOL 			forwardGrabbed() const;
	BOOL 			backwardGrabbed() const;
	BOOL 			upGrabbed() const;
	BOOL 			downGrabbed() const;

	//--------------------------------------------------------------------
	// Controls
	//--------------------------------------------------------------------
public:
	U32 			getControlFlags(); 
	void 			setControlFlags(U32 mask); 		// Performs bitwise mControlFlags |= mask
	void 			clearControlFlags(U32 mask); 	// Performs bitwise mControlFlags &= ~mask
	BOOL			controlFlagsDirty() const;
	void			enableControlFlagReset();
	void 			resetControlFlags();
	BOOL			anyControlGrabbed() const; 		// True iff a script has taken over a control
	BOOL			isControlGrabbed(S32 control_index) const;
	// Send message to simulator to force grabbed controls to be
	// released, in case of a poorly written script.
	void			forceReleaseControls();
	void			setFlagsDirty() { mbFlagsDirty = TRUE; }

private:
	S32				mControlsTakenCount[TOTAL_CONTROLS];
	S32				mControlsTakenPassedOnCount[TOTAL_CONTROLS];
	U32				mControlFlags;					// Replacement for the mFooKey's
	BOOL 			mbFlagsDirty;
	BOOL 			mbFlagsNeedReset;				// ! HACK ! For preventing incorrect flags sent when crossing region boundaries
	
	//--------------------------------------------------------------------
	// Animations
	//--------------------------------------------------------------------
public:
	void            stopCurrentAnimations();
	void			requestStopMotion(LLMotion* motion);
	void			onAnimStop(const LLUUID& id);
	void			sendAnimationRequests(LLDynamicArray<LLUUID> &anim_ids, EAnimRequest request);
	void			sendAnimationRequest(const LLUUID &anim_id, EAnimRequest request);
	void			endAnimationUpdateUI();
	void			unpauseAnimation() { mPauseRequest = NULL; }
	BOOL			getCustomAnim() const { return mCustomAnim; }
	void			setCustomAnim(BOOL anim) { mCustomAnim = anim; }
	
private:
	BOOL            mCustomAnim; 		// Current animation is ANIM_AGENT_CUSTOMIZE ?
	LLAnimPauseRequest mPauseRequest;
	BOOL			mViewsPushed; 		// Keep track of whether or not we have pushed views
	
/**                    Animation
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    MOVEMENT
 **/

	//--------------------------------------------------------------------
	// Movement from user input
	//--------------------------------------------------------------------
	// All set the appropriate animation flags.
	// All turn off autopilot and make sure the camera is behind the avatar.
	// Direction is either positive, zero, or negative
public:
	void			moveAt(S32 direction, bool reset_view = true);
	void			moveAtNudge(S32 direction);
	void			moveLeft(S32 direction);
	void			moveLeftNudge(S32 direction);
	void			moveUp(S32 direction);
	void			moveYaw(F32 mag, bool reset_view = true);
	void			movePitch(F32 mag);

	//--------------------------------------------------------------------
 	// Move the avatar's frame
	//--------------------------------------------------------------------
public:
	void			rotate(F32 angle, const LLVector3 &axis);
	void			rotate(F32 angle, F32 x, F32 y, F32 z);
	void			rotate(const LLMatrix3 &matrix);
	void			rotate(const LLQuaternion &quaternion);
	void			pitch(F32 angle);
	void			roll(F32 angle);
	void			yaw(F32 angle);
	LLVector3		getReferenceUpVector();
    F32             clampPitchToLimits(F32 angle);

	//--------------------------------------------------------------------
	// Autopilot
	//--------------------------------------------------------------------
public:
	BOOL			getAutoPilot() const				{ return mAutoPilot; }
	LLVector3d		getAutoPilotTargetGlobal() const 	{ return mAutoPilotTargetGlobal; }
	void			startAutoPilotGlobal(const LLVector3d &pos_global, 
										 const std::string& behavior_name = std::string(), 
										 const LLQuaternion *target_rotation = NULL, 
										 void (*finish_callback)(BOOL, void *) = NULL, void *callback_data = NULL, 
										 F32 stop_distance = 0.f, F32 rotation_threshold = 0.03f);
	void 			startFollowPilot(const LLUUID &leader_id);
	void			stopAutoPilot(BOOL user_cancel = FALSE);
	void 			setAutoPilotGlobal(const LLVector3d &pos_global);
	void			autoPilot(F32 *delta_yaw); 			// Autopilot walking action, angles in radians
	void			renderAutoPilotTarget();
private:
	BOOL			mAutoPilot;
	BOOL			mAutoPilotFlyOnStop;
	LLVector3d		mAutoPilotTargetGlobal;
	F32				mAutoPilotStopDistance;
	BOOL			mAutoPilotUseRotation;
	LLVector3		mAutoPilotTargetFacing;
	F32				mAutoPilotTargetDist;
	S32				mAutoPilotNoProgressFrameCount;
	F32				mAutoPilotRotationThreshold;
	std::string		mAutoPilotBehaviorName;
	void			(*mAutoPilotFinishedCallback)(BOOL, void *);
	void*			mAutoPilotCallbackData;
	LLUUID			mLeaderID;
	
/**                    Movement
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    TELEPORT
 **/

public:
	enum ETeleportState
	{
		TELEPORT_NONE = 0,			// No teleport in progress
		TELEPORT_START = 1,			// Transition to REQUESTED.  Viewer has sent a TeleportRequest to the source simulator
		TELEPORT_REQUESTED = 2,		// Waiting for source simulator to respond
		TELEPORT_MOVING = 3,		// Viewer has received destination location from source simulator
		TELEPORT_START_ARRIVAL = 4,	// Transition to ARRIVING.  Viewer has received avatar update, etc., from destination simulator
		TELEPORT_ARRIVING = 5		// Make the user wait while content "pre-caches"
	};

public:
	static void 	parseTeleportMessages(const std::string& xml_filename);
	const void getTeleportSourceSLURL(LLSLURL& slurl) const { slurl = mTeleportSourceSLURL; }
public:
	// ! TODO ! Define ERROR and PROGRESS enums here instead of exposing the mappings.
	static std::map<std::string, std::string> sTeleportErrorMessages;
	static std::map<std::string, std::string> sTeleportProgressMessages;
private:
	LLSLURL	mTeleportSourceSLURL; 			// SLURL where last TP began

	//--------------------------------------------------------------------
	// Teleport Actions
	//--------------------------------------------------------------------
public:
	void 			teleportRequest(const U64& region_handle,
									const LLVector3& pos_local);			// Go to a named location home
	void 			teleportViaLandmark(const LLUUID& landmark_id);			// Teleport to a landmark
	void 			teleportHome()	{ teleportViaLandmark(LLUUID::null); }	// Go home
	void 			teleportViaLure(const LLUUID& lure_id, BOOL godlike);	// To an invited location
	void 			teleportViaLocation(const LLVector3d& pos_global);		// To a global location - this will probably need to be deprecated
	void 			teleportCancel();										// May or may not be allowed by server
protected:
	bool 			teleportCore(bool is_local = false); 					// Stuff for all teleports; returns true if the teleport can proceed

	//--------------------------------------------------------------------
	// Teleport State
	//--------------------------------------------------------------------
public:
	ETeleportState	getTeleportState() const 						{ return mTeleportState; }
	void			setTeleportState(ETeleportState state);
private:
	ETeleportState	mTeleportState;

	//--------------------------------------------------------------------
	// Teleport Message
	//--------------------------------------------------------------------
public:
	const std::string& getTeleportMessage() const 					{ return mTeleportMessage; }
	void 			setTeleportMessage(const std::string& message) 	{ mTeleportMessage = message; }
private:
	std::string		mTeleportMessage;
	
/**                    Teleport
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    ACCESS
 **/

public:
	// Checks if agent can modify an object based on the permissions and the agent's proxy status.
	BOOL			isGrantedProxy(const LLPermissions& perm);
	BOOL			allowOperation(PermissionBit op,
								   const LLPermissions& perm,
								   U64 group_proxy_power = 0,
								   U8 god_minimum = GOD_MAINTENANCE);
	const LLAgentAccess& getAgentAccess();
	BOOL			canManageEstate() const;
	BOOL			getAdminOverride() const;
	// ! BACKWARDS COMPATIBILITY ! This function can go away after the AO transition (see llstartup.cpp).
	void 			setAOTransition();
private:
	LLAgentAccess 	mAgentAccess;
	
	//--------------------------------------------------------------------
	// God
	//--------------------------------------------------------------------
public:
	bool			isGodlike() const;
	bool			isGodlikeWithoutAdminMenuFakery() const;
	U8				getGodLevel() const;
	void			setAdminOverride(BOOL b);
	void			setGodLevel(U8 god_level);
	void			requestEnterGodMode();
	void			requestLeaveGodMode();

	//--------------------------------------------------------------------
	// Maturity
	//--------------------------------------------------------------------
public:
	// Note: this is a prime candidate for pulling out into a Maturity class.
	// Rather than just expose the preference setting, we're going to actually
	// expose what the client code cares about -- what the user should see
	// based on a combination of the is* and prefers* flags, combined with god bit.
	bool 			wantsPGOnly() const;
	bool 			canAccessMature() const;
	bool 			canAccessAdult() const;
	bool 			canAccessMaturityInRegion( U64 region_handle ) const;
	bool 			canAccessMaturityAtGlobal( LLVector3d pos_global ) const;
	bool 			prefersPG() const;
	bool 			prefersMature() const;
	bool 			prefersAdult() const;
	bool 			isTeen() const;
	bool 			isMature() const;
	bool 			isAdult() const;
	void 			setTeen(bool teen);
	void 			setMaturity(char text);
	static int 		convertTextToMaturity(char text); 
	bool 			sendMaturityPreferenceToServer(int preferredMaturity); // ! "U8" instead of "int"?

	// Maturity callbacks for PreferredMaturity control variable
	void 			handleMaturity(const LLSD& newvalue);
	bool 			validateMaturity(const LLSD& newvalue);


/**                    Access
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    RENDERING
 **/

public:
	LLQuaternion	getHeadRotation();
	BOOL			needsRenderAvatar(); // TRUE when camera mode is such that your own avatar should draw
	BOOL			needsRenderHead();
	void			setShowAvatar(BOOL show) { mShowAvatar = show; }
	BOOL			getShowAvatar() const { return mShowAvatar; }
	
private:
	BOOL			mShowAvatar; 		// Should we render the avatar?
	U32				mAppearanceSerialNum;
	
	//--------------------------------------------------------------------
	// Rendering state bitmap helpers
	//--------------------------------------------------------------------
public:
	void			setRenderState(U8 newstate);
	void			clearRenderState(U8 clearstate);
	U8				getRenderState();
private:
	U8				mRenderState; // Current behavior state of agent

	//--------------------------------------------------------------------
	// HUD
	//--------------------------------------------------------------------
public:
	const LLColor4	&getEffectColor();
	void			setEffectColor(const LLColor4 &color);
private:
	LLUIColor 		mEffectColor;

/**                    Rendering
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    GROUPS
 **/

public:
	const LLUUID	&getGroupID() const			{ return mGroupID; }
	// Get group information by group_id, or FALSE if not in group.
	BOOL 			getGroupData(const LLUUID& group_id, LLGroupData& data) const;
	// Get just the agent's contribution to the given group.
	S32 			getGroupContribution(const LLUUID& group_id) const;
	// Update internal datastructures and update the server.
	BOOL 			setGroupContribution(const LLUUID& group_id, S32 contribution);
	BOOL 			setUserGroupFlags(const LLUUID& group_id, BOOL accept_notices, BOOL list_in_profile);
	const std::string &getGroupName() const 	{ return mGroupName; }
	BOOL			canJoinGroups() const;
private:
	std::string		mGroupName;
	LLUUID			mGroupID;

	//--------------------------------------------------------------------
	// Group Membership
	//--------------------------------------------------------------------
public:
	// Checks against all groups in the entire agent group list.
	BOOL 			isInGroup(const LLUUID& group_id, BOOL ingnore_God_mod = FALSE) const;
protected:
	// Only used for building titles.
	BOOL			isGroupMember() const 		{ return !mGroupID.isNull(); } 
public:
	LLDynamicArray<LLGroupData> mGroups;

	//--------------------------------------------------------------------
	// Group Title
	//--------------------------------------------------------------------
public:
	void			setHideGroupTitle(BOOL hide)	{ mHideGroupTitle = hide; }
	BOOL			isGroupTitleHidden() const 		{ return mHideGroupTitle; }
private:
	std::string		mGroupTitle; 					// Honorific, like "Sir"
	BOOL			mHideGroupTitle;

	//--------------------------------------------------------------------
	// Group Powers
	//--------------------------------------------------------------------
public:
	BOOL 			hasPowerInGroup(const LLUUID& group_id, U64 power) const;
	BOOL 			hasPowerInActiveGroup(const U64 power) const;
	U64  			getPowerInGroup(const LLUUID& group_id) const;
 	U64				mGroupPowers;

	//--------------------------------------------------------------------
	// Friends
	//--------------------------------------------------------------------
public:
	void 			observeFriends();
	void 			friendsChanged();
private:
	LLFriendObserver* mFriendObserver;
	std::set<LLUUID> mProxyForAgents;

/**                    Groups
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    MESSAGING
 **/

	//--------------------------------------------------------------------
	// Send
	//--------------------------------------------------------------------
public:
	void			sendMessage(); // Send message to this agent's region
	void			sendReliableMessage();
	void			sendAgentSetAppearance();
	void 			sendAgentDataUpdateRequest();
	void 			sendAgentUserInfoRequest();
	// IM to Email and Online visibility
	void			sendAgentUpdateUserInfo(bool im_to_email, const std::string& directory_visibility);

	//--------------------------------------------------------------------
	// Receive
	//--------------------------------------------------------------------
public:
	static void		processAgentDataUpdate(LLMessageSystem *msg, void **);
	static void		processAgentGroupDataUpdate(LLMessageSystem *msg, void **);
	static void		processAgentDropGroup(LLMessageSystem *msg, void **);
	static void		processScriptControlChange(LLMessageSystem *msg, void **);
	static void		processAgentCachedTextureResponse(LLMessageSystem *mesgsys, void **user_data);
	
/**                    Messaging
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    DEBUGGING
 **/

public:
	static void		dumpGroupInfo();
	static void		clearVisualParams(void *);
	friend std::ostream& operator<<(std::ostream &s, const LLAgent &sphere);

/**                    Debugging
 **                                                                            **
 *******************************************************************************/

};

extern LLAgent gAgent;

inline bool operator==(const LLGroupData &a, const LLGroupData &b)
{
	return (a.mID == b.mID);
}

class LLAgentQueryManager
{
	friend class LLAgent;
	friend class LLAgentWearables;
	
public:
	LLAgentQueryManager();
	virtual ~LLAgentQueryManager();
	
	BOOL 			hasNoPendingQueries() const 	{ return getNumPendingQueries() == 0; }
	S32 			getNumPendingQueries() const 	{ return mNumPendingQueries; }
private:
	S32				mNumPendingQueries;
	S32				mWearablesCacheQueryID;
	U32				mUpdateSerialNum;
	S32		    	mActiveCacheQueries[LLVOAvatarDefines::BAKED_NUM_INDICES];
};

extern LLAgentQueryManager gAgentQueryManager;

#endif
