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

typedef HRESULT (WINAPI *NuiGetSensorCountType)(int*);
	NuiGetSensorCountType NuiGetSensorCountFunc;
typedef HRESULT (WINAPI *NuiCreateSensorByIndexType)(int, INuiSensor**);
	NuiCreateSensorByIndexType NuiCreateSensorByIndexFunc;

INuiSensor*		mKinectSensor;			// Kinect sensor that is being used.


// Public ----------------------------------------------------------------------

CASKinectController::CASKinectController()
{
	llinfos << "Kinect controller created" << llendl;
	loadKinectDLL();

	// Number of Kinect sensors.
	int numSensors = 0;
	NuiGetSensorCountFunc(&numSensors);
	llinfos << "Number of Kinect sensors = " << numSensors << llendl;

	// Find the first active Kinect sensor.
	mKinectSensor = NULL;
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

CASKinectController::~CASKinectController()
{
	if (mKinectSensor)
	{
		mKinectSensor->NuiShutdown();
		mKinectSensor->Release();
		mKinectSensor = NULL;
	}

	unloadKinectDLL();
	llinfos << "Kinect controller destroyed" << llendl;
}

bool CASKinectController::kinectConfigured()
{
	return (mKinectDLL != NULL);
}


// Private ---------------------------------------------------------------------

void CASKinectController::loadKinectDLL()
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

void CASKinectController::unloadKinectDLL()
{
	if (mKinectDLL)
	{
		NuiGetSensorCountFunc = NULL;
		NuiCreateSensorByIndexFunc = NULL;

		FreeLibrary(mKinectDLL);
		mKinectDLL = NULL;
	}
}

#endif  // LL_WINDOWS
