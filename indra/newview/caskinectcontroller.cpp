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

// Public ----------------------------------------------------------------------

CASKinectController::CASKinectController()
{
	llinfos << "Kinect controller created" << llendl;
	loadKinectDLL();
}

CASKinectController::~CASKinectController()
{
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
		FreeLibrary(mKinectDLL);
		mKinectDLL = NULL;
	}
}

#endif  // LL_WINDOWS
