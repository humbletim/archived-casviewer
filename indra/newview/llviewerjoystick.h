/** 
 * @file llviewerjoystick.h
 * @brief Viewer joystick / NDOF device functionality.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_LLVIEWERJOYSTICK_H
#define LL_LLVIEWERJOYSTICK_H

#include "stdtypes.h"

#if LIB_NDOF
#include "ndofdev_external.h"
#else
#define NDOF_Device	void
#define NDOF_HotPlugResult S32
#endif

typedef enum e_joystick_driver_state
{
	JDS_UNINITIALIZED,
	JDS_INITIALIZED,
	JDS_INITIALIZING
} EJoystickDriverState;

// <CV:David>
typedef enum e_controller_type
{
	XBOX_CONTROLLER,
	SPACENAVIGATOR_CONTROLLER,
	UNKNOWN_CONTROLLER
} EControllerType;
// </CV:David>

class LLViewerJoystick : public LLSingleton<LLViewerJoystick>
{
public:
	LLViewerJoystick();
	virtual ~LLViewerJoystick();
	
	void init(bool autoenable);
	void terminate();

	void updateStatus();
	void scanJoystick();
	void moveObjects(bool reset = false);
	void moveAvatar(bool reset = false);
	void moveFlycam(bool reset = false);
	void moveCursor();  // <CV:David>
	F32 getJoystickAxis(U32 axis) const;
	U32 getJoystickButton(U32 button) const;
	bool isJoystickInitialized() const {return (mDriverState==JDS_INITIALIZED);}
	bool isLikeSpaceNavigator() const;
	bool isLikeXboxController() const;  // <CV:David>
	void setNeedsReset(bool reset = true) { mResetFlag = reset; }
	void setCameraNeedsUpdate(bool b)     { mCameraUpdated = b; }
	bool getCameraNeedsUpdate() const     { return mCameraUpdated; }
	bool getOverrideCamera() { return mOverrideCamera; }
	void setOverrideCamera(bool val);
	bool toggleFlycam();
	// <CV:David>
	bool toggleCursor();
	bool toggle3d();
	//void setSNDefaults();
	void setControllerDefaults();
	//std::string getDescription();
	std::string getDescription() const;
	std::string getDescriptionShort() const;
	// <CV:David>
	
protected:
	void updateEnabled(bool autoenable);
	void handleRun(F32 inc);
	void agentSlide(F32 inc);
	void agentPush(F32 inc);
	void agentFly(F32 inc);
	void agentPitch(F32 pitch_inc);
	void agentYaw(F32 yaw_inc);
	void agentJump();
	// <CV:David>
	void cursorSlide(F32 inc);
	void cursorPush(F32 incl);
	void cursorZoom(F32 ind);
	// </CV:David>
	void resetDeltas(S32 axis[]);
#if LIB_NDOF
	static NDOF_HotPlugResult HotPlugAddCallback(NDOF_Device *dev);
	static void HotPlugRemovalCallback(NDOF_Device *dev);
#endif
	
private:
	F32						mAxes[6];
	long					mBtn[16];
	EJoystickDriverState	mDriverState;
	NDOF_Device				*mNdofDev;
	bool					mResetFlag;
	F32						mPerfScale;
	bool					mCameraUpdated;
	bool 					mOverrideCamera;
	U32						mJoystickRun;
	
	static F32				sLastDelta[7];
	static F32				sDelta[7];

	// <CV:David>

	// Controllers.
	EControllerType mController;
	
	// Controller-specific defaults.
	void setSNDefaults();
	void setXboxControllerDefaults();

	// Control cursor instead of avatar?
	bool mControlCursor;

	// The SpaceNavigator has a maximum update rate which necessitates continuing previous movement between samples.
	LLFrameTimer mSampleTimer;
	bool mNewSample;
	F32 mCurrentMovement[7];

	// Need to issue two nudges at start of movement in order to start moving at slowest speed.
	bool mMoving;
	U32 mMovingNudges;

	LLTimer mDoubleClickTimer;

	// </CV:David>
};

#endif
