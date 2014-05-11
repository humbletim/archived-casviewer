/** 
 * @file llversioninfo.cpp
 * @brief Routines to access the viewer version and build information
 * @author Martin Reddy
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

#include "llviewerprecompiledheaders.h"
#include <iostream>
#include <sstream>
#include "llversioninfo.h"
#include "llviewercontrol.h"  // <CV:David>

// <FS:TS> Use configured file instead of compile time definitions to avoid
//         rebuilding the world with every Mercurial pull
#include "fsversionvalues.h"

#if ! defined(LL_VIEWER_CHANNEL)       \
 || ! defined(LL_VIEWER_VERSION_MAJOR) \
 || ! defined(LL_VIEWER_VERSION_MINOR) \
 || ! defined(LL_VIEWER_VERSION_PATCH) \
 || ! defined(LL_VIEWER_VERSION_BUILD)
 #error "Channel or Version information is undefined"
#endif

const char * const LL_CHANNEL = LL_VIEWER_CHANNEL;

//
// Set the version numbers in indra/VIEWER_VERSION
//

//static
S32 LLVersionInfo::getMajor()
{
	return LL_VIEWER_VERSION_MAJOR;
}

//static
S32 LLVersionInfo::getMinor()
{
	return LL_VIEWER_VERSION_MINOR;
}

//static
S32 LLVersionInfo::getPatch()
{
	return LL_VIEWER_VERSION_PATCH;
}

//static
S32 LLVersionInfo::getBuild()
{
	return LL_VIEWER_VERSION_BUILD;
}

//static
const std::string &LLVersionInfo::getVersion()
{
	static std::string version("");
	if (version.empty())
	{
		std::ostringstream stream;
		stream << LLVersionInfo::getShortVersion() << "." << LLVersionInfo::getBuild();
		// cache the version string
		version = stream.str();
	}
	return version;
}

//static
const std::string &LLVersionInfo::getShortVersion()
{
	static std::string short_version("");
	if(short_version.empty())
	{
		// cache the version string
		std::ostringstream stream;
		stream << LL_VIEWER_VERSION_MAJOR << "."
		       << LL_VIEWER_VERSION_MINOR << "."
		       << LL_VIEWER_VERSION_PATCH;
		short_version = stream.str();
	}
	return short_version;
}

namespace
{
	/// Storage of the channel name the viewer is using.
	//  The channel name is set by hardcoded constant, 
	//  or by calling LLVersionInfo::resetChannel()
	std::string sWorkingChannelName(LL_VIEWER_CHANNEL);

	// Storage for the "version and channel" string.
	// This will get reset too.
	std::string sVersionChannel("");

	//<FS:TS> Same as above, with version number in Firestorm FSDATA
	// format. Unlike the above, the channel name will always be the
	// hardcoded version.
	std::string sVersionChannelFS("");
}

//static
const std::string &LLVersionInfo::getChannelAndVersion()
{
	if (sVersionChannel.empty())
	{
		// cache the version string
		sVersionChannel = LLVersionInfo::getChannel() + " " + LLVersionInfo::getVersion();
	}

	return sVersionChannel;
}

//<FS:TS> Get version and channel in the format needed for FSDATA.
//static
const std::string &LLVersionInfo::getChannelAndVersionFS()
{
	if (sVersionChannelFS.empty())
	{
		// cache the version string
		std::ostringstream stream;
		stream << LL_VIEWER_CHANNEL << " "
		       << LL_VIEWER_VERSION_MAJOR << "."
		       << LL_VIEWER_VERSION_MINOR << "."
		       << LL_VIEWER_VERSION_PATCH << " ("
		       << LL_VIEWER_VERSION_BUILD << ")";
		sVersionChannelFS = stream.str();
	}

	return sVersionChannelFS;
}
//</FS:TS>

//static
const std::string &LLVersionInfo::getChannel()
{
	return sWorkingChannelName;
}

void LLVersionInfo::resetChannel(const std::string& channel)
{
	sWorkingChannelName = channel;
	sVersionChannel.clear(); // Reset version and channel string til next use.
}

// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2011-05-08 (Catznip-2.6.0a) | Added: Catznip-2.6.0a
const char* getBuildPlatformString()
{
#if LL_WINDOWS
	#ifndef _WIN64
			return "Win32";
	#else
			return "Win64";
	#endif // _WIN64
#elif LL_SDL
	#if LL_GNUC
		#if ( defined(__amd64__) || defined(__x86_64__) )
			return "Linux64";
		#else
			return "Linux32";
		#endif
	#endif
#elif LL_DARWIN
		#if ( defined(__amd64__) || defined(__x86_64__) )
			return "Darwin64";
		#else
			return "Darwin";
		#endif
#else
			return "Unknown";
#endif
}

const std::string& LLVersionInfo::getBuildPlatform()
{
	static std::string strPlatform = getBuildPlatformString();
	return strPlatform;
}
// [/SL:KB]

// <CV:David>
const std::string& LLVersionInfo::getBaseFirestormVersion()
{
	static std::string strShortFirestormVersion = gSavedSettings.getString("BaseFirestormVersion");
	return strShortFirestormVersion;
}
// </CV:David>
