/** 
 * @file caskinectcontroller.h
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

#ifndef CAS_CASKINECTCONTROLLER_H
#define CAS_CASKINECTCONTROLLER_H

#if LL_WINDOWS

class CASKinectHandler;

class CASKinectController
{
public:
	CASKinectController(bool swap_fly_up_and_fly_down);
	~CASKinectController();

	bool kinectConfigured();
	void swapFlyUpAndFlyDown(bool);
	void scanKinect();

private:
	CASKinectHandler* mKinectHandler;
};

#endif  // LL_WINDOWS

#endif
