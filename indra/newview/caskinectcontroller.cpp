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

#include <ObjBase.h>
#include "NuiApi.h"


// Header ------------------------------------------------------------------------------------------
//
// The Kinect work is implemented in a class hidden from the rest of the viewer code because
// otherwise there is a clash between types used in the viewer code and the Kinect code plus
// required libraries (ObjBase.h).

class CASKinectHandler
{
public:

	CASKinectHandler();
	~CASKinectHandler();

	bool kinectConfigured()				{ return mKinectConfigured; }
	void processSkeletonFrame();

private:

	typedef HRESULT (WINAPI *NuiGetSensorCountType)(int*);
		NuiGetSensorCountType NuiGetSensorCountFunc;
	typedef HRESULT (WINAPI *NuiCreateSensorByIndexType)(int, INuiSensor**);
		NuiCreateSensorByIndexType NuiCreateSensorByIndexFunc;

	void loadKinectDLL();
	void unloadKinectDLL();

	int findClosestSkeleton(NUI_SKELETON_FRAME*);

	bool isStartGesture(NUI_SKELETON_DATA*);
	bool isStopGesture(NUI_SKELETON_DATA*);

	HMODULE		mKinectDLL;				// Dynamically loaded Kinect DLL.
	INuiSensor*	mKinectSensor;			// Kinect sensor that is being used.
	HANDLE		mSkeletonEvent;			// New skeleton data event.
	bool		mKinectConfigured;		// Has the Kinect been set up OK?
	bool		mControlling;			// Is the Kinect currently being used to control movement?
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

	mControlling = false;

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

void CASKinectHandler::processSkeletonFrame()
{
	NUI_SKELETON_FRAME skeletonFrame;
	NUI_SKELETON_DATA skeletonData;

	if (!mKinectConfigured || !WaitForSingleObject(mSkeletonEvent, 0) == WAIT_OBJECT_0)
	{
		return;
	}

	if (FAILED(mKinectSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame)))
	{
		return;
	}

	mKinectSensor->NuiTransformSmooth(&skeletonFrame, NULL);

	int skeleton = findClosestSkeleton(&skeletonFrame);
	if (skeleton == -1)
	{
		mControlling = false;
		return;
	}
	skeletonData = skeletonFrame.SkeletonData[skeleton];

	if (mControlling && isStopGesture(&skeletonData))
	{
		mControlling = false;
		return;
	}

	if (!mControlling && isStartGesture(&skeletonData))
	{
		mControlling = true;
		return;
	}
}


// Private -----------------------------------------------------------------------------------------

void CASKinectHandler::loadKinectDLL()
{	
	mKinectDLL = LoadLibrary(L"Kinect10");
	if (mKinectDLL != NULL)
	{
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

bool CASKinectHandler::isStartGesture(NUI_SKELETON_DATA* skeletonData)
{
	// Both hands above both elbows and both elbows above both shoulders
	// And both hands inside both shoulders
	// And both elbows outside both shoulders
	Vector4 leftHand = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT];
	Vector4 rightHand = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT];
	Vector4 leftElbow = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT];
	Vector4 rightElbow = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT];
	Vector4 leftShoulder = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT];
	Vector4 rightShoulder = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT];

	return (leftHand.y > leftElbow.y) && (rightHand.y > rightElbow.y)
		&& (leftElbow.y > leftShoulder.y) && (rightElbow.y > rightShoulder.y)
		&& (leftHand.x > leftShoulder.x) && (rightHand.x < rightShoulder.x)
		&& (leftElbow.x < leftShoulder.x) && (rightElbow.x > rightShoulder.x);
}

bool CASKinectHandler::isStopGesture(NUI_SKELETON_DATA* skeletonData)
{
	// Both elbows below shoulders and each wrist on opposite side of body
	Vector4 leftHand = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT];
	Vector4 rightHand = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT];
	Vector4 leftShoulder = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT];
	Vector4 rightShoulder = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT];
	Vector4 position = skeletonData->Position;

	return (leftHand.y < leftShoulder.y) && (rightHand.y < rightShoulder.y)
		&& (leftHand.x > position.x) && (rightHand.x < position.x);
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

void CASKinectController::processSkeletonFrame()
{
	mKinectHandler->processSkeletonFrame();
}

#endif  // LL_WINDOWS
