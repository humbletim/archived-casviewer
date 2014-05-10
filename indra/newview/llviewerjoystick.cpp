/** 
 * @file llviewerjoystick.cpp
 * @brief Joystick / NDOF device functionality.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

// </FS:ND> Fix for GCC 4.4.5, oherwise it complains at boost/signal
#ifdef LL_LINUX
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif
// </FS:ND>

#include "llviewerprecompiledheaders.h"

#include "llviewerjoystick.h"

#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llviewercamera.h"
#include "llappviewer.h"
#include "llkeyboard.h"
#include "lltoolmgr.h"
#include "llselectmgr.h"
#include "llviewermenu.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llfocusmgr.h"
#include "llmoveview.h"

// <CV:David>
#include "llviewerdisplay.h"
#include "llviewerkeyboard.h"
#include <boost/regex.hpp>
// </CV:David>

// ----------------------------------------------------------------------------
// Constants

#define  X_I	1
#define  Y_I	2
#define  Z_I	0
#define RX_I	4
#define RY_I	5
#define RZ_I	3

F32  LLViewerJoystick::sLastDelta[] = {0,0,0,0,0,0,0};
F32  LLViewerJoystick::sDelta[] = {0,0,0,0,0,0,0};

// These constants specify the maximum absolute value coming in from the device.
// HACK ALERT! the value of MAX_JOYSTICK_INPUT_VALUE is not arbitrary as it 
// should be.  It has to be equal to 3000 because the SpaceNavigator on Windows
// refuses to respond to the DirectInput SetProperty call; it always returns 
// values in the [-3000, 3000] range.
#define MAX_SPACENAVIGATOR_INPUT  3000.0f
#define MAX_JOYSTICK_INPUT_VALUE  MAX_SPACENAVIGATOR_INPUT

// <CV:David>
const F32 SAMPLE_TIME = 0.02f;		// Empirically determined. In seconds.

const U32 XBOX_A_KEY = 0;			// Xbox keys.
const U32 XBOX_B_KEY = 1;
const U32 XBOX_X_KEY = 2;
const U32 XBOX_Y_KEY = 3;
const U32 XBOX_L_BUMP_KEY = 4;
const U32 XBOX_R_BUMP_KEY = 5;
const U32 XBOX_BACK_KEY = 6;
const U32 XBOX_START_KEY = 7;
const U32 XBOX_L_STICK_CLICK = 8;
const U32 XBOX_R_STICK_CLICK = 9;

const F32 DOUBLE_CLICK_INTERVAL = 0.5f;	// Windows default.
// </CV:David>

// -----------------------------------------------------------------------------
void LLViewerJoystick::updateEnabled(bool autoenable)
{
	if (mDriverState == JDS_UNINITIALIZED)
	{
		gSavedSettings.setBOOL("JoystickEnabled", FALSE );
	}
	else
	{
		if (isLikeSpaceNavigator() && autoenable)
		{
			gSavedSettings.setBOOL("JoystickEnabled", TRUE );
		}
	}
	if (!gSavedSettings.getBOOL("JoystickEnabled"))
	{
		mOverrideCamera = FALSE;
	}
}

void LLViewerJoystick::setOverrideCamera(bool val)
{
	if (!gSavedSettings.getBOOL("JoystickEnabled"))
	{
		mOverrideCamera = FALSE;
	}
	else
	{
		mOverrideCamera = val;
	}

	if (mOverrideCamera)
	{
		gAgentCamera.changeCameraToDefault();
	}
}

// -----------------------------------------------------------------------------
#if LIB_NDOF
NDOF_HotPlugResult LLViewerJoystick::HotPlugAddCallback(NDOF_Device *dev)
{
	NDOF_HotPlugResult res = NDOF_DISCARD_HOTPLUGGED;
	LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
	if (joystick->mDriverState == JDS_UNINITIALIZED)
	{
        llinfos << "HotPlugAddCallback: will use device:" << llendl;
		ndof_dump(dev);
		joystick->mNdofDev = dev;
        joystick->mDriverState = JDS_INITIALIZED;
        res = NDOF_KEEP_HOTPLUGGED;
	}
	joystick->updateEnabled(true);
    return res;
}
#endif

// -----------------------------------------------------------------------------
#if LIB_NDOF
void LLViewerJoystick::HotPlugRemovalCallback(NDOF_Device *dev)
{
	LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
	if (joystick->mNdofDev == dev)
	{
        llinfos << "HotPlugRemovalCallback: joystick->mNdofDev=" 
				<< joystick->mNdofDev << "; removed device:" << llendl;
		ndof_dump(dev);
		joystick->mDriverState = JDS_UNINITIALIZED;
	}
	joystick->updateEnabled(true);
}
#endif

// -----------------------------------------------------------------------------
LLViewerJoystick::LLViewerJoystick()
:	mDriverState(JDS_UNINITIALIZED),
	mNdofDev(NULL),
	mResetFlag(false),
	mCameraUpdated(true),
	mOverrideCamera(false),
	mJoystickRun(0)
{
	for (int i = 0; i < 6; i++)
	{
		mAxes[i] = sDelta[i] = sLastDelta[i] = 0.0f;
	}
	
	memset(mBtn, 0, sizeof(mBtn));

	// factor in bandwidth? bandwidth = gViewerStats->mKBitStat
	mPerfScale = 4000.f / gSysCPU.getMHz(); // hmm.  why?

	// <CV:David>
	mSampleTimer.start();
	mNewSample = false;
	memset(mCurrentMovement, 0, sizeof(mCurrentMovement));

	mMoving = false;
	mMovingNudges = 0;

	mControlCursor = false;

	mDoubleClickTimer.start();
	// </CV:David>
}

// -----------------------------------------------------------------------------
LLViewerJoystick::~LLViewerJoystick()
{
	// <CV:David>
	mSampleTimer.stop();
	// </CV:David>

	if (mDriverState == JDS_INITIALIZED)
	{
		terminate();
	}
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::init(bool autoenable)
{
#if LIB_NDOF
	static bool libinit = false;
	mDriverState = JDS_INITIALIZING;

	if (libinit == false)
	{
		// Note: The HotPlug callbacks are not actually getting called on Windows
		if (ndof_libinit(HotPlugAddCallback, 
						 HotPlugRemovalCallback, 
						 NULL))
		{
			mDriverState = JDS_UNINITIALIZED;
		}
		else
		{
			// NB: ndof_libinit succeeds when there's no device
			libinit = true;

			// allocate memory once for an eventual device
			mNdofDev = ndof_create();
		}
	}

	if (libinit)
	{
		if (mNdofDev)
		{
			// Different joysticks will return different ranges of raw values.
			// Since we want to handle every device in the same uniform way, 
			// we initialize the mNdofDev struct and we set the range 
			// of values we would like to receive. 
			// 
			// HACK: On Windows, libndofdev passes our range to DI with a 
			// SetProperty call. This works but with one notable exception, the
			// SpaceNavigator, who doesn't seem to care about the SetProperty
			// call. In theory, we should handle this case inside libndofdev. 
			// However, the range we're setting here is arbitrary anyway, 
			// so let's just use the SpaceNavigator range for our purposes. 
			mNdofDev->axes_min = (long)-MAX_JOYSTICK_INPUT_VALUE;
			mNdofDev->axes_max = (long)+MAX_JOYSTICK_INPUT_VALUE;

			// libndofdev could be used to return deltas.  Here we choose to
			// just have the absolute values instead.
			mNdofDev->absolute = 1;

			// init & use the first suitable NDOF device found on the USB chain
			if (ndof_init_first(mNdofDev, NULL))
			{
				mDriverState = JDS_UNINITIALIZED;
				llwarns << "ndof_init_first FAILED" << llendl;
			}
			else
			{
				mDriverState = JDS_INITIALIZED;
			}
		}
		else
		{
			mDriverState = JDS_UNINITIALIZED;
		}
	}

	// Autoenable the joystick for recognized devices if nothing was connected previously
	if (!autoenable)
	{
		autoenable = gSavedSettings.getString("JoystickInitialized").empty() ? true : false;
	}
	updateEnabled(autoenable);
	
	if (mDriverState == JDS_INITIALIZED)
	{
		// A Joystick device is plugged in
		if (isLikeSpaceNavigator())
		{
			mController = SPACENAVIGATOR_CONTROLLER;  // <CV:David>

			// It's a space navigator, we have defaults for it.
			if (gSavedSettings.getString("JoystickInitialized") != "SpaceNavigator")
			{
				// Only set the defaults if we haven't already (in case they were overridden)
				setSNDefaults();
				gSavedSettings.setString("JoystickInitialized", "SpaceNavigator");
			}
		}
		// <CV:David>
		if (isLikeXboxController())
		{
			mController = XBOX_CONTROLLER;

			// It's an Xbox controller, we have defaults for it.
			if (gSavedSettings.getString("JoystickInitialized") != "XboxController")
			{
				// Only set the defaults if we haven't already (in case they were overridden)
				setXboxControllerDefaults();
				gSavedSettings.setString("JoystickInitialized", "XboxController");
			}
		}
		// </CV:David>
		else
		{
			// It's not a Space Navigator <CV:David> or Xbox 360 </CV:David>
			mController = UNKNOWN_CONTROLLER;  // <CV:David>
			gSavedSettings.setString("JoystickInitialized", "UnknownDevice");
		}
	}
	else
	{
		// No device connected, don't change any settings
	}
	
	llinfos << "ndof: mDriverState=" << mDriverState << "; mNdofDev=" 
			<< mNdofDev << "; libinit=" << libinit << llendl;

	// <CV:David>
	if (mDriverState == JDS_INITIALIZED)
	{
		llinfos << "Joystick = " << getDescription() << llendl;
	}
	// </CV:David>

#endif
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::terminate()
{
#if LIB_NDOF

	ndof_libcleanup();
	llinfos << "Terminated connection with NDOF device." << llendl;
	mDriverState = JDS_UNINITIALIZED;
#endif
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::updateStatus()
{
#if LIB_NDOF

	ndof_update(mNdofDev);

	for (int i=0; i<6; i++)
	{
		mAxes[i] = (F32) mNdofDev->axes[i] / mNdofDev->axes_max;
	}

	for (int i=0; i<16; i++)
	{
		mBtn[i] = mNdofDev->buttons[i];
	}
	
#endif
}

// -----------------------------------------------------------------------------
F32 LLViewerJoystick::getJoystickAxis(U32 axis) const
{
	if (axis < 6)
	{
		return mAxes[axis];
	}
	return 0.f;
}

// -----------------------------------------------------------------------------
U32 LLViewerJoystick::getJoystickButton(U32 button) const
{
	if (button < 16)
	{
		return mBtn[button];
	}
	return 0;
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::handleRun(F32 inc)
{
	// Decide whether to walk or run by applying a threshold, with slight
	// hysteresis to avoid oscillating between the two with input spikes.
	// Analog speed control would be better, but not likely any time soon.
	if (inc > gSavedSettings.getF32("JoystickRunThreshold"))
	{
		if (1 == mJoystickRun)
		{
			++mJoystickRun;
//			gAgent.setRunning();
//			gAgent.sendWalkRun(gAgent.getRunning());
// [RLVa:KB] - Checked: 2011-05-11 (RLVa-1.3.0i) | Added: RLVa-1.3.0i
			gAgent.setTempRun();
// [/RLVa:KB]
		}
		else if (0 == mJoystickRun)
		{
			// hysteresis - respond NEXT frame
			++mJoystickRun;
		}
	}
	else
	{
		if (mJoystickRun > 0)
		{
			--mJoystickRun;
			if (0 == mJoystickRun)
			{
//				gAgent.clearRunning();
//				gAgent.sendWalkRun(gAgent.getRunning());
// [RLVa:KB] - Checked: 2011-05-11 (RLVa-1.3.0i) | Added: RLVa-1.3.0i
				gAgent.clearTempRun();
// [/RLVa:KB]
			}
		}
	}
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentJump()
{
	// <CV:David>
	//gAgent.moveUp(1);
	gAgent.moveJump();
	// </CV:David>
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentSlide(F32 inc)
{
	// <CV:David>
	/*
	if (inc < 0.f)
	{
		gAgent.moveLeft(1);
	}
	else if (inc > 0.f)
	{
		gAgent.moveLeft(-1);
	}
	*/

	static F32 previousSlideInc = 0.f;		  // Smooth a little.

	if (inc != 0.f)
	{
		F32 slide = -(inc + previousSlideInc) / (2.f * gSavedSettings.getF32("JoystickRunThreshold"));

		if (mMovingNudges < 2)
		{
			gAgent.moveLeftNudge(slide < 0 ? -1 : 1);
			mMovingNudges += 1;
		}
		else
		{
			gAgent.moveLeft(slide);
		}
	}

	previousSlideInc = inc;
	// </CV:David>
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentPush(F32 inc)
{
	// <CV:David>
	/*
	if (inc < 0.f)                            // forward
	{
		gAgent.moveAt(1, false);
	}
	else if (inc > 0.f)                       // backward
	{
		gAgent.moveAt(-1, false);
	}
	*/

	static F32 previousPushInc = 0.f;		  // Smooth a little.

	if (inc != 0.f)
	{
		F32 push = -(inc + previousPushInc) / (2.f * gSavedSettings.getF32("JoystickRunThreshold"));

		if (mMovingNudges < 2)
		{
			gAgent.moveAtNudge(push < 0 ? -1 : 1);
			mMovingNudges += 1;
		}
		else
		{
			gAgent.moveAt(push);
		}
	}

	previousPushInc = inc;
	// </CV:David>
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentFly(F32 inc)
{
	// <CV:David>
	/*
	if (inc < 0.f)
	{
		if (! (gAgent.getFlying() ||
		       !gAgent.canFly() ||
		       gAgent.upGrabbed() ||
		       !gSavedSettings.getBOOL("AutomaticFly")) )
		{
			gAgent.setFlying(true);
		}
		gAgent.moveUp(1);
	}
	else if (inc > 0.f)
	{
		// crouch
		gAgent.moveUp(-1);
	}
	*/

	static F32 previousFlyInc = 0.f;		  // Smooth a little.

	if (inc != 0.f)
	{
		F32 fly = -(inc + previousFlyInc) / (2.f * gSavedSettings.getF32("JoystickRunThreshold"));

		if ((fly > 0.f) && (! (gAgent.getFlying() ||
		       !gAgent.canFly() ||
		       gAgent.upGrabbed() ||
		       !gSavedSettings.getBOOL("AutomaticFly")) ))
		{
			gAgent.setFlying(true);
		}

		gAgent.moveUp(fly);
	}

	previousFlyInc = inc;
	// </CV:David>
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentPitch(F32 pitch_inc)
{
	// <CV:David>
	if (gRift3DEnabled && gRiftMouseHorizontal)
	{
		return;
	}
	// </CV:David>

	if (pitch_inc < 0)
	{
		gAgent.setControlFlags(AGENT_CONTROL_PITCH_POS);
	}
	else if (pitch_inc > 0)
	{
		gAgent.setControlFlags(AGENT_CONTROL_PITCH_NEG);
	}
	
	gAgent.pitch(-pitch_inc);
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentYaw(F32 yaw_inc)
{	
	// Cannot steer some vehicles in mouselook if the script grabs the controls
	if (gAgentCamera.cameraMouselook() && !gSavedSettings.getBOOL("JoystickMouselookYaw"))
	{
		gAgent.rotate(-yaw_inc, gAgent.getReferenceUpVector());
	}
	else
	{
		if (yaw_inc < 0)
		{
			gAgent.setControlFlags(AGENT_CONTROL_YAW_POS);
		}
		else if (yaw_inc > 0)
		{
			gAgent.setControlFlags(AGENT_CONTROL_YAW_NEG);
		}

		gAgent.yaw(-yaw_inc);
	}
}

// -----------------------------------------------------------------------------
// <CV:David>
void LLViewerJoystick::cursorSlide(F32 inc)
{
	if (abs(inc) > 0.001)
	{
		static F32 previousInc = 0.f;  // Smooth a little.

		S32 x, y;
		LLUI::getMousePositionScreen(&x, &y);

		// Start out linear for fine control but then ramp up more quickly for faster movement.
		F32 nudge = inc > 0.f ? 1.f : -1.f;
		F32 linear = inc + previousInc;
		F32 square = 0.f;
		if (abs(linear) > 0.2f)
		{
			square = linear > 0.f ? linear - 0.2f : linear + 0.2f;
			square = square * abs(square);
		}

		S32 delta = (S32)(nudge + linear * 25.f + square * 300.f);

		x = llclamp(x + delta, 0, gViewerWindow->getWindowWidthRaw());
		LLUI::setMousePositionScreen(x, y);

		previousInc = inc;
	}
}
// </CV:David>

// -----------------------------------------------------------------------------
// <CV:David>
void LLViewerJoystick::cursorPush(F32 inc)
{
	if (abs(inc) > 0.001)
	{
		static F32 previousInc = 0.f;  // Smooth a little.

		S32 x, y;
		LLUI::getMousePositionScreen(&x, &y);

		// Start out linear for fine control but then ramp up more quickly for faster movement.
		F32 nudge = inc > 0.f ? 1.f : -1.f;
		F32 linear = inc + previousInc;
		F32 square = 0.f;
		if (abs(linear) > 0.2f)
		{
			square = linear > 0.f ? linear - 0.2f : linear + 0.2f;
			square = square * abs(square);
		}

		S32 delta = (S32)(nudge + linear * 25.f + square * 300.f);

		y = llclamp(y - delta, 0, gViewerWindow->getWindowHeightRaw());
		LLUI::setMousePositionScreen(x, y);

		previousInc = inc;
	}
}
// </CV:David>

// -----------------------------------------------------------------------------
// <CV:David>
void LLViewerJoystick::cursorZoom(F32 inc)
{
	if (abs(inc) > 0.001)
	{
		static U32 count = 0;

		count += 1;
		if (count > 2)  // Slow down the zoom in/out.
		{
			gViewerWindow->handleScrollWheel(inc > 0.f ? 1 : -1);
			count = 0;
		}
	}
}
// </CV:David>

// -----------------------------------------------------------------------------
void LLViewerJoystick::resetDeltas(S32 axis[])
{
	for (U32 i = 0; i < 6; i++)
	{
		sLastDelta[i] = -mAxes[axis[i]];
		sDelta[i] = 0.f;
	}

	sLastDelta[6] = sDelta[6] = 0.f;
	mResetFlag = false;
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::moveObjects(bool reset)
{
	static bool toggle_send_to_sim = false;

	if (!gFocusMgr.getAppHasFocus() || mDriverState != JDS_INITIALIZED
		|| !gSavedSettings.getBOOL("JoystickEnabled") || !gSavedSettings.getBOOL("JoystickBuildEnabled"))
	{
		return;
	}

	S32 axis[] = 
	{
		gSavedSettings.getS32("JoystickAxis0"),
		gSavedSettings.getS32("JoystickAxis1"),
		gSavedSettings.getS32("JoystickAxis2"),
		gSavedSettings.getS32("JoystickAxis3"),
		gSavedSettings.getS32("JoystickAxis4"),
		gSavedSettings.getS32("JoystickAxis5"),
	};

	if (reset || mResetFlag)
	{
		resetDeltas(axis);
		return;
	}

	F32 axis_scale[] =
	{
		gSavedSettings.getF32("BuildAxisScale0"),
		gSavedSettings.getF32("BuildAxisScale1"),
		gSavedSettings.getF32("BuildAxisScale2"),
		gSavedSettings.getF32("BuildAxisScale3"),
		gSavedSettings.getF32("BuildAxisScale4"),
		gSavedSettings.getF32("BuildAxisScale5"),
	};

	F32 dead_zone[] =
	{
		gSavedSettings.getF32("BuildAxisDeadZone0"),
		gSavedSettings.getF32("BuildAxisDeadZone1"),
		gSavedSettings.getF32("BuildAxisDeadZone2"),
		gSavedSettings.getF32("BuildAxisDeadZone3"),
		gSavedSettings.getF32("BuildAxisDeadZone4"),
		gSavedSettings.getF32("BuildAxisDeadZone5"),
	};

	F32 cur_delta[6];
	F32 time = gFrameIntervalSeconds;

	// avoid making ridicously big movements if there's a big drop in fps 
	if (time > .2f)
	{
		time = .2f;
	}

	// max feather is 32
	F32 feather = gSavedSettings.getF32("BuildFeathering"); 
	bool is_zero = true, absolute = gSavedSettings.getBOOL("Cursor3D");
	
	for (U32 i = 0; i < 6; i++)
	{
		cur_delta[i] = -mAxes[axis[i]];
		F32 tmp = cur_delta[i];
		if (absolute)
		{
			cur_delta[i] = cur_delta[i] - sLastDelta[i];
		}
		sLastDelta[i] = tmp;
		is_zero = is_zero && (cur_delta[i] == 0.f);
			
		if (cur_delta[i] > 0)
		{
			cur_delta[i] = llmax(cur_delta[i]-dead_zone[i], 0.f);
		}
		else
		{
			cur_delta[i] = llmin(cur_delta[i]+dead_zone[i], 0.f);
		}
		cur_delta[i] *= axis_scale[i];
		
		if (!absolute)
		{
			cur_delta[i] *= time;
		}

		sDelta[i] = sDelta[i] + (cur_delta[i]-sDelta[i])*time*feather;
	}

	U32 upd_type = UPD_NONE;
	LLVector3 v;
    
	if (!is_zero)
	{
		// Clear AFK state if moved beyond the deadzone
		if (gAwayTimer.getElapsedTimeF32() > LLAgent::MIN_AFK_TIME)
		{
			gAgent.clearAFK();
		}
		
		if (sDelta[0] || sDelta[1] || sDelta[2])
		{
			upd_type |= UPD_POSITION;
			v.setVec(sDelta[0], sDelta[1], sDelta[2]);
		}
		
		if (sDelta[3] || sDelta[4] || sDelta[5])
		{
			upd_type |= UPD_ROTATION;
		}
				
		// the selection update could fail, so we won't send 
		if (LLSelectMgr::getInstance()->selectionMove(v, sDelta[3],sDelta[4],sDelta[5], upd_type))
		{
			toggle_send_to_sim = true;
		}
	}
	else if (toggle_send_to_sim)
	{
		LLSelectMgr::getInstance()->sendSelectionMove();
		toggle_send_to_sim = false;
	}
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::moveAvatar(bool reset)
{
	if (!gFocusMgr.getAppHasFocus() || mDriverState != JDS_INITIALIZED
		|| !gSavedSettings.getBOOL("JoystickEnabled") || !gSavedSettings.getBOOL("JoystickAvatarEnabled"))
	{
		return;
	}

	S32 axis[] = 
	{
		// [1 0 2 4  3  5]
		// [Z X Y RZ RX RY]
		gSavedSettings.getS32("JoystickAxis0"),
		gSavedSettings.getS32("JoystickAxis1"),
		gSavedSettings.getS32("JoystickAxis2"),
		gSavedSettings.getS32("JoystickAxis3"),
		gSavedSettings.getS32("JoystickAxis4"),
		gSavedSettings.getS32("JoystickAxis5")
	};

	if (reset || mResetFlag)
	{
		resetDeltas(axis);
		if (reset)
		{
			// Note: moving the agent triggers agent camera mode;
			//  don't do this every time we set mResetFlag (e.g. because we gained focus)
			gAgent.moveAt(0, true);
		}
		return;
	}

	// <CV:David>
	if (!mNewSample)
	{
		// <CV:David>
		if (!mControlCursor)
		{
		// </CV:David>
			agentSlide(mCurrentMovement[X_I]);
			agentFly(mCurrentMovement[Y_I]);
			agentPush(mCurrentMovement[Z_I]);
			agentPitch(mCurrentMovement[RX_I]);
			agentYaw(mCurrentMovement[RY_I]);
		// <CV:David>
		}
		// </CV:David>
		return;
	}

	mNewSample = false;
	// </CV:David>

	bool is_zero = true;
	static bool button_held = false;

	// <CV:David>
	//if (mBtn[1] == 1)
	if ((mController != XBOX_CONTROLLER) && mBtn[1] == 1 || mController == XBOX_CONTROLLER && mBtn[XBOX_L_STICK_CLICK] == 1)
	// </CV:David>
	{
		// If AutomaticFly is enabled, then button1 merely causes a
		// jump (as the up/down axis already controls flying) if on the
		// ground, or cease flight if already flying.
		// If AutomaticFly is disabled, then button1 toggles flying.
		if (gSavedSettings.getBOOL("AutomaticFly"))
		{
			if (!gAgent.getFlying())
			{
				// <CV:David>
				//gAgent.moveUp(1);
				gAgent.moveJump();
				// </CV:David>
			}
			else if (!button_held)
			{
				button_held = true;
				gAgent.setFlying(FALSE);
			}
		}
		else if (!button_held)
		{
			button_held = true;
			gAgent.setFlying(!gAgent.getFlying());
		}

		is_zero = false;
	}
	else
	{
		button_held = false;
	}

	F32 axis_scale[] =
	{
		gSavedSettings.getF32("AvatarAxisScale0"),
		gSavedSettings.getF32("AvatarAxisScale1"),
		gSavedSettings.getF32("AvatarAxisScale2"),
		gSavedSettings.getF32("AvatarAxisScale3"),
		gSavedSettings.getF32("AvatarAxisScale4"),
		gSavedSettings.getF32("AvatarAxisScale5")
	};

	F32 dead_zone[] =
	{
		gSavedSettings.getF32("AvatarAxisDeadZone0"),
		gSavedSettings.getF32("AvatarAxisDeadZone1"),
		gSavedSettings.getF32("AvatarAxisDeadZone2"),
		gSavedSettings.getF32("AvatarAxisDeadZone3"),
		gSavedSettings.getF32("AvatarAxisDeadZone4"),
		gSavedSettings.getF32("AvatarAxisDeadZone5")
	};

	// time interval in seconds between this frame and the previous
	F32 time = gFrameIntervalSeconds;

	// avoid making ridicously big movements if there's a big drop in fps 
	if (time > .2f)
	{
		time = .2f;
	}

	// note: max feather is 32.0
	F32 feather = gSavedSettings.getF32("AvatarFeathering"); 
	
	F32 cur_delta[6];
	F32 val, dom_mov = 0.f;
	U32 dom_axis = Z_I;
#if LIB_NDOF
    bool absolute = (gSavedSettings.getBOOL("Cursor3D") && mNdofDev->absolute);
#else
    bool absolute = false;
#endif
	// remove dead zones and determine biggest movement on the joystick 
	for (U32 i = 0; i < 6; i++)
	{
		cur_delta[i] = -mAxes[axis[i]];
		if (absolute)
		{
			F32 tmp = cur_delta[i];
			cur_delta[i] = cur_delta[i] - sLastDelta[i];
			sLastDelta[i] = tmp;
		}

		if (cur_delta[i] > 0)
		{
			cur_delta[i] = llmax(cur_delta[i]-dead_zone[i], 0.f);
		}
		else
		{
			cur_delta[i] = llmin(cur_delta[i]+dead_zone[i], 0.f);
		}

		// we don't care about Roll (RZ) and Z is calculated after the loop
        if (i != Z_I && i != RZ_I)
		{
			// find out the axis with the biggest joystick motion
			val = fabs(cur_delta[i]);
			if (val > dom_mov)
			{
				dom_axis = i;
				dom_mov = val;
			}
		}
		
		is_zero = is_zero && (cur_delta[i] == 0.f);
	}

	// <CV:David>
	if (!is_zero && !mMoving)
	{
		mMovingNudges = 0;
	}
	mMoving = !is_zero;
	// </CV:David>

	if (!is_zero)
	{
		// Clear AFK state if moved beyond the deadzone
		if (gAwayTimer.getElapsedTimeF32() > LLAgent::MIN_AFK_TIME)
		{
			gAgent.clearAFK();
		}
		
		setCameraNeedsUpdate(true);
	}

	// forward|backward movements overrule the real dominant movement if 
	// they're bigger than its 20%. This is what you want 'cos moving forward
	// is what you do most. We also added a special (even more lenient) case 
	// for RX|RY to allow walking while pitching and turning
	if (fabs(cur_delta[Z_I]) > .2f * dom_mov
	    || ((dom_axis == RX_I || dom_axis == RY_I) 
		&& fabs(cur_delta[Z_I]) > .05f * dom_mov))
	{
		dom_axis = Z_I;
	}

	sDelta[X_I] = -cur_delta[X_I] * axis_scale[X_I];
	sDelta[Y_I] = -cur_delta[Y_I] * axis_scale[Y_I];
	sDelta[Z_I] = -cur_delta[Z_I] * axis_scale[Z_I];
	cur_delta[RX_I] *= -axis_scale[RX_I] * mPerfScale;
	cur_delta[RY_I] *= -axis_scale[RY_I] * mPerfScale;
		
	if (!absolute)
	{
		cur_delta[RX_I] *= time;
		cur_delta[RY_I] *= time;
	}
	sDelta[RX_I] += (cur_delta[RX_I] - sDelta[RX_I]) * time * feather;
	sDelta[RY_I] += (cur_delta[RY_I] - sDelta[RY_I]) * time * feather;
	
	handleRun((F32) sqrt(sDelta[Z_I]*sDelta[Z_I] + sDelta[X_I]*sDelta[X_I]));
	
	// Allow forward/backward movement some priority
	// <CV:David>
	//if (dom_axis == Z_I)
	if (dom_axis == Z_I && mController != XBOX_CONTROLLER)
	// </CV:David>
	{
		// <CV:David>
		memset(mCurrentMovement, 0, sizeof(mCurrentMovement));
		// </CV:David>

		agentPush(sDelta[Z_I]);			// forward/back
		// <CV:David>
		mCurrentMovement[Z_I] = sDelta[Z_I];
		// </CV:David>
		
		if (fabs(sDelta[X_I])  > .1f)
		{
			agentSlide(sDelta[X_I]);	// move sideways
			// <CV:David>
			mCurrentMovement[X_I] = sDelta[X_I];
			// </CV:David>
		}
		
		if (fabs(sDelta[Y_I])  > .1f)
		{
			agentFly(sDelta[Y_I]);		// up/down & crouch
			// <CV:David>
			mCurrentMovement[Y_I] = sDelta[Y_I];
			// </CV:David>
		}
	
		// too many rotations during walking can be confusing, so apply
		// the deadzones one more time (quick & dirty), at 50%|30% power
		F32 eff_rx = .3f * dead_zone[RX_I];
		F32 eff_ry = .3f * dead_zone[RY_I];
	
		if (sDelta[RX_I] > 0)
		{
			eff_rx = llmax(sDelta[RX_I] - eff_rx, 0.f);
		}
		else
		{
			eff_rx = llmin(sDelta[RX_I] + eff_rx, 0.f);
		}

		if (sDelta[RY_I] > 0)
		{
			eff_ry = llmax(sDelta[RY_I] - eff_ry, 0.f);
		}
		else
		{
			eff_ry = llmin(sDelta[RY_I] + eff_ry, 0.f);
		}
		
		
		if (fabs(eff_rx) > 0.f || fabs(eff_ry) > 0.f)
		{
			if (gAgent.getFlying())
			{
				agentPitch(eff_rx);
				agentYaw(eff_ry);
				// <CV:David>
				mCurrentMovement[RX_I] = eff_rx;
				mCurrentMovement[RY_I] = eff_ry;
				// </CV:David>
			}
			else
			{
				agentPitch(eff_rx);
				agentYaw(2.f * eff_ry);
				// <CV:David>
				mCurrentMovement[RX_I] = eff_rx;
				mCurrentMovement[RY_I] = 2.f * eff_ry;
				// </CV:David>
			}
		}
	}
	else
	{
		// <CV:David>
		mCurrentMovement[X_I] = sDelta[X_I];
		mCurrentMovement[Y_I] = sDelta[Y_I];
		mCurrentMovement[Z_I] = sDelta[Z_I];
		mCurrentMovement[RX_I] = sDelta[RX_I];
		mCurrentMovement[RY_I] = sDelta[RY_I];
		// </CV:David>

		agentSlide(sDelta[X_I]);		// move sideways
		agentFly(sDelta[Y_I]);			// up/down & crouch
		agentPush(sDelta[Z_I]);			// forward/back
		agentPitch(sDelta[RX_I]);		// pitch
		agentYaw(sDelta[RY_I]);			// turn
	}
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::moveFlycam(bool reset)
{
	static LLQuaternion 		sFlycamRotation;
	static LLVector3    		sFlycamPosition;
	static F32          		sFlycamZoom;
	
	// <CV:David>
	// Don't move flycam while controlling cursor but do maintain flycam camera positioning.
	if (!mControlCursor || reset)
	{
	// </CV:David>

		if (!gFocusMgr.getAppHasFocus() || mDriverState != JDS_INITIALIZED
			|| !gSavedSettings.getBOOL("JoystickEnabled") || !gSavedSettings.getBOOL("JoystickFlycamEnabled"))
		{
			return;
		}

		S32 axis[] = 
		{
			gSavedSettings.getS32("JoystickAxis0"),
			gSavedSettings.getS32("JoystickAxis1"),
			gSavedSettings.getS32("JoystickAxis2"),
			gSavedSettings.getS32("JoystickAxis3"),
			gSavedSettings.getS32("JoystickAxis4"),
			gSavedSettings.getS32("JoystickAxis5"),
			gSavedSettings.getS32("JoystickAxis6")
		};

		bool in_build_mode = LLToolMgr::getInstance()->inBuildMode();
		if (reset || mResetFlag)
		{
			sFlycamPosition = LLViewerCamera::getInstance()->getOrigin();
			sFlycamRotation = LLViewerCamera::getInstance()->getQuaternion();
			sFlycamZoom = LLViewerCamera::getInstance()->getView();
		
			resetDeltas(axis);

			return;
		}

		F32 axis_scale[] =
		{
			gSavedSettings.getF32("FlycamAxisScale0"),
			gSavedSettings.getF32("FlycamAxisScale1"),
			gSavedSettings.getF32("FlycamAxisScale2"),
			gSavedSettings.getF32("FlycamAxisScale3"),
			gSavedSettings.getF32("FlycamAxisScale4"),
			gSavedSettings.getF32("FlycamAxisScale5"),
			gSavedSettings.getF32("FlycamAxisScale6")
		};

		F32 dead_zone[] =
		{
			gSavedSettings.getF32("FlycamAxisDeadZone0"),
			gSavedSettings.getF32("FlycamAxisDeadZone1"),
			gSavedSettings.getF32("FlycamAxisDeadZone2"),
			gSavedSettings.getF32("FlycamAxisDeadZone3"),
			gSavedSettings.getF32("FlycamAxisDeadZone4"),
			gSavedSettings.getF32("FlycamAxisDeadZone5"),
			gSavedSettings.getF32("FlycamAxisDeadZone6")
		};

		F32 time = gFrameIntervalSeconds;

		// avoid making ridiculously big movements if there's a big drop in fps 
		if (time > .2f)
		{
			time = .2f;
		}

		F32 cur_delta[7];
		F32 feather = gSavedSettings.getF32("FlycamFeathering");
		bool absolute = gSavedSettings.getBOOL("Cursor3D");
		bool is_zero = true;

		for (U32 i = 0; i < 7; i++)
		{
			cur_delta[i] = -getJoystickAxis(axis[i]);


			F32 tmp = cur_delta[i];
			if (absolute)
			{
				cur_delta[i] = cur_delta[i] - sLastDelta[i];
			}
			sLastDelta[i] = tmp;

			if (cur_delta[i] > 0)
			{
				cur_delta[i] = llmax(cur_delta[i]-dead_zone[i], 0.f);
			}
			else
			{
				cur_delta[i] = llmin(cur_delta[i]+dead_zone[i], 0.f);
			}

			// We may want to scale camera movements up or down in build mode.
			// NOTE: this needs to remain after the deadzone calculation, otherwise
			// we have issues with flycam "jumping" when the build dialog is opened/closed  -Nyx
			if (in_build_mode)
			{
				if (i == X_I || i == Y_I || i == Z_I)
				{
					static LLCachedControl<F32> build_mode_scale(gSavedSettings,"FlycamBuildModeScale", 1.0);
					cur_delta[i] *= build_mode_scale;
				}
			}

			cur_delta[i] *= axis_scale[i];
		
			if (!absolute)
			{
				cur_delta[i] *= time;
			}

			sDelta[i] = sDelta[i] + (cur_delta[i]-sDelta[i])*time*feather;

			is_zero = is_zero && (cur_delta[i] == 0.f);

		}
	
		// Clear AFK state if moved beyond the deadzone
		if (!is_zero && gAwayTimer.getElapsedTimeF32() > LLAgent::MIN_AFK_TIME)
		{
			gAgent.clearAFK();
		}
	
		sFlycamPosition += LLVector3(sDelta) * sFlycamRotation;

		LLMatrix3 rot_mat(sDelta[3], sDelta[4], sDelta[5]);
		sFlycamRotation = LLQuaternion(rot_mat)*sFlycamRotation;

		if (gSavedSettings.getBOOL("AutoLeveling"))
		{
			LLMatrix3 level(sFlycamRotation);

			LLVector3 x = LLVector3(level.mMatrix[0]);
			LLVector3 y = LLVector3(level.mMatrix[1]);
			LLVector3 z = LLVector3(level.mMatrix[2]);

			y.mV[2] = 0.f;
			y.normVec();

			level.setRows(x,y,z);
			level.orthogonalize();
				
			LLQuaternion quat(level);
			sFlycamRotation = nlerp(llmin(feather*time,1.f), sFlycamRotation, quat);
		}

		if (gSavedSettings.getBOOL("ZoomDirect"))
		{
			sFlycamZoom = sLastDelta[6]*axis_scale[6]+dead_zone[6];
		}
		else
		{
			sFlycamZoom += sDelta[6];
		}

	// <CV:David>
	}
	// </CV:David>

	// <CV:David>
	//	LLMatrix3 mat(sFlycamRotation);
	LLMatrix3 mat;
	if (gRift3DEnabled)
	{
		LLQuaternion riftPitch = gAgentCamera.getRiftPitch();
		LLQuaternion riftRoll = gAgentCamera.getRiftRoll();
		LLQuaternion riftYaw = gAgentCamera.getRiftYaw();
		LLQuaternion riftRotation = riftPitch * riftRoll * riftYaw * sFlycamRotation;
		mat = LLMatrix3(riftRotation);
	}
	else
	{
		mat = LLMatrix3(sFlycamRotation);
	}
	// </CV:David>

	LLViewerCamera::getInstance()->setView(sFlycamZoom);
	LLViewerCamera::getInstance()->setOrigin(sFlycamPosition);
	LLViewerCamera::getInstance()->mXAxis = LLVector3(mat.mMatrix[0]);
	LLViewerCamera::getInstance()->mYAxis = LLVector3(mat.mMatrix[1]);
	LLViewerCamera::getInstance()->mZAxis = LLVector3(mat.mMatrix[2]);
}

// -----------------------------------------------------------------------------
// <CV:David>
// Based on moveAvatar().
void LLViewerJoystick::moveCursor()
{
	if (!gFocusMgr.getAppHasFocus() || mDriverState != JDS_INITIALIZED
		|| !gSavedSettings.getBOOL("JoystickEnabled") || !gSavedSettings.getBOOL("JoystickAvatarEnabled"))
	{
		return;
	}

	S32 axis[] = 
	{
		// [1 0 2 4  3  5]
		// [Z X Y RZ RX RY]
		gSavedSettings.getS32("JoystickAxis0"),
		gSavedSettings.getS32("JoystickAxis1"),
		gSavedSettings.getS32("JoystickAxis2"),
		gSavedSettings.getS32("JoystickAxis3"),
		gSavedSettings.getS32("JoystickAxis4"),
		gSavedSettings.getS32("JoystickAxis5")
	};

	if (!mNewSample)
	{
		return;
	}
	mNewSample = false;

	F32 axis_scale[] =
	{
		gSavedSettings.getF32("AvatarAxisScale0"),
		gSavedSettings.getF32("AvatarAxisScale1"),
		gSavedSettings.getF32("AvatarAxisScale2"),
		gSavedSettings.getF32("AvatarAxisScale3"),
		gSavedSettings.getF32("AvatarAxisScale4"),
		gSavedSettings.getF32("AvatarAxisScale5")
	};

	F32 dead_zone[] =
	{
		gSavedSettings.getF32("AvatarAxisDeadZone0"),
		gSavedSettings.getF32("AvatarAxisDeadZone1"),
		gSavedSettings.getF32("AvatarAxisDeadZone2"),
		gSavedSettings.getF32("AvatarAxisDeadZone3"),
		gSavedSettings.getF32("AvatarAxisDeadZone4"),
		gSavedSettings.getF32("AvatarAxisDeadZone5")
	};

	// time interval in seconds between this frame and the previous
	F32 time = gFrameIntervalSeconds;

	// avoid making ridicously big movements if there's a big drop in fps 
	if (time > .2f)
	{
		time = .2f;
	}

	// note: max feather is 32.0
	F32 feather = gSavedSettings.getF32("AvatarFeathering"); 
	
	F32 cur_delta[6];
#if LIB_NDOF
    bool absolute = (gSavedSettings.getBOOL("Cursor3D") && mNdofDev->absolute);
#else
    bool absolute = false;
#endif
	// remove dead zones and determine biggest movement on the joystick 
	for (U32 i = 0; i < 6; i++)
	{
		cur_delta[i] = -mAxes[axis[i]];
		if (absolute)
		{
			F32 tmp = cur_delta[i];
			cur_delta[i] = cur_delta[i] - sLastDelta[i];
			sLastDelta[i] = tmp;
		}

		if (cur_delta[i] > 0)
		{
			cur_delta[i] = llmax(cur_delta[i]-dead_zone[i], 0.f);
		}
		else
		{
			cur_delta[i] = llmin(cur_delta[i]+dead_zone[i], 0.f);
		}
	}

	sDelta[X_I] = -cur_delta[X_I] * axis_scale[X_I];
	sDelta[Y_I] = -cur_delta[Y_I] * axis_scale[Y_I];
	sDelta[Z_I] = -cur_delta[Z_I] * axis_scale[Z_I];
	cur_delta[RX_I] *= -axis_scale[RX_I] * mPerfScale;
	cur_delta[RY_I] *= -axis_scale[RY_I] * mPerfScale;
		
	if (!absolute)
	{
		cur_delta[RX_I] *= time;
		cur_delta[RY_I] *= time;
	}
	sDelta[RX_I] += (cur_delta[RX_I] - sDelta[RX_I]) * time * feather;
	sDelta[RY_I] += (cur_delta[RY_I] - sDelta[RY_I]) * time * feather;
	
	mCurrentMovement[X_I] = sDelta[X_I];
	mCurrentMovement[Y_I] = sDelta[Y_I];
	mCurrentMovement[Z_I] = sDelta[Z_I];
	mCurrentMovement[RX_I] = sDelta[RX_I];
	mCurrentMovement[RY_I] = sDelta[RY_I];

	cursorSlide(sDelta[X_I]);		// left / right
	cursorPush(sDelta[Z_I]);		// up / down
	cursorZoom(sDelta[RX_I]);		// mousewheel
}
// </CV:David>

// -----------------------------------------------------------------------------
bool LLViewerJoystick::toggleFlycam()
{
	if (!gSavedSettings.getBOOL("JoystickEnabled") || !gSavedSettings.getBOOL("JoystickFlycamEnabled"))
	{
		mOverrideCamera = false;
		LLPanelStandStopFlying::clearStandStopFlyingMode(LLPanelStandStopFlying::SSFM_FLYCAM);
		return false;
	}

	if (!mOverrideCamera)
	{
		gAgentCamera.changeCameraToDefault();
	}

	if (gAwayTimer.getElapsedTimeF32() > LLAgent::MIN_AFK_TIME)
	{
		gAgent.clearAFK();
	}
	
	mOverrideCamera = !mOverrideCamera;
	if (mOverrideCamera)
	{
		moveFlycam(true);
		LLPanelStandStopFlying::setStandStopFlyingMode(LLPanelStandStopFlying::SSFM_FLYCAM);
	}
	else 
	{
		// Exiting from the flycam mode: since we are going to keep the flycam POV for
		// the main camera until the avatar moves, we need to track this situation.
		// <CV:David> 
		// Need to reposition camera when exit flycam while in Riftlook. Makes no sense not to do the same other times.
		//setCameraNeedsUpdate(false);
		// Revert to first person view if in Riftlook.
		if (gRift3DEnabled)
		{
			gAgentCamera.changeCameraToMouselook();
		}
		// </CV:David>
		setNeedsReset(true);
		LLPanelStandStopFlying::clearStandStopFlyingMode(LLPanelStandStopFlying::SSFM_FLYCAM);
	}
	return true;
}

// <CV:David>
bool LLViewerJoystick::toggleCursor()
{
	mControlCursor = !mControlCursor;

	return true;
}
// </CV:David>

// <CV:David>
bool LLViewerJoystick::toggle3d()
{
	CVToggle3D::toggle3D();

	return true;
}
// </CV:David>

void LLViewerJoystick::scanJoystick()
{
	if (mDriverState != JDS_INITIALIZED || !gSavedSettings.getBOOL("JoystickEnabled"))
	{
		return;
	}

#if LL_WINDOWS
	// On windows, the flycam is updated syncronously with a timer, so there is
	// no need to update the status of the joystick here.
	// <CV:David>
	// But Do need to update joystick status if an Xbox controller.
	// if (!mOverrideCamera)
	if (!mOverrideCamera || mController == XBOX_CONTROLLER)
	// </CV:David>
#endif
	// <CV:David>
	//updateStatus();
	if (mSampleTimer.getElapsedTimeF32() > SAMPLE_TIME)
	{
		updateStatus();
		mNewSample = true;
		mSampleTimer.reset();
	}
	// </CV:David>


	// App focus check Needs to happen AFTER updateStatus in case the joystick
	// is not centred when the app loses focus.
	if (!gFocusMgr.getAppHasFocus())
	{
		return;
	}

	// <CV:David>
	//static long toggle_flycam = 0;

	// Guard against high FPS making buttons too sensitive.
	if (mNewSample)
	{
		static long toggle_3d = 0;
		static long toggle_cursor = 0;
		static long toggle_flycam_held = 0;
		static long toggle_cursor_held = 0;
		const long MIN_HOLD_COUNT = 3;

		static long toggle_flycam = 0;

		if (mController == XBOX_CONTROLLER)
		{

			// Special command keys ...
			if ((mBtn[XBOX_BACK_KEY] == 1) && (mBtn[XBOX_START_KEY] == 1))
			{
				// - Back + Start = toggle 3D
				if (!toggle_3d)
				{
					toggle_3d = toggle3d();
				}
			}
			else
			{
				toggle_3d = 0;

				// - Back = toggle flycam
				if (mBtn[XBOX_BACK_KEY] == 1)
				{
					if (toggle_flycam_held < MIN_HOLD_COUNT)
					{
						toggle_flycam_held += 1;
					}
					else if (!toggle_flycam)
					{
						toggle_flycam = toggleFlycam();
					}
				}
				else
				{
					toggle_flycam = 0;
					toggle_flycam_held = 0;
				}

				// - Start = toggle cursor/camera control
				if (mBtn[XBOX_START_KEY] == 1)
				{
					if (toggle_cursor_held < MIN_HOLD_COUNT)
					{
						toggle_cursor_held += 1;
					}
					else if (!toggle_cursor)
					{
						toggle_cursor = toggleCursor();
					}
				}
				else
				{
					toggle_cursor = 0;
					toggle_cursor_held = 0;
				}
			}

			// Toggle mouse/Rift-look ...
			static bool right_stick_click_down = false;
			if (mBtn[XBOX_R_STICK_CLICK] == 1 && !right_stick_click_down)
			{
				if (!gAgentCamera.cameraMouselook())
				{
					gAgentCamera.changeCameraToMouselook();
				}
				else
				{
					gAgentCamera.changeCameraToDefault();
				}
				right_stick_click_down = true;
			}
			else if (mBtn[XBOX_R_STICK_CLICK] == 0 && right_stick_click_down)
			{
				right_stick_click_down = false;
			}

			// Esc, Alt, Ctrl, Shift keys ...
			static bool esc_key_down = false;
			static bool alt_key_down = false;
			static bool ctrl_key_down = false;
			static bool shift_key_down = false;

			// Esc
			if (mBtn[XBOX_Y_KEY] == 1 && !esc_key_down)
			{
				gKeyboard->handleTranslatedKeyDown(KEY_ESCAPE, MASK_NONE);
				esc_key_down = true;
			}
			else if (mBtn[XBOX_Y_KEY] == 0 && esc_key_down)
			{
				gKeyboard->handleTranslatedKeyUp(KEY_ESCAPE, MASK_NONE);
				esc_key_down = false;
			}

			// Alt
			if (mBtn[XBOX_A_KEY] == 1 && !alt_key_down)
			{
				gKeyboard->setKeyDown(KEY_ALT, TRUE);
				alt_key_down = true;
			}
			else if (mBtn[XBOX_A_KEY] == 0 && alt_key_down)
			{
				gKeyboard->setKeyLevel(KEY_ALT, FALSE);
				gKeyboard->setKeyUp(KEY_ALT, TRUE);
				alt_key_down = false;
			}
			else if (alt_key_down)
			{
				gKeyboard->setKeyLevel(KEY_ALT, TRUE);
			}

			// Ctrl
			if (mBtn[XBOX_X_KEY] == 1 && !ctrl_key_down)
			{
				gKeyboard->setKeyDown(KEY_CONTROL, TRUE);
				ctrl_key_down = true;
			}
			else if (mBtn[XBOX_X_KEY] == 0 && ctrl_key_down)
			{
				gKeyboard->setKeyLevel(KEY_CONTROL, FALSE);
				gKeyboard->setKeyUp(KEY_CONTROL, TRUE);
				ctrl_key_down = false;
			}
			else if (ctrl_key_down)
			{
				gKeyboard->setKeyLevel(KEY_CONTROL, TRUE);
			}

			// Shift
			if (mBtn[XBOX_B_KEY] == 1 && !shift_key_down)
			{
				gKeyboard->setKeyDown(KEY_SHIFT, TRUE);
				shift_key_down = true;
			}
			else if (mBtn[XBOX_B_KEY] == 0 && shift_key_down)
			{
				gKeyboard->setKeyLevel(KEY_SHIFT, FALSE);
				gKeyboard->setKeyUp(KEY_SHIFT, TRUE);
				shift_key_down = false;
			}
			else if (shift_key_down)
			{
				gKeyboard->setKeyLevel(KEY_SHIFT, TRUE);
			}

			// Mouse clicks ...
			static long left_mouse_down = 0;
			static long right_mouse_down = 0;
			static long last_mouse_down = 0;

			S32 x, y;
			LLUI::getMousePositionScreen(&x, &y);
			LLCoordGL coord(x, y);
			MASK mask = gKeyboard->currentMask(TRUE);
			U32 left_mouse_button, right_mouse_button;

			if (gSavedSettings.getBOOL("JoystickSwapMouseButtons"))
			{
				left_mouse_button = XBOX_R_BUMP_KEY;
				right_mouse_button = XBOX_L_BUMP_KEY;
			}
			else
			{
				left_mouse_button = XBOX_L_BUMP_KEY;
				right_mouse_button = XBOX_R_BUMP_KEY;
			}

			if (mBtn[left_mouse_button] == 1 && left_mouse_down == 0)
			{
				gViewerWindow->handleMouseDown(gViewerWindow->getWindow(), coord, mask);
				left_mouse_down = 1;
				if (last_mouse_down == left_mouse_button && mDoubleClickTimer.getElapsedTimeF32() < DOUBLE_CLICK_INTERVAL)
				{
					gViewerWindow->handleDoubleClick(gViewerWindow->getWindow(), coord, mask);
				}
				else
				{
					last_mouse_down = left_mouse_button;
					mDoubleClickTimer.start();
				}
			}
			else if (mBtn[left_mouse_button] == 0 && left_mouse_down == 1)
			{
				gViewerWindow->handleMouseUp(gViewerWindow->getWindow(), coord, mask);
				left_mouse_down = 0;
			}

			if (mBtn[right_mouse_button] == 1 && right_mouse_down == 0)
			{
				gViewerWindow->handleRightMouseDown(gViewerWindow->getWindow(), coord, mask);
				right_mouse_down = 1;
				last_mouse_down = right_mouse_button;
			}
			else if (mBtn[right_mouse_button] == 0 && right_mouse_down == 1)
			{
				gViewerWindow->handleRightMouseUp(gViewerWindow->getWindow(), coord, mask);
				right_mouse_down = 0;
			}
		}
		else
		{
		// </CV:David>

			if (mBtn[0] == 1)
			{
				if (mBtn[0] != toggle_flycam)
				{
					toggle_flycam = toggleFlycam() ? 1 : 0;
				}
			}
			else
			{
				toggle_flycam = 0;
			}

		// <CV:David>
		}
	}
	// </CV:David>

	// <CV:David>
	//if (!mOverrideCamera && !(LLToolMgr::getInstance()->inBuildMode() && gSavedSettings.getBOOL("JoystickBuildEnabled")))
	if (!mOverrideCamera && !(LLToolMgr::getInstance()->inBuildMode() && gSavedSettings.getBOOL("JoystickBuildEnabled")) && !mControlCursor)
	// </CV:David>
	{
		moveAvatar();
	}

	// <CV:David>
	if (mControlCursor)
	{
		moveCursor();
	}
	// </CV:David>
}

// -----------------------------------------------------------------------------
// <CV:David>
//std::string LLViewerJoystick::getDescription()
std::string LLViewerJoystick::getDescription() const
// </CV:David>
{
	std::string res;
#if LIB_NDOF
	if (mDriverState == JDS_INITIALIZED && mNdofDev)
	{
		res = ll_safe_string(mNdofDev->product);
	}

	// <CV:David>
	// Tidy up description of Xbox controllers.
	res = boost::regex_replace(res, boost::regex("^Controller \\((.*)\\)$", boost::regex::perl), "$1");
	// </CV:David>
#endif
	return res;
}

// <CV:David>
std::string LLViewerJoystick::getDescriptionShort() const
{
	std::string res;
#if LIB_NDOF
	res = boost::regex_replace(getDescription(), boost::regex("^(\\S+(\\s+\\S+)?).*$", boost::regex::perl), "$1");
#endif
	return res;
}
// </CV:David>

bool LLViewerJoystick::isLikeSpaceNavigator() const
{
#if LIB_NDOF	
	return (isJoystickInitialized() 
			&& (strncmp(mNdofDev->product, "SpaceNavigator", 14) == 0
				|| strncmp(mNdofDev->product, "SpaceExplorer", 13) == 0
				|| strncmp(mNdofDev->product, "SpaceTraveler", 13) == 0
				|| strncmp(mNdofDev->product, "SpacePilot", 10) == 0));
#else
	return false;
#endif
}

// <CV:David>
bool LLViewerJoystick::isLikeXboxController() const
{
#if LIB_NDOF	
	return (isJoystickInitialized() && (getDescription().find("Xbox") == 0));
#else
	return false;
#endif
}
// </CV:David>

// -----------------------------------------------------------------------------
// <CV:David>
void LLViewerJoystick::setControllerDefaults()
{
	if (isLikeXboxController())
	{
		setXboxControllerDefaults();
	}
	else
	{
		// SpaceNavigator or other controller
		setSNDefaults();
	}
}
// </CV:David>

void LLViewerJoystick::setSNDefaults()
{
#if LL_DARWIN || LL_LINUX
	const float platformScale = 20.f;
	const float platformScaleAvXZ = 1.f;
	// The SpaceNavigator doesn't act as a 3D cursor on OS X / Linux. 
	const bool is_3d_cursor = false;
#else
	const float platformScale = 1.f;
	const float platformScaleAvXZ = 2.f;
	const bool is_3d_cursor = true;
#endif
	
	//gViewerWindow->alertXml("CacheWillClear");
	llinfos << "restoring SpaceNavigator defaults..." << llendl;
	
	gSavedSettings.setS32("JoystickAxis0", 1); // z (at)
	gSavedSettings.setS32("JoystickAxis1", 0); // x (slide)
	gSavedSettings.setS32("JoystickAxis2", 2); // y (up)
	// <CV:David> Comments label pitch and roll the wrong way around
	//gSavedSettings.setS32("JoystickAxis3", 4); // pitch
	//gSavedSettings.setS32("JoystickAxis4", 3); // roll 
	gSavedSettings.setS32("JoystickAxis3", 4); // roll
	gSavedSettings.setS32("JoystickAxis4", 3); // pitch 
	// <CV:David>
	gSavedSettings.setS32("JoystickAxis5", 5); // yaw
	gSavedSettings.setS32("JoystickAxis6", -1);
	
	gSavedSettings.setBOOL("Cursor3D", is_3d_cursor);
	gSavedSettings.setBOOL("AutoLeveling", true);
	gSavedSettings.setBOOL("ZoomDirect", false);
	
	// <CV:David>
	//gSavedSettings.setF32("AvatarAxisScale0", 1.f * platformScaleAvXZ);
	//gSavedSettings.setF32("AvatarAxisScale1", 1.f * platformScaleAvXZ);
	//gSavedSettings.setF32("AvatarAxisScale2", 1.f);
	gSavedSettings.setF32("AvatarAxisScale0", 1.25f * platformScaleAvXZ);
	gSavedSettings.setF32("AvatarAxisScale1", 1.25f * platformScaleAvXZ);
	gSavedSettings.setF32("AvatarAxisScale2", 2.5f * platformScale);
	// </CV:David>
	gSavedSettings.setF32("AvatarAxisScale4", .1f * platformScale);
	gSavedSettings.setF32("AvatarAxisScale5", .1f * platformScale);
	gSavedSettings.setF32("AvatarAxisScale3", 0.f * platformScale);
	gSavedSettings.setF32("BuildAxisScale1", .3f * platformScale);
	gSavedSettings.setF32("BuildAxisScale2", .3f * platformScale);
	gSavedSettings.setF32("BuildAxisScale0", .3f * platformScale);
	gSavedSettings.setF32("BuildAxisScale4", .3f * platformScale);
	gSavedSettings.setF32("BuildAxisScale5", .3f * platformScale);
	gSavedSettings.setF32("BuildAxisScale3", .3f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale1", 2.f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale2", 2.f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale0", 2.1f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale4", .1f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale5", .15f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale3", 0.f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale6", 0.f * platformScale);
	
	// <CV:David>
	//gSavedSettings.setF32("AvatarAxisDeadZone0", .1f);
	//gSavedSettings.setF32("AvatarAxisDeadZone1", .1f);
	//gSavedSettings.setF32("AvatarAxisDeadZone2", .1f);
	gSavedSettings.setF32("AvatarAxisDeadZone0", .01f);
	gSavedSettings.setF32("AvatarAxisDeadZone1", .01f);
	gSavedSettings.setF32("AvatarAxisDeadZone2", .02f);
	// </CV:David>
	gSavedSettings.setF32("AvatarAxisDeadZone3", 1.f);
	gSavedSettings.setF32("AvatarAxisDeadZone4", .02f);
	gSavedSettings.setF32("AvatarAxisDeadZone5", .01f);
	gSavedSettings.setF32("BuildAxisDeadZone0", .01f);
	gSavedSettings.setF32("BuildAxisDeadZone1", .01f);
	gSavedSettings.setF32("BuildAxisDeadZone2", .01f);
	gSavedSettings.setF32("BuildAxisDeadZone3", .01f);
	gSavedSettings.setF32("BuildAxisDeadZone4", .01f);
	gSavedSettings.setF32("BuildAxisDeadZone5", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone0", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone1", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone2", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone3", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone4", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone5", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone6", 1.f);
	
	gSavedSettings.setF32("AvatarFeathering", 6.f);
	gSavedSettings.setF32("BuildFeathering", 12.f);
	gSavedSettings.setF32("FlycamFeathering", 5.f);

	gSavedSettings.setBOOL("JoystickSwapMouseButtons", FALSE);  // <CV:David>
}

// <CV:David>
void LLViewerJoystick::setXboxControllerDefaults()
{
#if LL_DARWIN || LL_LINUX
	const float platformScale = 20.f;
	const float platformScaleAvXZ = 1.f;
#else
	const float platformScale = 1.f;
	const float platformScaleAvXZ = 2.f;
#endif

	//gViewerWindow->alertXml("CacheWillClear");
	llinfos << "Restoring Xbox Controller defaults..." << llendl;

	gSavedSettings.setS32("JoystickAxis0", 1); // z (at)
	gSavedSettings.setS32("JoystickAxis1", 0); // x (slide)
	gSavedSettings.setS32("JoystickAxis2", 2); // y (up)
	gSavedSettings.setS32("JoystickAxis3", -1); // roll
	gSavedSettings.setS32("JoystickAxis4", 4); // pitch 
	gSavedSettings.setS32("JoystickAxis5", 3); // yaw
	gSavedSettings.setS32("JoystickAxis6", -1);
	
	gSavedSettings.setBOOL("Cursor3D", false);
	gSavedSettings.setBOOL("AutoLeveling", true);
	gSavedSettings.setBOOL("ZoomDirect", false);
	
	gSavedSettings.setF32("AvatarAxisScale1", 0.2f * platformScaleAvXZ);
	gSavedSettings.setF32("AvatarAxisScale2", 0.4f * platformScale);
	gSavedSettings.setF32("AvatarAxisScale0", 0.2f * platformScaleAvXZ);
	gSavedSettings.setF32("AvatarAxisScale4", 1.0f * platformScale);
	gSavedSettings.setF32("AvatarAxisScale5", 2.0f * platformScale);
	gSavedSettings.setF32("AvatarAxisScale3", 1.0f * platformScale);
	gSavedSettings.setF32("BuildAxisScale1", 0.8f * platformScale);
	gSavedSettings.setF32("BuildAxisScale2", 0.8f * platformScale);
	gSavedSettings.setF32("BuildAxisScale0", 1.6f * platformScale);
	gSavedSettings.setF32("BuildAxisScale4", 1.0f * platformScale);
	gSavedSettings.setF32("BuildAxisScale5", 2.0f * platformScale);
	gSavedSettings.setF32("BuildAxisScale3", 1.0 * platformScale);
	gSavedSettings.setF32("FlycamAxisScale1", 12.f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale2", 16.f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale0", 20.f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale4", 1.f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale5", 2.f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale3", 1.f * platformScale);
	gSavedSettings.setF32("FlycamAxisScale6", 0.f * platformScale);
	
	gSavedSettings.setF32("AvatarAxisDeadZone1", .2f);
	gSavedSettings.setF32("AvatarAxisDeadZone2", .2f);
	gSavedSettings.setF32("AvatarAxisDeadZone0", .2f);
	gSavedSettings.setF32("AvatarAxisDeadZone4", .2f);
	gSavedSettings.setF32("AvatarAxisDeadZone5", .2f);
	gSavedSettings.setF32("AvatarAxisDeadZone3", .2f);
	gSavedSettings.setF32("BuildAxisDeadZone1", .1f);
	gSavedSettings.setF32("BuildAxisDeadZone2", .1f);
	gSavedSettings.setF32("BuildAxisDeadZone0", .2f);
	gSavedSettings.setF32("BuildAxisDeadZone4", .1f);
	gSavedSettings.setF32("BuildAxisDeadZone5", .1f);
	gSavedSettings.setF32("BuildAxisDeadZone3", .1f);
	gSavedSettings.setF32("FlycamAxisDeadZone1", .2f);
	gSavedSettings.setF32("FlycamAxisDeadZone2", .2f);
	gSavedSettings.setF32("FlycamAxisDeadZone0", .2f);
	gSavedSettings.setF32("FlycamAxisDeadZone4", .2f);
	gSavedSettings.setF32("FlycamAxisDeadZone5", .2f);
	gSavedSettings.setF32("FlycamAxisDeadZone3", .2f);
	gSavedSettings.setF32("FlycamAxisDeadZone6", 1.f);
	
	gSavedSettings.setF32("AvatarFeathering", 6.f);
	gSavedSettings.setF32("BuildFeathering", 12.f);
	gSavedSettings.setF32("FlycamFeathering", 10.f);

	gSavedSettings.setBOOL("JoystickSwapMouseButtons", TRUE);  // </CV:David>
}
// </CV:David>
