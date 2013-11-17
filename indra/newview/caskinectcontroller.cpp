/** 
 * @file caskinectcontroller.cpp
 * @brief The CASKinectController class definitions
 *
 * $LicenseInfo:firstyear=2013&license=casviewerlgpl$
 * CtrlAltStudio Viewer Source Code
 * Copyright (C) 2013, David Rowe <david@ctrlaltstudio.com>
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
 * CtrlAltStudio
 * http://ctrlaltstudio.com
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#if LL_WINDOWS

#include "caskinectcontroller.h"
#include "llagent.h"
#include "llviewercontrol.h"

#include <ObjBase.h>
#include "NuiApi.h"


// Header ------------------------------------------------------------------------------------------
//
// The Kinect work is implemented in a class hidden from the rest of the viewer code because
// otherwise there is a clash between types used in the viewer code and the Kinect code plus
// required libraries (ObjBase.h).

const F32 NUDGE_TIME = 0.02f;		// In seconds.

class CASKinectHandler
{
public:

	CASKinectHandler();
	~CASKinectHandler();

	bool kinectConfigured()				{ return mKinectConfigured; }
	void scanKinect();

private:

	typedef HRESULT (WINAPI *NuiGetSensorCountType)(int*);
		NuiGetSensorCountType NuiGetSensorCountFunc;
	typedef HRESULT (WINAPI *NuiCreateSensorByIndexType)(int, INuiSensor**);
		NuiCreateSensorByIndexType NuiCreateSensorByIndexFunc;

	void loadKinectDLL();
	void unloadKinectDLL();

	bool getFileVersion(LPCWSTR file, int* major, int* minor, int* revision, int* build);

	int findClosestSkeleton(NUI_SKELETON_FRAME*);
	
	enum EKinectGesture
	{
		KG_NONE,
		KG_STOP_CONTROLLING,
		KG_START_CONTROLLING,
		KG_FLY_UP,
		KG_FLY_DOWN
	};
	
	EKinectGesture getGesture(NUI_SKELETON_DATA*);
	F32 getShoulderDelta(NUI_SKELETON_DATA*);
	F32 getMovingTime();
	void stopMoving();

	bool inline inRange(F32 val, F32 min, F32 max)	{ return min < val && val < max; }
	S32 inline sign(F32 val) { return (val >= 0) ? 1 : -1; }

	HMODULE		mKinectDLL;				// Dynamically loaded Kinect DLL.
	INuiSensor*	mKinectSensor;			// Kinect sensor that is being used.
	HANDLE		mSkeletonEvent;			// New skeleton data event.
	bool		mKinectConfigured;		// Has the Kinect been set up OK?
	bool		mControlling;			// Is the Kinect currently being used to control movement?
	LLFrameTimer	mMovingTimer;		// How long have been moving.
	bool		mWalking;				// Currently walking forwards or backwards.
	bool		mRunning;				// Currently running forwards.
	bool		mStrafing;				// Currently strafing left or right.
	U32			mFramesSkipped;			// Number of frames skipped. Stop controlling if gets too large.
	S32			mSensedGesture;			// Most recently sensed gesture.
	Vector4		mSensedPosition;		// Most recently sensed skeleton position.
	F32			mSensedShoulderDelta;	// Most recently sensed delta between shoulders.
	F32			mHysterisis;			// To prevent avatar "flickering" when changing back from walking, running, or strafing.
	Vector4		mZeroPosition;			// The home position of zero movement.
};


// Public ------------------------------------------------------------------------------------------

CASKinectHandler::CASKinectHandler()
{
	mKinectConfigured = false;

	llinfos << "Kinect controller created" << llendl;
	loadKinectDLL();

	// Number of Kinect sensors.
	int numSensors = 0;
	if (mKinectDLL)
	{
		NuiGetSensorCountFunc(&numSensors);
		llinfos << "Number of Kinect sensors = " << numSensors << llendl;
	}

	// Find the first active Kinect sensor.
	mKinectSensor = NULL;
	if (numSensors)
	{
		for (int i = 0; i < numSensors; i++)
		{
			INuiSensor* kinectSensor;
			if (SUCCEEDED(NuiCreateSensorByIndexFunc(i, &kinectSensor)))
			{
				if (SUCCEEDED(kinectSensor->NuiStatus()))
				{
					mKinectSensor = kinectSensor;
					break;
				}

				kinectSensor->Release();
			}
		}
	}

	// Set up to process skeleton data.
	if (mKinectSensor)
	{
		if (SUCCEEDED(mKinectSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_SKELETON)))
		{
			mSkeletonEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
			mKinectConfigured = SUCCEEDED(mKinectSensor->NuiSkeletonTrackingEnable(mSkeletonEvent, 0));
		}
	}

	mFramesSkipped = 0;
	mControlling = false;
	mWalking = false;
	mRunning = false;
	mStrafing = false;
	mHysterisis = 0.01f;

	llinfos << "Kinect sensor configured = " << mKinectConfigured << llendl;
}

CASKinectHandler::~CASKinectHandler()
{
	if (mKinectSensor)
	{
		mKinectSensor->NuiShutdown();

		if (mSkeletonEvent && (mSkeletonEvent != INVALID_HANDLE_VALUE))
		{
			CloseHandle(mSkeletonEvent);
		}

		mKinectSensor->Release();
		mKinectSensor = NULL;
	}

	unloadKinectDLL();
	llinfos << "Kinect controller destroyed" << llendl;
}

void CASKinectHandler::scanKinect()
{
	// Processes a new Kinect skeleton frame if there is one, otherwise continues controlling per previous frame.

	NUI_SKELETON_FRAME skeletonFrame;
	NUI_SKELETON_DATA skeletonData;

	if (!mKinectConfigured)
	{
		stopMoving();
		mControlling = false;
		return;
	}

	static U32 frames_skipped;
	if (WaitForSingleObject(mSkeletonEvent, 0) == WAIT_OBJECT_0 && SUCCEEDED(mKinectSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame)))
	{
		frames_skipped = 0;

		mKinectSensor->NuiTransformSmooth(&skeletonFrame, NULL);

		int skeleton = findClosestSkeleton(&skeletonFrame);
		if (skeleton == -1)
		{
			stopMoving();
			mControlling = false;
			return;
		}
		skeletonData = skeletonFrame.SkeletonData[skeleton];

		mSensedGesture = getGesture(&skeletonData);
		mSensedPosition = skeletonData.Position;
		mSensedShoulderDelta = getShoulderDelta(&skeletonData);

		switch (mSensedGesture)
		{
		case KG_STOP_CONTROLLING:
			if (mControlling)
			{
				stopMoving();
				mControlling = false;
				return;
			}
			break;
		case KG_START_CONTROLLING:
			mZeroPosition = mSensedPosition;
			mControlling = true;
			return;
		}
	}
	else
	{
		// Just in case.
		if (++frames_skipped > 25)
		{
			stopMoving();
			mControlling = false;
			return;
		}
	}

	if (mControlling)
	{
		switch (mSensedGesture)
		{
		case KG_FLY_UP:
			if (!gAgent.getFlying())
			{
				gAgent.setFlying(true);
			}
			gAgent.moveUp(1);
			break;
		case KG_FLY_DOWN:
			gAgent.moveUp(-1);
			break;
		}

		F32 sensitivity = (F32)gSavedSettings.getU32("KinectSensitivity");
		F32 positionDeadZone = 0.22f - 0.02f * sensitivity;
		F32 rotationDeadZone = positionDeadZone / 5.f;
		F32 walkMin = positionDeadZone / 2.f - (mWalking ? mHysterisis : 0.f);
		F32 walkMax = 1.5f * positionDeadZone - (mRunning ? mHysterisis : 0.f);
		F32 runMax = 2.5f * positionDeadZone;
		F32 strafeMin = positionDeadZone / 2.f - (mStrafing ? mHysterisis : 0.f);
		F32 strafeMax = 1.5f * positionDeadZone;
		F32 turnMin = rotationDeadZone;
		F32 turnMax = rotationDeadZone + 5.f * (10.f - sensitivity);

		// Move according to latest sensed position.
		F32 deltaX = mZeroPosition.x - mSensedPosition.x;
		F32 deltaZ = mZeroPosition.z - mSensedPosition.z;

		// Only move if within tolerance of zero position.
		if (inRange(deltaX, -strafeMax, strafeMax) && inRange(deltaZ, -walkMax, runMax))
		{
			F32 time = getMovingTime();

			// Forwards / backwards.
			if (inRange(deltaZ, walkMax, runMax))
			{
				mWalking = false;
				if (!mRunning)
				{
					gAgent.setTempRun();
					mRunning = true;
				}
				gAgent.moveAt(1);
			}
			else
			{
				if (mRunning)
				{
					gAgent.clearTempRun();
					mRunning = false;
				}

				if (inRange(abs(deltaZ), walkMin, walkMax))
				{
					mWalking = true;
					if (time < NUDGE_TIME)
					{
						gAgent.moveAtNudge(sign(deltaZ));  // Need an initial nudge to get moving at slowest walk speed.
					}
					else
					{
						gAgent.moveAt(sign(deltaZ) * (abs(deltaZ) - walkMin) / (walkMax - walkMin));
					}
				}
				else
				{
					mWalking = false;
				}
			}

			// Left / right.
			if (inRange(abs(deltaX), strafeMin, strafeMax))
			{
				mStrafing = true;
				if (time < NUDGE_TIME)
				{
					gAgent.moveLeftNudge(sign(deltaX));
				}
				else
				{
					gAgent.moveLeft(sign(deltaX) * (abs(deltaX) - strafeMin) / (strafeMax - strafeMin));
				}
			}
			else
			{
				mStrafing = false;
			}

			// Turn.
			if (inRange(abs(mSensedShoulderDelta), turnMin, turnMax))
			{
				gAgent.moveYaw(sign(mSensedShoulderDelta) * (5.f + sensitivity) * (abs(mSensedShoulderDelta) - rotationDeadZone));
			}
		}
		else
		{
			stopMoving();
		}
	}
}


// Private -----------------------------------------------------------------------------------------

void CASKinectHandler::loadKinectDLL()
{	
	LPCWSTR kinectLibrary = L"Kinect10";

	mKinectDLL = LoadLibrary(kinectLibrary);
	if (mKinectDLL != NULL)
	{
		int major, minor, revision, build;
		if (getFileVersion(kinectLibrary, &major, &minor, &revision, &build))
		{
			llinfos << "Kinect DLL version: " << major << "." << minor << "." << revision << "." << build << llendl;
		}
		
		NuiGetSensorCountFunc = (NuiGetSensorCountType)::GetProcAddress(mKinectDLL, "NuiGetSensorCount");
		NuiCreateSensorByIndexFunc = (NuiCreateSensorByIndexType)::GetProcAddress(mKinectDLL, "NuiCreateSensorByIndex");

		if (NuiGetSensorCountFunc == NULL ||
			NuiCreateSensorByIndexFunc == NULL)
		{
			llwarns << "Could not load Kinect library functions!" << llendl;
			unloadKinectDLL();
		}
	}
	else
	{
		llwarns << "Could not load Kinect library!" << llendl;
	}
}

void CASKinectHandler::unloadKinectDLL()
{
	if (mKinectDLL)
	{
		NuiGetSensorCountFunc = NULL;
		NuiCreateSensorByIndexFunc = NULL;

		FreeLibrary(mKinectDLL);
		mKinectDLL = NULL;
	}
}

bool CASKinectHandler::getFileVersion(LPCWSTR file, int* major, int* minor, int* revision, int* build)
{
	DWORD  verHandle = NULL;
	UINT   size      = 0;
	LPBYTE lpBuffer  = NULL;
	DWORD  verSize   = GetFileVersionInfoSize(file, &verHandle);

	if (verSize != NULL)
	{
		LPSTR verData = new char[verSize];

		if (GetFileVersionInfo(file, verHandle, verSize, verData))
		{

			if (VerQueryValue(verData, TEXT("\\"), (VOID FAR* FAR*)&lpBuffer, &size))
			{
				if (size)
				{
					VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
					if (verInfo->dwSignature == 0xfeef04bd)
					{
						#ifdef ND_BUILD64BIT_ARCH
							major = (verInfo->dwProductVersionMS >> 16) & 0xff;
							minor = (verInfo->dwProductVersionMS >> 0) & 0xff;
							revision = (verInfo->dwProductVersionLS >> 16) & 0xff;
							build = (verInfo->dwProductVersionLS >> 0) & 0xff;

						#else
							*major = HIWORD(verInfo->dwProductVersionMS);
							*minor = LOWORD(verInfo->dwProductVersionMS);
							*revision = HIWORD(verInfo->dwProductVersionLS);
							*build = LOWORD(verInfo->dwProductVersionLS);
						#endif
						return true;
					}
				}
			}

		}
		delete[] verData;
	}

	return false;
}

int CASKinectHandler::findClosestSkeleton(NUI_SKELETON_FRAME* skeletonFrame)
{
	int skeleton = -1;

	for (int i = 0; i < NUI_SKELETON_COUNT; i++)
	{
		if(skeletonFrame->SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED)
		{
			if (skeleton == -1)
			{
				skeleton = i;
			}
			else if (skeletonFrame->SkeletonData[i].Position.z < skeletonFrame->SkeletonData[skeleton].Position.z)
			{
				skeleton = i;
			}
		}
	}

	return skeleton;
}

CASKinectHandler::EKinectGesture CASKinectHandler::getGesture(NUI_SKELETON_DATA* skeletonData)
{
	Vector4 leftHand = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT];
	Vector4 rightHand = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT];
	Vector4 leftElbow = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT];
	Vector4 rightElbow = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT];
	Vector4 leftShoulder = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT];
	Vector4 rightShoulder = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT];
	Vector4 position = skeletonData->Position;
	const F32 flyDownMin = 0.2f - 0.01f * (F32)gSavedSettings.getU32("KinectSensitivity");

	EKinectGesture gesture = KG_NONE;

	// Stop controlling:
	// - Both elbows below shoulders and each wrist on opposite side of body.
	if ((leftHand.y < leftShoulder.y) && (rightHand.y < rightShoulder.y)
		&& (leftHand.x > position.x) && (rightHand.x < position.x))
	{
		gesture = KG_STOP_CONTROLLING;
	}
	// Start controlling:
	// - Both hands above both elbows and both elbows above both shoulders.
	// - Both hands inside both shoulders.
	// - Both elbows outside both shoulders.
	else if ((leftHand.y > leftElbow.y) && (rightHand.y > rightElbow.y)
		&& (leftElbow.y > leftShoulder.y) && (rightElbow.y > rightShoulder.y)
		&& (leftHand.x > leftShoulder.x) && (rightHand.x < rightShoulder.x)
		&& (leftElbow.x < leftShoulder.x) && (rightElbow.x > rightShoulder.x))
	{
		gesture = KG_START_CONTROLLING;
	}
	// Fly up:
	// - Both elbows outside both shoulders.
	// - Both hands outside both shoulders.
	// - Hands and elbows above shoulders;
	else if ((leftElbow.x < leftShoulder.x) && (rightElbow.x > rightShoulder.x)
		&& (leftHand.x < leftShoulder.x) && (rightHand.x > rightShoulder.x)
		&& (leftElbow.y > leftShoulder.y) && (rightElbow.y > rightShoulder.y)
		&& (leftHand.y > leftShoulder.y) && (rightHand.y > rightShoulder.y))
	{
		gesture = KG_FLY_UP;
	}
	// Fly down:
	// - Body position lowered.
	else if ((mZeroPosition.y - position.y) > flyDownMin)
	{
		gesture = KG_FLY_DOWN;
	}

	return gesture;
}

F32 CASKinectHandler::getShoulderDelta(NUI_SKELETON_DATA* skeletonData)
{
	NUI_SKELETON_POSITION_TRACKING_STATE stateShoulderLeft, stateShoulderRight;
	stateShoulderLeft = skeletonData->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_LEFT];
	stateShoulderRight = skeletonData->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_RIGHT];

	F32 deltaShoulder = 0.f;
	if (stateShoulderLeft != NUI_SKELETON_POSITION_NOT_TRACKED && stateShoulderLeft != NUI_SKELETON_POSITION_INFERRED
		&& stateShoulderRight != NUI_SKELETON_POSITION_NOT_TRACKED && stateShoulderRight != NUI_SKELETON_POSITION_INFERRED)
	{
		deltaShoulder = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].z - skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].z;
	}

	return deltaShoulder;
}

F32 CASKinectHandler::getMovingTime()
{
	if (mRunning || mWalking || mStrafing)
	{
		return mMovingTimer.getElapsedTimeF32();
	}
	else
	{
		mMovingTimer.reset();
		return 0.f;
	}
}

void CASKinectHandler::stopMoving()
{
	mWalking = false;
	if (mRunning)
	{
		gAgent.clearTempRun();
		mRunning = false;
	}
	mStrafing = false;
}


// Controller --------------------------------------------------------------------------------------

CASKinectController::CASKinectController()
{
	mKinectHandler = new CASKinectHandler();
}

CASKinectController::~CASKinectController()
{
	delete mKinectHandler;
	mKinectHandler = NULL;
}

bool CASKinectController::kinectConfigured()
{
	return mKinectHandler->kinectConfigured();
}

void CASKinectController::scanKinect()
{
	mKinectHandler->scanKinect();
}

#endif  // LL_WINDOWS
