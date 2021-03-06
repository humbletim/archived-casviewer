/** 
 * @file llviewerdisplay.h
 * @brief LLViewerDisplay class header file
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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

#ifndef LL_LLVIEWERDISPLAY_H
#define LL_LLVIEWERDISPLAY_H

class LLPostProcess;

void display_startup();
void display_cleanup();

void display(BOOL rebuild = TRUE, F32 zoom_factor = 1.f, int subfield = 0, BOOL for_snapshot = FALSE);

extern BOOL gDisplaySwapBuffers;
extern BOOL gDepthDirty;
extern BOOL	gTeleportDisplay;
extern LLFrameTimer	gTeleportDisplayTimer;
extern BOOL			gForceRenderLandFence;
extern BOOL gResizeScreenTexture;
extern BOOL gWindowResized;

// <FS:Ansariel> Draw Distance stepping; originally based on SpeedRez by Henri Beauchamp, licensed under LGPL
extern F32 gSavedDrawDistance;
extern F32 gLastDrawDistanceStep;

// <CV:David>
void setRiftSDKRendering(bool on);
extern U32 gOutputType;
const U32 OUTPUT_TYPE_NORMAL = 0;
const U32 OUTPUT_TYPE_STEREO = 1;
const U32 OUTPUT_TYPE_RIFT = 2;
extern BOOL gStereoscopic3DConfigured;
extern BOOL gStereoscopic3DEnabled;
extern BOOL gRift3DConfigured;
extern BOOL gRift3DEnabled;
extern BOOL gRiftInitialized;
extern unsigned gRiftHmdCaps;
const U32 RIFT_OPERATE_SEATED = 0;
const U32 RIFT_OPERATE_STANDING = 1;
extern BOOL gRiftStanding;
extern BOOL gRiftStrafe;
const U32 RIFT_HEAD_REORIENTS_AFTER_DEFAULT = 45;
const U32 RIFT_HEAD_REORIENTS_SPEED_DEFAULT = 5;
extern BOOL gRiftHeadReorients;
extern U32 gRiftHeadReorientsAfter;
extern U32 gRiftHeadReorientsSpeed;
const U32 RIFT_MOUSE_CURSOR = 0;
const U32 RIFT_MOUSE_CAMERA = 1;
extern BOOL gRiftMouseCursor;
extern BOOL gRiftMouseHorizontal;
extern S32 gRiftCurrentEye;
const U32 RENDER_NORMAL = 0;
const U32 RENDER_STEREO_LEFT = 1;
const U32 RENDER_STEREO_RIGHT = 2;
const U32 RENDER_RIFT_LEFT = 3;
const U32 RENDER_RIFT_RIGHT = 4;
// </CV:David>

#endif // LL_LLVIEWERDISPLAY_H
