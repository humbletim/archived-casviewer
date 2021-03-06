/** 
 * @file llviewerdisplay.cpp
 * @brief LLViewerDisplay class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llviewerdisplay.h"

#include "llgl.h"
#include "llrender.h"
#include "llglheaders.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llviewercontrol.h"
#include "llcoord.h"
#include "llcriticaldamp.h"
#include "lldir.h"
#include "lldynamictexture.h"
#include "lldrawpoolalpha.h"
#include "llfeaturemanager.h"
//#include "llfirstuse.h"
#include "llhudmanager.h"
#include "llimagebmp.h"
#include "llmemory.h"
#include "llselectmgr.h"
#include "llsky.h"
#include "llstartup.h"
#include "lltoolfocus.h"
#include "lltoolmgr.h"
#include "lltooldraganddrop.h"
#include "lltoolpie.h"
#include "lltracker.h"
#include "lltrans.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvograss.h"
#include "llworld.h"
#include "pipeline.h"
#include "llspatialpartition.h"
#include "llappviewer.h"
#include "llstartup.h"
#include "llviewershadermgr.h"
#include "llfasttimer.h"
#include "llfloatertools.h"
#include "llviewertexturelist.h"
#include "llfocusmgr.h"
#include "llcubemap.h"
#include "llviewerregion.h"
#include "lldrawpoolwater.h"
#include "lldrawpoolbump.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"
#include "llpostprocess.h"
#include "llscenemonitor.h"
// [RLVa:KB] - Checked: 2011-05-22 (RLVa-1.3.1a)
#include "rlvhandler.h"
#include "rlvlocks.h"
// [/RLVa:KB]
#include "llviewermenu.h"  // <CV:David> DJRTODO: Temporarily needed for going fullscreen with DK2 in extended mode
#include "llnotificationsutil.h"  // <CV:David>

extern LLPointer<LLViewerTexture> gStartTexture;
extern bool gShiftFrame;

LLPointer<LLViewerTexture> gDisconnectedImagep = NULL;

// used to toggle renderer back on after teleport
BOOL		 gTeleportDisplay = FALSE;
LLFrameTimer gTeleportDisplayTimer;
LLFrameTimer gTeleportArrivalTimer;
const F32		RESTORE_GL_TIME = 5.f;	// Wait this long while reloading textures before we raise the curtain
// <FS:Ansariel> Draw Distance stepping; originally based on SpeedRez by Henri Beauchamp, licensed under LGPL
F32			gSavedDrawDistance = 0.0f;
F32			gLastDrawDistanceStep = 0.0f;

BOOL gForceRenderLandFence = FALSE;
BOOL gDisplaySwapBuffers = FALSE;
BOOL gDepthDirty = FALSE;
BOOL gResizeScreenTexture = FALSE;
BOOL gWindowResized = FALSE;
BOOL gSnapshot = FALSE;
BOOL gShaderProfileFrame = FALSE;
BOOL gRebuild = FALSE;  // <CV:David>

// This is how long the sim will try to teleport you before giving up.
const F32 TELEPORT_EXPIRY = 15.0f;
// Additional time (in seconds) to wait per attachment
const F32 TELEPORT_EXPIRY_PER_ATTACHMENT = 3.f;

U32 gRecentFrameCount = 0; // number of 'recent' frames
LLFrameTimer gRecentFPSTime;
LLFrameTimer gRecentMemoryTime;

// Rendering stuff
void pre_show_depth_buffer();
void post_show_depth_buffer();
void render_frame(U32 render_type);  // <CV:David>
void render_ui(F32 zoom_factor = 1.f, int subfield = 0);
void swap();
void render_hud_attachments();
void render_ui_3d();
void render_ui_2d();
void render_disconnected_background();

// <CV:David>
U32 gOutputType = OUTPUT_TYPE_NORMAL;
BOOL gStereoscopic3DEnabled = FALSE;
BOOL gStereoscopic3DConfigured = FALSE;
BOOL gRift3DEnabled = FALSE;
BOOL gRift3DConfigured = FALSE;
BOOL gRiftInitialized = FALSE;
unsigned gRiftHmdCaps = 0;
BOOL gRiftStanding = FALSE;
BOOL gRiftStrafe = FALSE;
BOOL gRiftHeadReorients = FALSE;
U32 gRiftHeadReorientsAfter = RIFT_HEAD_REORIENTS_AFTER_DEFAULT;
U32 gRiftHeadReorientsSpeed = RIFT_HEAD_REORIENTS_SPEED_DEFAULT;
BOOL gRiftMouseCursor = TRUE;
BOOL gRiftMouseHorizontal = FALSE;
S32 gRiftCurrentEye;  // 0 = left, 1 = right

void display_startup()
{
	if (   !gViewerWindow
		|| !gViewerWindow->getActive()
		|| !gViewerWindow->getWindow()->getVisible() 
		|| gViewerWindow->getWindow()->getMinimized() )
	{
		return; 
	}
	
	gPipeline.updateGL();

	// Update images?
	//gImageList.updateImages(0.01f);
	
	// Written as branch to appease GCC which doesn't like different
	// pointer types across ternary ops
	//
	if (!LLViewerFetchedTexture::sWhiteImagep.isNull())
	{
	LLTexUnit::sWhiteTexture = LLViewerFetchedTexture::sWhiteImagep->getTexName();
	}

	LLGLSDefault gls_default;

	// Required for HTML update in login screen
	static S32 frame_count = 0;

	LLGLState::checkStates();
	LLGLState::checkTextureChannels();

	if (frame_count++ > 1) // make sure we have rendered a frame first
	{
		LLViewerDynamicTexture::updateAllInstances();
	}

	LLGLState::checkStates();
	LLGLState::checkTextureChannels();

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	LLGLSUIDefault gls_ui;
	gPipeline.disableLights();

	if (gViewerWindow)
	gViewerWindow->setup2DRender();
	gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);

	gGL.color4f(1,1,1,1);
	if (gViewerWindow)
	gViewerWindow->draw();
	gGL.flush();

	LLVertexBuffer::unbind();

	LLGLState::checkStates();
	LLGLState::checkTextureChannels();

	if (gViewerWindow && gViewerWindow->getWindow())
	gViewerWindow->getWindow()->swapBuffers();

	glClear(GL_DEPTH_BUFFER_BIT);
}

static LLTrace::BlockTimerStatHandle FTM_UPDATE_CAMERA("Update Camera");

void display_update_camera()
{
	LL_RECORD_BLOCK_TIME(FTM_UPDATE_CAMERA);
	// TODO: cut draw distance down if customizing avatar?
	// TODO: cut draw distance on per-parcel basis?

	// Cut draw distance in half when customizing avatar,
	// but on the viewer only.
	F32 final_far = gAgentCamera.mDrawDistance;
	if (CAMERA_MODE_CUSTOMIZE_AVATAR == gAgentCamera.getCameraMode())
	{
		final_far *= 0.5f;
	}
// <FS:CR> Aurora sim
	if(LLWorld::getInstance()->getLockedDrawDistance())
	{
		//Reset the draw distance and do not update with the new val
		final_far = LLViewerCamera::getInstance()->getFar();
	}
// </FS:CR> Aurora sim
	LLViewerCamera::getInstance()->setFar(final_far);
	gViewerWindow->setup3DRender();
	
	// update all the sky/atmospheric/water settings
	LLWLParamManager::getInstance()->update(LLViewerCamera::getInstance());
	LLWaterParamManager::getInstance()->update(LLViewerCamera::getInstance());

	// Update land visibility too
	LLWorld::getInstance()->setLandFarClip(final_far);
}

// Write some stats to LL_INFOS()
void display_stats()
{
	// <FS:Ansariel> gSavedSettings replacement
	//F32 fps_log_freq = gSavedSettings.getF32("FPSLogFrequency");
	static LLCachedControl<F32> fpsLogFrequency(gSavedSettings, "FPSLogFrequency");
	F32 fps_log_freq = (F32)fpsLogFrequency;
	// </FS:Ansariel>
	if (fps_log_freq > 0.f && gRecentFPSTime.getElapsedTimeF32() >= fps_log_freq)
	{
		F32 fps = gRecentFrameCount / fps_log_freq;
		LL_INFOS() << llformat("FPS: %.02f", fps) << LL_ENDL;
		gRecentFrameCount = 0;
		gRecentFPSTime.reset();
	}
	// <FS:Ansariel> gSavedSettings replacement
	//F32 mem_log_freq = gSavedSettings.getF32("MemoryLogFrequency");
	static LLCachedControl<F32> memoryLogFrequency(gSavedSettings, "MemoryLogFrequency");
	F32 mem_log_freq = (F32)memoryLogFrequency;
	// </FS:Ansariel>
	if (mem_log_freq > 0.f && gRecentMemoryTime.getElapsedTimeF32() >= mem_log_freq)
	{
		gMemoryAllocated = (U64Bytes)LLMemory::getCurrentRSS();
		U32Megabytes memory = gMemoryAllocated;
		LL_INFOS() << llformat("MEMORY: %d MB", memory.value()) << LL_ENDL;
		LLMemory::logMemoryInfo(TRUE) ;
		gRecentMemoryTime.reset();
	}
}

static LLTrace::BlockTimerStatHandle FTM_PICK("Picking");
static LLTrace::BlockTimerStatHandle FTM_RENDER("Render");
static LLTrace::BlockTimerStatHandle FTM_UPDATE_SKY("Update Sky");
static LLTrace::BlockTimerStatHandle FTM_UPDATE_DYNAMIC_TEXTURES("Update Dynamic Textures");
static LLTrace::BlockTimerStatHandle FTM_IMAGE_UPDATE("Update Images");
static LLTrace::BlockTimerStatHandle FTM_IMAGE_UPDATE_CLASS("Class");
static LLTrace::BlockTimerStatHandle FTM_IMAGE_UPDATE_BUMP("Image Update Bump");
static LLTrace::BlockTimerStatHandle FTM_IMAGE_UPDATE_LIST("List");
static LLTrace::BlockTimerStatHandle FTM_IMAGE_UPDATE_DELETE("Delete");
static LLTrace::BlockTimerStatHandle FTM_RESIZE_WINDOW("Resize Window");
static LLTrace::BlockTimerStatHandle FTM_HUD_UPDATE("HUD Update");
static LLTrace::BlockTimerStatHandle FTM_DISPLAY_UPDATE_GEOM("Update Geom");
static LLTrace::BlockTimerStatHandle FTM_TEXTURE_UNBIND("Texture Unbind");
static LLTrace::BlockTimerStatHandle FTM_TELEPORT_DISPLAY("Teleport Display");
static LLTrace::BlockTimerStatHandle FTM_SWAP("Swap");

// Paint the display!
void display(BOOL rebuild, F32 zoom_factor, int subfield, BOOL for_snapshot)
{
	LL_RECORD_BLOCK_TIME(FTM_RENDER);

	if (gWindowResized)
	{ //skip render on frames where window has been resized
		LL_RECORD_BLOCK_TIME(FTM_RESIZE_WINDOW);
		gGL.flush();
		glClear(GL_COLOR_BUFFER_BIT);
		if (!gRift3DEnabled)
		{
			gViewerWindow->getWindow()->swapBuffers();
		}
		LLPipeline::refreshCachedSettings();
		gPipeline.resizeScreenTexture();
		gResizeScreenTexture = FALSE;
		gWindowResized = FALSE;
		return;
	}

	// <CV:David>
	// Save original snapshot setting for use in deciding whether or not to render in stereo.
	// Works around the hack that follows.
	BOOL output_for_snapshot = for_snapshot;
	// </CV:David>

	if (LLPipeline::sRenderDeferred)
	{ //hack to make sky show up in deferred snapshots
		for_snapshot = FALSE;
	}

	if (LLPipeline::sRenderFrameTest)
	{
		send_agent_pause();
	}

	gSnapshot = for_snapshot;
	gRebuild = rebuild;  // <CV:David>

	LLGLSDefault gls_default;
	LLGLDepthTest gls_depth(GL_TRUE, GL_TRUE, GL_LEQUAL);
	
	LLVertexBuffer::unbind();

	LLGLState::checkStates();
	LLGLState::checkTextureChannels();
	
	stop_glerror();

	gPipeline.disableLights();
	
	//reset vertex buffers if needed
	gPipeline.doResetVertexBuffers();

	stop_glerror();

	// Don't draw if the window is hidden or minimized.
	// In fact, must explicitly check the minimized state before drawing.
	// Attempting to draw into a minimized window causes a GL error. JC
	if (   !gViewerWindow->getActive()
		|| !gViewerWindow->getWindow()->getVisible() 
		|| gViewerWindow->getWindow()->getMinimized() )
	{
		// Clean up memory the pools may have allocated
		if (rebuild)
		{
			stop_glerror();
			gPipeline.rebuildPools();
			stop_glerror();
		}

		stop_glerror();
		gViewerWindow->returnEmptyPicks();
		stop_glerror();
		return; 
	}

	gViewerWindow->checkSettings();
	
	{
		LL_RECORD_BLOCK_TIME(FTM_PICK);
		LLAppViewer::instance()->pingMainloopTimeout("Display:Pick");
		gViewerWindow->performPick();
	}
	
	LLAppViewer::instance()->pingMainloopTimeout("Display:CheckStates");
	LLGLState::checkStates();
	LLGLState::checkTextureChannels();
	
	//////////////////////////////////////////////////////////
	//
	// Logic for forcing window updates if we're in drone mode.
	//

	// *TODO: Investigate running display() during gHeadlessClient.  See if this early exit is needed DK 2011-02-18
	if (gHeadlessClient) 
	{
#if LL_WINDOWS
		static F32 last_update_time = 0.f;
		if ((gFrameTimeSeconds - last_update_time) > 1.f)
		{
			InvalidateRect((HWND)gViewerWindow->getPlatformWindow(), NULL, FALSE);
			last_update_time = gFrameTimeSeconds;
		}
#elif LL_DARWIN
		// MBW -- Do something clever here.
#endif
		// Not actually rendering, don't bother.
		return;
	}


	//
	// Bail out if we're in the startup state and don't want to try to
	// render the world.
	//
	// <FS:Ansariel> Revert to original state to prevent flickering if login progress screen is disabled
	//if (LLStartUp::getStartupState() < STATE_PRECACHE)
	if (LLStartUp::getStartupState() < STATE_STARTED)
	// </FS:Ansariel>
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:Startup");
		display_startup();
		return;
	}


	if (gShaderProfileFrame)
	{
		LLGLSLShader::initProfile();
	}

	//LLGLState::verify(FALSE);

	/////////////////////////////////////////////////
	//
	// Update GL Texture statistics (used for discard logic?)
	//

	LLAppViewer::instance()->pingMainloopTimeout("Display:TextureStats");
	stop_glerror();

	LLImageGL::updateStats(gFrameTimeSeconds);
	
// <FS:CR> Aurora sim
	//LLVOAvatar::sRenderName = gSavedSettings.getS32("AvatarNameTagMode");
	static LLCachedControl<S32> avatarNameTagMode(gSavedSettings, "AvatarNameTagMode");
	S32 RenderName = (S32)avatarNameTagMode;

	if(RenderName > LLWorld::getInstance()->getAllowRenderName())//The most restricted gets set here
		RenderName = LLWorld::getInstance()->getAllowRenderName();
	LLVOAvatar::sRenderName = RenderName;
// <FS:CR> Aurora sim

	// <FS:Ansariel> gSavedSettings replacement
	//LLVOAvatar::sRenderGroupTitles = (gSavedSettings.getBOOL("NameTagShowGroupTitles") && gSavedSettings.getS32("AvatarNameTagMode"));
	static LLCachedControl<bool> nameTagShowGroupTitles(gSavedSettings, "NameTagShowGroupTitles");
	LLVOAvatar::sRenderGroupTitles = (nameTagShowGroupTitles && LLVOAvatar::sRenderName);
	// </FS:Ansariel>
	
	gPipeline.mBackfaceCull = TRUE;
	gFrameCount++;
	gRecentFrameCount++;
	if (gFocusMgr.getAppHasFocus())
	{
		gForegroundFrameCount++;
	}

	//////////////////////////////////////////////////////////
	//
	// Display start screen if we're teleporting, and skip render
	//

	if (gTeleportDisplay)
	{
		LL_RECORD_BLOCK_TIME(FTM_TELEPORT_DISPLAY);
		LLAppViewer::instance()->pingMainloopTimeout("Display:Teleport");
		static LLCachedControl<F32> teleport_arrival_delay(gSavedSettings, "TeleportArrivalDelay");
		static LLCachedControl<F32> teleport_local_delay(gSavedSettings, "TeleportLocalDelay");

		S32 attach_count = 0;
		if (isAgentAvatarValid())
		{
			attach_count = gAgentAvatarp->getAttachmentCount();
		}
		F32 teleport_save_time = TELEPORT_EXPIRY + TELEPORT_EXPIRY_PER_ATTACHMENT * attach_count;
		F32 teleport_elapsed = gTeleportDisplayTimer.getElapsedTimeF32();
		F32 teleport_percent = teleport_elapsed * (100.f / teleport_save_time);
		if( (gAgent.getTeleportState() != LLAgent::TELEPORT_START) && (teleport_percent > 100.f) )
		{
			// Give up.  Don't keep the UI locked forever.
			gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
			gAgent.setTeleportMessage(std::string());
		}

		const std::string& message = gAgent.getTeleportMessage();
		switch( gAgent.getTeleportState() )
		{
		case LLAgent::TELEPORT_PENDING:
			gTeleportDisplayTimer.reset();
			gViewerWindow->setShowProgress(TRUE,!gSavedSettings.getBOOL("FSDisableTeleportScreens"));
			gViewerWindow->setProgressPercent(llmin(teleport_percent, 0.0f));
			gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages["pending"]);
			gViewerWindow->setProgressString(LLAgent::sTeleportProgressMessages["pending"]);
			break;

		case LLAgent::TELEPORT_START:
			// Transition to REQUESTED.  Viewer has sent some kind
			// of TeleportRequest to the source simulator

			// Reset view angle if in mouselook. Fixes camera angle getting stuck on teleport. -Zi
			// <CV:David>
			//if(gAgentCamera.cameraMouselook())
			if (gAgentCamera.cameraMouselook() && !gRift3DEnabled)
			// <CV:David>
			{
				// If someone knows how to call "View.ZoomDefault" by hand, we should do that instead of
				// replicating the behavior here. -Zi
				LLViewerCamera::getInstance()->setDefaultFOV(DEFAULT_FIELD_OF_VIEW);
				if(gSavedSettings.getBOOL("FSResetCameraOnTP"))
				{
					gSavedSettings.setF32("CameraAngle", LLViewerCamera::getInstance()->getView()); // FS:LO Dont reset rightclick zoom when we teleport however. Fixes FIRE-6246.
				}
				// also, reset the marker for "currently zooming" in the mouselook zoom settings. -Zi
				LLVector3 vTemp=gSavedSettings.getVector3("_NACL_MLFovValues");
				vTemp.mV[2]=0.0f;
				gSavedSettings.setVector3("_NACL_MLFovValues",vTemp);
			}

			gTeleportDisplayTimer.reset();
			gViewerWindow->setShowProgress(TRUE,!gSavedSettings.getBOOL("FSDisableTeleportScreens"));
			gViewerWindow->setProgressPercent(llmin(teleport_percent, 0.0f));

			gAgent.setTeleportState( LLAgent::TELEPORT_REQUESTED );
			gAgent.setTeleportMessage(
				LLAgent::sTeleportProgressMessages["requesting"]);
			gViewerWindow->setProgressString(LLAgent::sTeleportProgressMessages["requesting"]);
			break;

		case LLAgent::TELEPORT_REQUESTED:
			// Waiting for source simulator to respond
			gViewerWindow->setProgressPercent( llmin(teleport_percent, 37.5f) );
			gViewerWindow->setProgressString(message);
			break;

		case LLAgent::TELEPORT_MOVING:
			// Viewer has received destination location from source simulator
			gViewerWindow->setProgressPercent( llmin(teleport_percent, 75.f) );
			gViewerWindow->setProgressString(message);
			break;

		case LLAgent::TELEPORT_START_ARRIVAL:
			// Transition to ARRIVING.  Viewer has received avatar update, etc., from destination simulator
			gTeleportArrivalTimer.reset();
				gViewerWindow->setProgressCancelButtonVisible(FALSE, LLTrans::getString("Cancel"));
			gViewerWindow->setProgressPercent(75.f);
			gAgent.setTeleportState( LLAgent::TELEPORT_ARRIVING );
			gAgent.setTeleportMessage(
				LLAgent::sTeleportProgressMessages["arriving"]);
			gTextureList.mForceResetTextureStats = TRUE;
			if(!gSavedSettings.getBOOL("FSDisableTeleportScreens"))
			{
				gAgentCamera.resetView(TRUE, TRUE);
			}
			break;

		case LLAgent::TELEPORT_ARRIVING:
			// Make the user wait while content "pre-caches"
			{
				F32 arrival_fraction = (gTeleportArrivalTimer.getElapsedTimeF32() / teleport_arrival_delay());
				if( arrival_fraction > 1.f || gSavedSettings.getBOOL("FSDisableTeleportScreens") )
				{
					arrival_fraction = 1.f;
					//LLFirstUse::useTeleport();
					gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
				}
				gViewerWindow->setProgressCancelButtonVisible(FALSE, LLTrans::getString("Cancel"));
				gViewerWindow->setProgressPercent(  arrival_fraction * 25.f + 75.f);
				gViewerWindow->setProgressString(message);
			}
			break;

		case LLAgent::TELEPORT_LOCAL:
			// Short delay when teleporting in the same sim (progress screen active but not shown - did not
			// fall-through from TELEPORT_START)
			{
				// <FS:CR> FIRE-8721 - Remove local teleport delay
				//if( gTeleportDisplayTimer.getElapsedTimeF32() > teleport_local_delay() )
				// </FS:CR>
				{
					//LLFirstUse::useTeleport();
					gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
				}
			}
			break;

		case LLAgent::TELEPORT_NONE:
			// No teleport in progress
			gViewerWindow->setShowProgress(FALSE,FALSE);
			gTeleportDisplay = FALSE;
			break;
		}
	}
    else if(LLAppViewer::instance()->logoutRequestSent())
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:Logout");
		F32 percent_done = gLogoutTimer.getElapsedTimeF32() * 100.f / gLogoutMaxTime;
		if (percent_done > 100.f)
		{
			percent_done = 100.f;
		}

		if( LLApp::isExiting() )
		{
			percent_done = 100.f;
		}
		
		gViewerWindow->setProgressPercent( percent_done );
	}
	else
	if (gRestoreGL)
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:RestoreGL");
		F32 percent_done = gRestoreGLTimer.getElapsedTimeF32() * 100.f / RESTORE_GL_TIME;
		if( percent_done > 100.f )
		{
			gViewerWindow->setShowProgress(FALSE,FALSE);
			gRestoreGL = FALSE;
		}
		else
		{

			if( LLApp::isExiting() )
			{
				percent_done = 100.f;
			}
			
			gViewerWindow->setProgressPercent( percent_done );
		}
	}

	// <FS::Ansariel> Draw Distance stepping; originally based on SpeedRez by Henri Beauchamp, licensed under LGPL
	// Progressively increase draw distance after TP when required.
	static LLCachedControl<F32> renderFarClip(gSavedSettings, "RenderFarClip");
	if (gSavedDrawDistance > 0.0f && gAgent.getTeleportState() == LLAgent::TELEPORT_NONE)
	{
		if (gLastDrawDistanceStep != (F32)renderFarClip)
		{
			gSavedDrawDistance = 0.0f;
			gLastDrawDistanceStep = 0.0f;
			gSavedSettings.setF32("FSSavedRenderFarClip", 0.0f);
		}

		if (gTeleportArrivalTimer.getElapsedTimeF32() >=
			(F32)gSavedSettings.getU32("FSRenderFarClipSteppingInterval"))
		{
			gTeleportArrivalTimer.reset();
			F32 current = gSavedSettings.getF32("RenderFarClip");
			if (gSavedDrawDistance > current)
			{
				current *= 2.0;
				if (current > gSavedDrawDistance)
				{
					current = gSavedDrawDistance;
				}
				gSavedSettings.setF32("RenderFarClip", current);
				gLastDrawDistanceStep = current;
			}
			if (current >= gSavedDrawDistance)
			{
				gSavedDrawDistance = 0.0f;
				gLastDrawDistanceStep = 0.0f;
				gSavedSettings.setF32("FSSavedRenderFarClip", 0.0f);
			}
		}
	}
	// </FS::Ansariel>

	//////////////////////////
	//
	// Prepare for the next frame
	//

	/////////////////////////////
	//
	// Update the camera
	//
	//

	LLAppViewer::instance()->pingMainloopTimeout("Display:Camera");
	LLViewerCamera::getInstance()->setZoomParameters(zoom_factor, subfield);
	LLViewerCamera::getInstance()->setNear(MIN_NEAR_PLANE);

	//////////////////////////
	//
	// clear the next buffer
	// (must follow dynamic texture writing since that uses the frame buffer)
	//

	if (gDisconnected)
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:Disconnected");
		render_ui();
		swap();
	}
	
	//////////////////////////
	//
	// Set rendering options
	//
	//
	LLAppViewer::instance()->pingMainloopTimeout("Display:RenderSetup");
	stop_glerror();

	///////////////////////////////////////
	//
	// Slam lighting parameters back to our defaults.
	// Note that these are not the same as GL defaults...

	stop_glerror();
	gGL.setAmbientLightColor(LLColor4::white);
	stop_glerror();
			
	/////////////////////////////////////
	//
	// Render
	//
	// Actually push all of our triangles to the screen.
	//

	// do render-to-texture stuff here
	if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_DYNAMIC_TEXTURES))
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:DynamicTextures");
		LL_RECORD_BLOCK_TIME(FTM_UPDATE_DYNAMIC_TEXTURES);
		if (LLViewerDynamicTexture::updateAllInstances())
		{
			gGL.setColorMask(true, true);
			glClear(GL_DEPTH_BUFFER_BIT);
		}
	}

	gViewerWindow->setup3DViewport();

	gPipeline.resetFrameStats();	// Reset per-frame statistics.
	
	if (!gDisconnected)
	{

		// <CV:David>
		if (gRift3DEnabled)
		{
			gAgentCamera.calcRiftValues();  // DJRTODO: Do separately for each eye?
		}

		if ((gOutputType == OUTPUT_TYPE_NORMAL) 
			|| (gOutputType == OUTPUT_TYPE_STEREO && !gStereoscopic3DEnabled) 
			|| (gOutputType == OUTPUT_TYPE_RIFT && !gRift3DEnabled) 
			|| output_for_snapshot)
		{
			LLViewerCamera::getInstance()->calcMonoValues();

			render_frame(RENDER_NORMAL);

			LLAppViewer::instance()->pingMainloopTimeout("Display:RenderUI");
			if (!gSnapshot)
			{
				render_ui();
			}
			LLSpatialGroup::sNoDelete = FALSE;
		}
		else if (gOutputType == OUTPUT_TYPE_RIFT) // && gRift3DEnabled && !output_for_snapshot
		{
			LLViewerCamera::getInstance()->calcStereoValues();
			int curIndex;
			GLuint curTexId;
			ovrEyeType eye;

			// First (left) eye ...
			eye = ovrEyeType(0);
			gRiftCurrentEye = eye;

			ovr_GetTextureSwapChainCurrentIndex(gRiftSession, gRiftTextureSwapChain[eye], &curIndex);
			ovr_GetTextureSwapChainBufferGL(gRiftSession, gRiftTextureSwapChain[eye], curIndex, &curTexId);
			glBindFramebuffer(GL_FRAMEBUFFER, gRiftTextureBuffers[eye]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curTexId, 0);
			//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gRiftDepthBuffers[eye], 0);
			glViewport(0, 0, gRiftHSample, gRiftVSample);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_FRAMEBUFFER_SRGB);
			
			render_frame(RENDER_RIFT_LEFT);
			LLAppViewer::instance()->pingMainloopTimeout("Display:RenderUILeftEye");
			render_ui();
			LLSpatialGroup::sNoDelete = FALSE;

			ovr_CommitTextureSwapChain(gRiftSession, gRiftTextureSwapChain[eye]);

			// Second (right) eye ...
			eye = ovrEyeType(1);
			gRiftCurrentEye = eye;

			ovr_GetTextureSwapChainCurrentIndex(gRiftSession, gRiftTextureSwapChain[eye], &curIndex);
			ovr_GetTextureSwapChainBufferGL(gRiftSession, gRiftTextureSwapChain[eye], curIndex, &curTexId);
			glBindFramebuffer(GL_FRAMEBUFFER, gRiftTextureBuffers[eye]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curTexId, 0);
			//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gRiftDepthBuffers[eye], 0);
			glViewport(0, 0, gRiftHSample, gRiftVSample);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_FRAMEBUFFER_SRGB);

			render_frame(RENDER_RIFT_RIGHT);
			LLAppViewer::instance()->pingMainloopTimeout("Display:RenderUIRightEye");
			render_ui();
			LLSpatialGroup::sNoDelete = FALSE;

			ovr_CommitTextureSwapChain(gRiftSession, gRiftTextureSwapChain[eye]);
			
			// Submit frame with single layer we're using ...
			ovrLayerHeader* layers = &gRiftLayer.Header;
			ovrResult result = ovr_SubmitFrame(gRiftSession, 0, nullptr, &layers, 1);
			if (result != ovrSuccess) {
				// DJRTODO: Switch out of Riftlook?
				LL_INFOS() << "Oculus Rift: ovrHmd_SubmitFrame() failed!" << LL_ENDL;
			}

			// Mirror Rift display to application window ...
			static LLCachedControl<bool> riftMirrorToDesktop(gSavedSettings, "RiftMirrorToDesktop");
			if (riftMirrorToDesktop && gRiftHaveMirrorTexture)
			{
				glBindFramebuffer(GL_READ_FRAMEBUFFER, gRiftMirrorFBO);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
				GLint w = gRiftMirrorTextureDesc.Width;
				GLint h = gRiftMirrorTextureDesc.Height;
				glBlitFramebuffer(0, h, w, 0, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			}
			else
			{
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			}
		}
		else // gOutputType == OUTPUT_TYPE_STEREO && gStereoscopic3DEnabled && !output_for_snapshot
		{
			// Stereoscopic 3D references:
			// - http://nvidia.asia/content/asia/event/siggraph-asia-2010/presos/Gateau_Stereoscopy.pdf
			// - http://www.forejune.com/stereo/

			LLViewerCamera::getInstance()->calcStereoValues();

			// Left eye ...
			glDrawBuffer(GL_BACK_LEFT);
			glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
			render_frame(RENDER_STEREO_LEFT);
			LLAppViewer::instance()->pingMainloopTimeout("Display:RenderUILeftEye");
			render_ui();  // Note: UI rendering code entwines 2D and 3D to easiest just to render 2D twice, once for each eye.
			LLSpatialGroup::sNoDelete = FALSE;

			// Right eye ...
			glDrawBuffer(GL_BACK_RIGHT);
			glClear( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
			render_frame(RENDER_STEREO_RIGHT);
			LLAppViewer::instance()->pingMainloopTimeout("Display:RenderUIRightEye");
			render_ui();
			LLSpatialGroup::sNoDelete = FALSE;

			glDrawBuffer(GL_BACK);  // Needed so that snapshot on exit doesn't include UI.
		}

		if (!gSnapshot)
		{
			swap();
		}

		//LLSpatialGroup::sNoDelete = FALSE;
		// </CV:David>
		
		// <	FS:ND>FIRE-9943; resizeScreenTexture will try to disable deferred mode in low memory situations.
		// Depending	 on the state of the pipeline. this can trigger illegal deletion of drawables.
		// To work around th	at, resizeScreenTexture will just set a flag, which then later does trigger the change
		// in shaders.
		// RenderDeffered will be shut down here if needed.
		gPipeline.disableDeferredOnLowMemory();
		// </FS:ND>

		gPipeline.clearReferences();

		gPipeline.rebuildGroups();
	}

	LLAppViewer::instance()->pingMainloopTimeout("Display:FrameStats");
	
	stop_glerror();

	if (LLPipeline::sRenderFrameTest)
	{
		send_agent_resume();
		LLPipeline::sRenderFrameTest = FALSE;
	}

	display_stats();
				
	LLAppViewer::instance()->pingMainloopTimeout("Display:Done");

	gShiftFrame = false;

	if (gShaderProfileFrame)
	{
		gShaderProfileFrame = FALSE;
		LLGLSLShader::finishProfile();
	}
	
	// <CV:David>
	// Temporary coded needed when going fullscreen when toggling into Riftlook so that Rift driver picks up full screen resolution.
	if (gDoSetRiftlook) {
		CVToggle3D::setRiftlook(gDoSetRiftlookValue);
	}
	gDoSetRiftlook = false;
	// </CV:David>
}

void render_frame(U32 render_type)  // <CV:David> Frame rendering refactored for use in stereoscopic 3D.
{

	// <CV:David> Collect objects in the stereoscopic cull frustum rather than each eye's asymmetric camera frustum.
	if (render_type == RENDER_STEREO_LEFT || render_type == RENDER_STEREO_RIGHT || render_type == RENDER_RIFT_LEFT || render_type == RENDER_RIFT_RIGHT)
	{
		LLViewerCamera::getInstance()->moveToStereoCullFrustum();
	}
	else if (((gOutputType == OUTPUT_TYPE_STEREO) && gStereoscopic3DEnabled) || ((gOutputType == OUTPUT_TYPE_RIFT) && gRift3DEnabled))
	{
		// For snapshots and such.
		LLViewerCamera::getInstance()->moveToCenter();
	}
	// </CV:David>

	LLAppViewer::instance()->pingMainloopTimeout("Display:Update");
	if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
	{ //don't draw hud objects in this frame
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
	}

	if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES))
	{ //don't draw hud particles in this frame
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
	}

	//upkeep gl name pools
	LLGLNamePool::upkeepPools();
		
	stop_glerror();
	display_update_camera();
	stop_glerror();
				
	// *TODO: merge these two methods
	{
		LL_RECORD_BLOCK_TIME(FTM_HUD_UPDATE);
		LLHUDManager::getInstance()->updateEffects();
		LLHUDObject::updateAll();
		stop_glerror();
	}

	{
		LL_RECORD_BLOCK_TIME(FTM_DISPLAY_UPDATE_GEOM);
		const F32 max_geom_update_time = 0.005f*10.f*gFrameIntervalSeconds.value(); // 50 ms/second update time
		gPipeline.createObjects(max_geom_update_time);
		gPipeline.processPartitionQ();
		gPipeline.updateGeom(max_geom_update_time);
		stop_glerror();
	}

	gPipeline.updateGL();
		
	stop_glerror();

	S32 water_clip = 0;
	if ((LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_ENVIRONMENT) > 1) &&
			(gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_WATER) || 
			gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_VOIDWATER)))
	{
		if (LLViewerCamera::getInstance()->cameraUnderWater())
		{
			water_clip = -1;
		}
		else
		{
			water_clip = 1;
		}
	}
		
	LLAppViewer::instance()->pingMainloopTimeout("Display:Cull");
		
	//Increment drawable frame counter
	LLDrawable::incrementVisible();

	LLSpatialGroup::sNoDelete = TRUE;
	LLTexUnit::sWhiteTexture = LLViewerFetchedTexture::sWhiteImagep->getTexName();

	S32 occlusion = LLPipeline::sUseOcclusion;
	if (gDepthDirty)
	{ //depth buffer is invalid, don't overwrite occlusion state
		LLPipeline::sUseOcclusion = llmin(occlusion, 1);
	}
	gDepthDirty = FALSE;

	LLGLState::checkStates();
	LLGLState::checkTextureChannels();
	LLGLState::checkClientArrays();

	static LLCullResult result;
	LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
	LLPipeline::sUnderWaterRender = LLViewerCamera::getInstance()->cameraUnderWater() ? TRUE : FALSE;
	gPipeline.updateCull(*LLViewerCamera::getInstance(), result, water_clip);
	stop_glerror();

	// <CV:David>
	// Set left / right camera for rendering collected objects.
	if ((render_type == RENDER_STEREO_LEFT) || (render_type == RENDER_RIFT_LEFT))
	{
		LLViewerCamera::getInstance()->moveToLeftEye();
	}
	else if ((render_type == RENDER_STEREO_RIGHT) || (render_type == RENDER_RIFT_RIGHT))
	{
		LLViewerCamera::getInstance()->moveToRightEye();
	}
	stop_glerror();
	display_update_camera();
	stop_glerror();
	// </CV:David>

	LLGLState::checkStates();
	LLGLState::checkTextureChannels();
	LLGLState::checkClientArrays();

	BOOL to_texture = gPipeline.canUseVertexShaders() &&
					LLPipeline::sRenderGlow;

	LLAppViewer::instance()->pingMainloopTimeout("Display:Swap");
		
	{ 
		if (gResizeScreenTexture)
		{
			gResizeScreenTexture = FALSE;
			gPipeline.resizeScreenTexture();
		}

		gGL.setColorMask(true, true);
		glClearColor(0,0,0,0);

		LLGLState::checkStates();
		LLGLState::checkTextureChannels();
		LLGLState::checkClientArrays();

		if (!gSnapshot)
		{
			if (gFrameCount > 1)
			{ //for some reason, ATI 4800 series will error out if you 
				//try to generate a shadow before the first frame is through
				gPipeline.generateSunShadow(*LLViewerCamera::getInstance());
			}

			LLVertexBuffer::unbind();

			LLGLState::checkStates();
			LLGLState::checkTextureChannels();
			LLGLState::checkClientArrays();

			glh::matrix4f proj = glh_get_current_projection();
			glh::matrix4f mod = glh_get_current_modelview();
			glViewport(0,0,512,512);
			LLVOAvatar::updateFreezeCounter() ;

			if(!LLPipeline::sMemAllocationThrottled)
			{		
				LLVOAvatar::updateImpostors();
			}

			glh_set_current_projection(proj);
			glh_set_current_modelview(mod);
			gGL.matrixMode(LLRender::MM_PROJECTION);
			gGL.loadMatrix(proj.m);
			gGL.matrixMode(LLRender::MM_MODELVIEW);
			gGL.loadMatrix(mod.m);
			gViewerWindow->setup3DViewport();

			LLGLState::checkStates();
			LLGLState::checkTextureChannels();
			LLGLState::checkClientArrays();

		}
		glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	LLGLState::checkStates();
	LLGLState::checkClientArrays();

	//if (!gSnapshot)
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:Imagery");
		gPipeline.generateWaterReflection(*LLViewerCamera::getInstance());
		gPipeline.generateHighlight(*LLViewerCamera::getInstance());
		gPipeline.renderPhysicsDisplay();
	}

	LLGLState::checkStates();
	LLGLState::checkClientArrays();

	//////////////////////////////////////
	//
	// Update images, using the image stats generated during object update/culling
	//
	// Can put objects onto the retextured list.
	//
	// Doing this here gives hardware occlusion queries extra time to complete
	LLAppViewer::instance()->pingMainloopTimeout("Display:UpdateImages");
		
	{
		LL_RECORD_BLOCK_TIME(FTM_IMAGE_UPDATE);
			
		{
			LL_RECORD_BLOCK_TIME(FTM_IMAGE_UPDATE_CLASS);
			LLTrace::CountStatHandle<>* velocity_stat = LLViewerCamera::getVelocityStat();
			LLTrace::CountStatHandle<>* angular_velocity_stat = LLViewerCamera::getAngularVelocityStat();
			LLViewerTexture::updateClass(LLTrace::get_frame_recording().getPeriodMeanPerSec(*velocity_stat),
											LLTrace::get_frame_recording().getPeriodMeanPerSec(*angular_velocity_stat));
		}

			
		{
			LL_RECORD_BLOCK_TIME(FTM_IMAGE_UPDATE_BUMP);
			gBumpImageList.updateImages();  // must be called before gTextureList version so that it's textures are thrown out first.
		}

		{
			LL_RECORD_BLOCK_TIME(FTM_IMAGE_UPDATE_LIST);
			F32 max_image_decode_time = 0.050f*gFrameIntervalSeconds.value(); // 50 ms/second decode time
			max_image_decode_time = llclamp(max_image_decode_time, 0.002f, 0.005f ); // min 2ms/frame, max 5ms/frame)
			gTextureList.updateImages(max_image_decode_time);
		}

		/*{
			LL_RECORD_BLOCK_TIME(FTM_IMAGE_UPDATE_DELETE);
			//remove dead textures from GL
			LLImageGL::deleteDeadTextures();
			stop_glerror();
		}*/
	}

	LLGLState::checkStates();
	LLGLState::checkClientArrays();

	///////////////////////////////////
	//
	// StateSort
	//
	// Responsible for taking visible objects, and adding them to the appropriate draw orders.
	// In the case of alpha objects, z-sorts them first.
	// Also creates special lists for outlines and selected face rendering.
	//
	LLAppViewer::instance()->pingMainloopTimeout("Display:StateSort");
	{
		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
		gPipeline.stateSort(*LLViewerCamera::getInstance(), result);
		stop_glerror();
				
		if (gRebuild)
		{
			//////////////////////////////////////
			//
			// rebuildPools
			//
			//
			gPipeline.rebuildPools();
			stop_glerror();
		}
	}

		LLSceneMonitor::getInstance()->fetchQueryResult();
		
	LLGLState::checkStates();
	LLGLState::checkClientArrays();

	LLPipeline::sUseOcclusion = occlusion;

	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:Sky");
		LL_RECORD_BLOCK_TIME(FTM_UPDATE_SKY);	
		gSky.updateSky();
	}

	if(gUseWireframe)
	{
		glClearColor(0.5f, 0.5f, 0.5f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	LLAppViewer::instance()->pingMainloopTimeout("Display:RenderStart");
		
	//// render frontmost floater opaque for occlusion culling purposes
	//LLFloater* frontmost_floaterp = gFloaterView->getFrontmost();
	//// assumes frontmost floater with focus is opaque
	//if (frontmost_floaterp && gFocusMgr.childHasKeyboardFocus(frontmost_floaterp))
	//{
	//	gGL.matrixMode(LLRender::MM_MODELVIEW);
	//	gGL.pushMatrix();
	//	{
	//		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	//		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
	//		gGL.loadIdentity();

	//		LLRect floater_rect = frontmost_floaterp->calcScreenRect();
	//		// deflate by one pixel so rounding errors don't occlude outside of floater extents
	//		floater_rect.stretch(-1);
	//		LLRectf floater_3d_rect((F32)floater_rect.mLeft / (F32)gViewerWindow->getWindowWidthScaled(), 
	//								(F32)floater_rect.mTop / (F32)gViewerWindow->getWindowHeightScaled(),
	//								(F32)floater_rect.mRight / (F32)gViewerWindow->getWindowWidthScaled(),
	//								(F32)floater_rect.mBottom / (F32)gViewerWindow->getWindowHeightScaled());
	//		floater_3d_rect.translate(-0.5f, -0.5f);
	//		gGL.translatef(0.f, 0.f, -LLViewerCamera::getInstance()->getNear());
	//		gGL.scalef(LLViewerCamera::getInstance()->getNear() * LLViewerCamera::getInstance()->getAspect() / sinf(LLViewerCamera::getInstance()->getView()), LLViewerCamera::getInstance()->getNear() / sinf(LLViewerCamera::getInstance()->getView()), 1.f);
	//		gGL.color4fv(LLColor4::white.mV);
	//		gGL.begin(LLVertexBuffer::QUADS);
	//		{
	//			gGL.vertex3f(floater_3d_rect.mLeft, floater_3d_rect.mBottom, 0.f);
	//			gGL.vertex3f(floater_3d_rect.mLeft, floater_3d_rect.mTop, 0.f);
	//			gGL.vertex3f(floater_3d_rect.mRight, floater_3d_rect.mTop, 0.f);
	//			gGL.vertex3f(floater_3d_rect.mRight, floater_3d_rect.mBottom, 0.f);
	//		}
	//		gGL.end();
	//		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	//	}
	//	gGL.popMatrix();
	//}

	LLPipeline::sUnderWaterRender = LLViewerCamera::getInstance()->cameraUnderWater() ? TRUE : FALSE;

// <FS:CR> Aurora Sim
	if (!LLWorld::getInstance()->getAllowRenderWater())
	{
		LLPipeline::sUnderWaterRender = FALSE;
	}
// </FS:CR> Aurora Sim
	LLGLState::checkStates();
	LLGLState::checkClientArrays();

	stop_glerror();

	if (to_texture)
	{
		gGL.setColorMask(true, true);
					
		if (LLPipeline::sRenderDeferred)
		{
			gPipeline.mDeferredScreen.bindTarget();
			glClearColor(1,0,1,1);
			gPipeline.mDeferredScreen.clear();
		}
		else
		{
			gPipeline.mScreen.bindTarget();
			if (LLPipeline::sUnderWaterRender && !gPipeline.canUseWindLightShaders())
			{
				const LLColor4 &col = LLDrawPoolWater::sWaterFogColor;
				glClearColor(col.mV[0], col.mV[1], col.mV[2], 0.f);
			}
			gPipeline.mScreen.clear();
		}
			
		gGL.setColorMask(true, false);
	}
		
	LLAppViewer::instance()->pingMainloopTimeout("Display:RenderGeom");
		
	if (!(LLAppViewer::instance()->logoutRequestSent() && LLAppViewer::instance()->hasSavedFinalSnapshot())
			&& !gRestoreGL)
	{
		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

		// <FS:Ansariel> gSavedSettings replacement
		//if (gSavedSettings.getBOOL("RenderDepthPrePass") && LLGLSLShader::sNoFixedFunction)
		static LLCachedControl<bool> renderDepthPrePass(gSavedSettings, "RenderDepthPrePass");
		if (renderDepthPrePass && LLGLSLShader::sNoFixedFunction)
		// </FS:Ansariel>
		{
			gGL.setColorMask(false, false);
				
			U32 types[] = { 
				LLRenderPass::PASS_SIMPLE, 
				LLRenderPass::PASS_FULLBRIGHT, 
				LLRenderPass::PASS_SHINY 
			};

			U32 num_types = LL_ARRAY_SIZE(types);
			gOcclusionProgram.bind();
			for (U32 i = 0; i < num_types; i++)
			{
				gPipeline.renderObjects(types[i], LLVertexBuffer::MAP_VERTEX, FALSE);
			}

			gOcclusionProgram.unbind();
		}


		gGL.setColorMask(true, false);
		if (LLPipeline::sRenderDeferred)
		{
			gPipeline.renderGeomDeferred(*LLViewerCamera::getInstance());
		}
		else
		{
			gPipeline.renderGeom(*LLViewerCamera::getInstance(), TRUE);
		}
			
		gGL.setColorMask(true, true);

		//store this frame's modelview matrix for use
		//when rendering next frame's occlusion queries
		for (U32 i = 0; i < 16; i++)
		{
			gGLLastModelView[i] = gGLModelView[i];
			gGLLastProjection[i] = gGLProjection[i];
		}
		stop_glerror();
	}

	{
		LL_RECORD_BLOCK_TIME(FTM_TEXTURE_UNBIND);
		for (U32 i = 0; i < gGLManager.mNumTextureImageUnits; i++)
		{ //dummy cleanup of any currently bound textures
			if (gGL.getTexUnit(i)->getCurrType() != LLTexUnit::TT_NONE)
			{
				gGL.getTexUnit(i)->unbind(gGL.getTexUnit(i)->getCurrType());
				gGL.getTexUnit(i)->disable();
			}
		}
	}

	LLAppViewer::instance()->pingMainloopTimeout("Display:RenderFlush");		
		
	if (to_texture)
	{
		if (LLPipeline::sRenderDeferred)
		{
			gPipeline.mDeferredScreen.flush();
			if(LLRenderTarget::sUseFBO)
			{
				LLRenderTarget::copyContentsToFramebuffer(gPipeline.mDeferredScreen, 0, 0, gPipeline.mDeferredScreen.getWidth(), 
															gPipeline.mDeferredScreen.getHeight(), 0, 0, 
															gPipeline.mDeferredScreen.getWidth(), 
															gPipeline.mDeferredScreen.getHeight(), 
															GL_DEPTH_BUFFER_BIT, GL_NEAREST);
			}
		}
		else
		{
			gPipeline.mScreen.flush();
			if(LLRenderTarget::sUseFBO)
			{				
				LLRenderTarget::copyContentsToFramebuffer(gPipeline.mScreen, 0, 0, gPipeline.mScreen.getWidth(), 
															gPipeline.mScreen.getHeight(), 0, 0, 
															gPipeline.mScreen.getWidth(), 
															gPipeline.mScreen.getHeight(), 
															GL_DEPTH_BUFFER_BIT, GL_NEAREST);
			}
		}
	}

	if (LLPipeline::sRenderDeferred)
	{
		gPipeline.renderDeferredLighting();
	}

	LLPipeline::sUnderWaterRender = FALSE;

	{
		//capture the frame buffer.
		LLSceneMonitor::getInstance()->capture();
	}
}

void render_hud_attachments()
{
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
		
	glh::matrix4f current_proj = glh_get_current_projection();
	glh::matrix4f current_mod = glh_get_current_modelview();

	// clamp target zoom level to reasonable values
//	gAgentCamera.mHUDTargetZoom = llclamp(gAgentCamera.mHUDTargetZoom, 0.1f, 1.f);
// [RLVa:KB] - Checked: 2010-08-22 (RLVa-1.2.1a) | Modified: RLVa-1.0.0c
	gAgentCamera.mHUDTargetZoom = llclamp(gAgentCamera.mHUDTargetZoom, (!gRlvAttachmentLocks.hasLockedHUD()) ? 0.1f : 0.85f, 1.f);
// [/RLVa:KB]

	// smoothly interpolate current zoom level
	gAgentCamera.mHUDCurZoom = lerp(gAgentCamera.mHUDCurZoom, gAgentCamera.mHUDTargetZoom, LLSmoothInterpolation::getInterpolant(0.03f));

	if (LLPipeline::sShowHUDAttachments && !gDisconnected && setup_hud_matrices())
	{
		LLPipeline::sRenderingHUDs = TRUE;
		LLCamera hud_cam = *LLViewerCamera::getInstance();
		hud_cam.setOrigin(-1.f,0,0);
		hud_cam.setAxes(LLVector3(1,0,0), LLVector3(0,1,0), LLVector3(0,0,1));
		LLViewerCamera::updateFrustumPlanes(hud_cam, TRUE);

		// <FS:Ansariel> gSavedSettings replacement
		//bool render_particles = gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES) && gSavedSettings.getBOOL("RenderHUDParticles");
		static LLCachedControl<bool> renderHUDParticles(gSavedSettings, "RenderHUDParticles");
		bool render_particles = gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES) && renderHUDParticles;
		// </FS:Ansariel>
		
		//only render hud objects
		gPipeline.pushRenderTypeMask();
		
		// turn off everything
		gPipeline.andRenderTypeMask(LLPipeline::END_RENDER_TYPES);
		// turn on HUD
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
		// turn on HUD particles
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);

		// if particles are off, turn off hud-particles as well
		if (!render_particles)
		{
			// turn back off HUD particles
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
		}

		bool has_ui = gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);
		if (has_ui)
		{
			gPipeline.toggleRenderDebugFeature((void*) LLPipeline::RENDER_DEBUG_FEATURE_UI);
		}

		S32 use_occlusion = LLPipeline::sUseOcclusion;
		LLPipeline::sUseOcclusion = 0;
				
		//cull, sort, and render hud objects
		static LLCullResult result;
		LLSpatialGroup::sNoDelete = TRUE;

		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
		gPipeline.updateCull(hud_cam, result);

		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_BUMP);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_SIMPLE);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_VOLUME);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_ALPHA);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_FULLBRIGHT_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_FULLBRIGHT);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_ALPHA);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_BUMP);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_MATERIAL);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_SHINY);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_INVISIBLE);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_INVISI_SHINY);
		
		gPipeline.stateSort(hud_cam, result);

		gPipeline.renderGeom(hud_cam);

		LLSpatialGroup::sNoDelete = FALSE;
		//gPipeline.clearReferences();

		render_hud_elements();

		//restore type mask
		gPipeline.popRenderTypeMask();

		if (has_ui)
		{
			gPipeline.toggleRenderDebugFeature((void*) LLPipeline::RENDER_DEBUG_FEATURE_UI);
		}
		LLPipeline::sUseOcclusion = use_occlusion;
		LLPipeline::sRenderingHUDs = FALSE;
	}
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();
	
	glh_set_current_projection(current_proj);
	glh_set_current_modelview(current_mod);
}

LLRect get_whole_screen_region()
{
	LLRect whole_screen = gViewerWindow->getWorldViewRectScaled();
	
	// apply camera zoom transform (for high res screenshots)
	F32 zoom_factor = LLViewerCamera::getInstance()->getZoomFactor();
	S16 sub_region = LLViewerCamera::getInstance()->getZoomSubRegion();
	if (zoom_factor > 1.f)
	{
		S32 num_horizontal_tiles = llceil(zoom_factor);
		S32 tile_width = llround((F32)gViewerWindow->getWorldViewWidthScaled() / zoom_factor);
		S32 tile_height = llround((F32)gViewerWindow->getWorldViewHeightScaled() / zoom_factor);
		int tile_y = sub_region / num_horizontal_tiles;
		int tile_x = sub_region - (tile_y * num_horizontal_tiles);
			
		whole_screen.setLeftTopAndSize(tile_x * tile_width, gViewerWindow->getWorldViewHeightScaled() - (tile_y * tile_height), tile_width, tile_height);
	}
	return whole_screen;
}

bool get_hud_matrices(const LLRect& screen_region, glh::matrix4f &proj, glh::matrix4f &model)
{
	if (isAgentAvatarValid() && gAgentAvatarp->hasHUDAttachment())
	{
		F32 zoom_level = gAgentCamera.mHUDCurZoom;
		LLBBox hud_bbox = gAgentAvatarp->getHUDBBox();
		
		F32 hud_depth = llmax(1.f, hud_bbox.getExtentLocal().mV[VX] * 1.1f);
		proj = gl_ortho(-0.5f * LLViewerCamera::getInstance()->getAspect(), 0.5f * LLViewerCamera::getInstance()->getAspect(), -0.5f, 0.5f, 0.f, hud_depth);
		proj.element(2,2) = -0.01f;
		
		F32 aspect_ratio = LLViewerCamera::getInstance()->getAspect();
		
		glh::matrix4f mat;
		F32 scale_x = (F32)gViewerWindow->getWorldViewWidthScaled() / (F32)screen_region.getWidth();
		F32 scale_y = (F32)gViewerWindow->getWorldViewHeightScaled() / (F32)screen_region.getHeight();
		mat.set_scale(glh::vec3f(scale_x, scale_y, 1.f));
		mat.set_translate(
			glh::vec3f(clamp_rescale((F32)(screen_region.getCenterX() - screen_region.mLeft), 0.f, (F32)gViewerWindow->getWorldViewWidthScaled(), 0.5f * scale_x * aspect_ratio, -0.5f * scale_x * aspect_ratio),
					   clamp_rescale((F32)(screen_region.getCenterY() - screen_region.mBottom), 0.f, (F32)gViewerWindow->getWorldViewHeightScaled(), 0.5f * scale_y, -0.5f * scale_y),
					   0.f));
		proj *= mat;
		
		glh::matrix4f tmp_model((GLfloat*) OGL_TO_CFR_ROTATION);
		
		mat.set_scale(glh::vec3f(zoom_level, zoom_level, zoom_level));
		mat.set_translate(glh::vec3f(-hud_bbox.getCenterLocal().mV[VX] + (hud_depth * 0.5f), 0.f, 0.f));
		
		tmp_model *= mat;
		model = tmp_model;		
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

bool get_hud_matrices(glh::matrix4f &proj, glh::matrix4f &model)
{
	LLRect whole_screen = get_whole_screen_region();
	return get_hud_matrices(whole_screen, proj, model);
}

BOOL setup_hud_matrices()
{
	LLRect whole_screen = get_whole_screen_region();
	return setup_hud_matrices(whole_screen);
}

BOOL setup_hud_matrices(const LLRect& screen_region)
{
	glh::matrix4f proj, model;
	bool result = get_hud_matrices(screen_region, proj, model);
	if (!result) return result;
	
	// set up transform to keep HUD objects in front of camera
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.loadMatrix(proj.m);
	glh_set_current_projection(proj);
	
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.loadMatrix(model.m);
	glh_set_current_modelview(model);
	return TRUE;
}

void render_ui(F32 zoom_factor, int subfield)
{
	LLGLState::checkStates();
	
	glh::matrix4f saved_view = glh_get_current_modelview();

	if (!gSnapshot)
	{
		gGL.pushMatrix();
		gGL.loadMatrix(gGLLastModelView);
		glh_set_current_modelview(glh_copy_matrix(gGLLastModelView));
	}
	
	if(LLSceneMonitor::getInstance()->needsUpdate())
	{
		gGL.pushMatrix();
		gViewerWindow->setup2DRender();
		LLSceneMonitor::getInstance()->compare();
		gViewerWindow->setup3DRender();
		gGL.popMatrix();
	}

	{
		BOOL to_texture = gPipeline.canUseVertexShaders() &&
							LLPipeline::sRenderGlow;

		if (to_texture)
		{
			gPipeline.renderBloom(gSnapshot, zoom_factor, subfield);
		}

		// <CV:David>
		if (gRift3DEnabled)
		{
			gPipeline.mScreen.bindTarget();
		}
		// </CV:David>
		
		render_hud_elements();
		// <CV:David>
		//render_hud_attachments();
		if (!gRift3DEnabled)
		{
			render_hud_attachments();
		}
		// </CV:David>
	}

	LLGLSDefault gls_default;
	LLGLSUIDefault gls_ui;
	{
		gPipeline.disableLights();
	}

	{
		gGL.color4f(1,1,1,1);
		if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
		{
			LL_RECORD_BLOCK_TIME(FTM_RENDER_UI);

			if (!gDisconnected)
			{
				// <CV:David>
				//render_ui_3d();
				if (!gRift3DEnabled)
				{
					render_ui_3d();
				}
				// </CV:David>
				LLGLState::checkStates();
			}
			else
			{
				render_disconnected_background();
			}

			render_ui_2d();
			LLGLState::checkStates();
		}
		gGL.flush();

		{
			gViewerWindow->setup2DRender();
			gViewerWindow->updateDebugText();
			gViewerWindow->drawDebugText();
		}

		LLVertexBuffer::unbind();

		// <CV:David>
		if (gRift3DEnabled)
		{
			gPipeline.mScreen.flush();

			int curIndex;
			GLuint curTexId;
			ovr_GetTextureSwapChainCurrentIndex(gRiftSession, gRiftTextureSwapChain[gRiftCurrentEye], &curIndex);
			ovr_GetTextureSwapChainBufferGL(gRiftSession, gRiftTextureSwapChain[gRiftCurrentEye], curIndex, &curTexId);
			LLRenderTarget::copyContentsToTexture(gPipeline.mScreen, 0, 0, gRiftHBuffer, gRiftVBuffer, 
				curTexId, 0, 0, gRiftHBuffer, gRiftVBuffer, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		// </CV:David>
	}

	if (!gSnapshot)
	{
		glh_set_current_modelview(saved_view);
		gGL.popMatrix();
	}
}

void swap()
{
	LL_RECORD_BLOCK_TIME(FTM_SWAP);

	if (gDisplaySwapBuffers)
	{
		gViewerWindow->getWindow()->swapBuffers();
	}
	gDisplaySwapBuffers = TRUE;
}

void renderCoordinateAxes()
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.begin(LLRender::LINES);
		gGL.color3f(1.0f, 0.0f, 0.0f);   // i direction = X-Axis = red
		gGL.vertex3f(0.0f, 0.0f, 0.0f);
		gGL.vertex3f(2.0f, 0.0f, 0.0f);
		gGL.vertex3f(3.0f, 0.0f, 0.0f);
		gGL.vertex3f(5.0f, 0.0f, 0.0f);
		gGL.vertex3f(6.0f, 0.0f, 0.0f);
		gGL.vertex3f(8.0f, 0.0f, 0.0f);
		// Make an X
		gGL.vertex3f(11.0f, 1.0f, 1.0f);
		gGL.vertex3f(11.0f, -1.0f, -1.0f);
		gGL.vertex3f(11.0f, 1.0f, -1.0f);
		gGL.vertex3f(11.0f, -1.0f, 1.0f);

		gGL.color3f(0.0f, 1.0f, 0.0f);   // j direction = Y-Axis = green
		gGL.vertex3f(0.0f, 0.0f, 0.0f);
		gGL.vertex3f(0.0f, 2.0f, 0.0f);
		gGL.vertex3f(0.0f, 3.0f, 0.0f);
		gGL.vertex3f(0.0f, 5.0f, 0.0f);
		gGL.vertex3f(0.0f, 6.0f, 0.0f);
		gGL.vertex3f(0.0f, 8.0f, 0.0f);
		// Make a Y
		gGL.vertex3f(1.0f, 11.0f, 1.0f);
		gGL.vertex3f(0.0f, 11.0f, 0.0f);
		gGL.vertex3f(-1.0f, 11.0f, 1.0f);
		gGL.vertex3f(0.0f, 11.0f, 0.0f);
		gGL.vertex3f(0.0f, 11.0f, 0.0f);
		gGL.vertex3f(0.0f, 11.0f, -1.0f);

		gGL.color3f(0.0f, 0.0f, 1.0f);   // Z-Axis = blue
		gGL.vertex3f(0.0f, 0.0f, 0.0f);
		gGL.vertex3f(0.0f, 0.0f, 2.0f);
		gGL.vertex3f(0.0f, 0.0f, 3.0f);
		gGL.vertex3f(0.0f, 0.0f, 5.0f);
		gGL.vertex3f(0.0f, 0.0f, 6.0f);
		gGL.vertex3f(0.0f, 0.0f, 8.0f);
		// Make a Z
		gGL.vertex3f(-1.0f, 1.0f, 11.0f);
		gGL.vertex3f(1.0f, 1.0f, 11.0f);
		gGL.vertex3f(1.0f, 1.0f, 11.0f);
		gGL.vertex3f(-1.0f, -1.0f, 11.0f);
		gGL.vertex3f(-1.0f, -1.0f, 11.0f);
		gGL.vertex3f(1.0f, -1.0f, 11.0f);
	gGL.end();
}


void draw_axes() 
{
	LLGLSUIDefault gls_ui;
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	// A vertical white line at origin
	LLVector3 v = gAgent.getPositionAgent();
	gGL.begin(LLRender::LINES);
		gGL.color3f(1.0f, 1.0f, 1.0f); 
		gGL.vertex3f(0.0f, 0.0f, 0.0f);
		gGL.vertex3f(0.0f, 0.0f, 40.0f);
	gGL.end();
	// Some coordinate axes
	gGL.pushMatrix();
		gGL.translatef( v.mV[VX], v.mV[VY], v.mV[VZ] );
		renderCoordinateAxes();
	gGL.popMatrix();
}

void render_ui_3d()
{
	LLGLSPipeline gls_pipeline;

	//////////////////////////////////////
	//
	// Render 3D UI elements
	// NOTE: zbuffer is cleared before we get here by LLDrawPoolHUD,
	//		 so 3d elements requiring Z buffer are moved to LLDrawPoolHUD
	//

	/////////////////////////////////////////////////////////////
	//
	// Render 2.5D elements (2D elements in the world)
	// Stuff without z writes
	//

	// Debugging stuff goes before the UI.

	stop_glerror();
	
	if (LLGLSLShader::sNoFixedFunction)
	{
		gUIProgram.bind();
	}

	// Coordinate axes
	// <FS:Ansariel> gSavedSettings replacement
	//if (gSavedSettings.getBOOL("ShowAxes"))
	static LLCachedControl<bool> showAxes(gSavedSettings, "ShowAxes");
	if (showAxes)
	// </FS:Ansariel>
	{
		draw_axes();
	}

	gViewerWindow->renderSelections(FALSE, FALSE, TRUE); // Non HUD call in render_hud_elements
	stop_glerror();
}

void render_ui_2d()
{
	// <CV:David>
	if (gRift3DEnabled)
	{
		gPipeline.mScreen.flush();
		gPipeline.mUIScreen.bindTarget();
		gGL.setColorMask(true, true);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	// </CV:David>

	LLGLSUIDefault gls_ui;

	/////////////////////////////////////////////////////////////
	//
	// Render 2D UI elements that overlay the world (no z compare)

	//  Disable wireframe mode below here, as this is HUD/menus
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	//  Menu overlays, HUD, etc
	gViewerWindow->setup2DRender();

	F32 zoom_factor = LLViewerCamera::getInstance()->getZoomFactor();
	S16 sub_region = LLViewerCamera::getInstance()->getZoomSubRegion();

	if (zoom_factor > 1.f)
	{
		//decompose subregion number to x and y values
		int pos_y = sub_region / llceil(zoom_factor);
		int pos_x = sub_region - (pos_y*llceil(zoom_factor));
		// offset for this tile
		LLFontGL::sCurOrigin.mX -= llround((F32)gViewerWindow->getWindowWidthScaled() * (F32)pos_x / zoom_factor);
		LLFontGL::sCurOrigin.mY -= llround((F32)gViewerWindow->getWindowHeightScaled() * (F32)pos_y / zoom_factor);
	}

	stop_glerror();
	//gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);

	// render outline for HUD
	if (isAgentAvatarValid() && gAgentCamera.mHUDCurZoom < 0.98f)
	{
		gGL.pushMatrix();
		S32 half_width = (gViewerWindow->getWorldViewWidthScaled() / 2);
		S32 half_height = (gViewerWindow->getWorldViewHeightScaled() / 2);
		gGL.scalef(LLUI::getScaleFactor().mV[0], LLUI::getScaleFactor().mV[1], 1.f);
		gGL.translatef((F32)half_width, (F32)half_height, 0.f);
		F32 zoom = gAgentCamera.mHUDCurZoom;
		gGL.scalef(zoom,zoom,1.f);
		gGL.color4fv(LLColor4::white.mV);
		gl_rect_2d(-half_width, half_height, half_width, -half_height, FALSE);
		gGL.popMatrix();
		stop_glerror();
	}
	

	// <FS:Ansariel> gSavedSettings replacement
	//if (gSavedSettings.getBOOL("RenderUIBuffer"))
	static LLCachedControl<bool> renderUIBuffer(gSavedSettings, "RenderUIBuffer");
	if (renderUIBuffer)
	// </FS:Ansariel>
	{
		if (LLUI::sDirty)
		{
			LLUI::sDirty = FALSE;
			LLRect t_rect;

			gPipeline.mUIScreen.bindTarget();
			gGL.setColorMask(true, true);
			{
				static const S32 pad = 8;

				LLUI::sDirtyRect.mLeft -= pad;
				LLUI::sDirtyRect.mRight += pad;
				LLUI::sDirtyRect.mBottom -= pad;
				LLUI::sDirtyRect.mTop += pad;

				LLGLEnable scissor(GL_SCISSOR_TEST);
				static LLRect last_rect = LLUI::sDirtyRect;

				//union with last rect to avoid mouse poop
				last_rect.unionWith(LLUI::sDirtyRect);
								
				t_rect = LLUI::sDirtyRect;
				LLUI::sDirtyRect = last_rect;
				last_rect = t_rect;
			
				last_rect.mLeft = LLRect::tCoordType(last_rect.mLeft / LLUI::getScaleFactor().mV[0]);
				last_rect.mRight = LLRect::tCoordType(last_rect.mRight / LLUI::getScaleFactor().mV[0]);
				last_rect.mTop = LLRect::tCoordType(last_rect.mTop / LLUI::getScaleFactor().mV[1]);
				last_rect.mBottom = LLRect::tCoordType(last_rect.mBottom / LLUI::getScaleFactor().mV[1]);

				LLRect clip_rect(last_rect);
				
				glClear(GL_COLOR_BUFFER_BIT);

				gViewerWindow->draw();
			}

			gPipeline.mUIScreen.flush();
			gGL.setColorMask(true, false);

			LLUI::sDirtyRect = t_rect;
		}

		LLGLDisable cull(GL_CULL_FACE);
		LLGLDisable blend(GL_BLEND);
		S32 width = gViewerWindow->getWindowWidthScaled();
		S32 height = gViewerWindow->getWindowHeightScaled();
		gGL.getTexUnit(0)->bind(&gPipeline.mUIScreen);
		gGL.begin(LLRender::TRIANGLE_STRIP);
		gGL.color4f(1,1,1,1);
		gGL.texCoord2f(0, 0);			gGL.vertex2i(0, 0);
		gGL.texCoord2f(width, 0);		gGL.vertex2i(width, 0);
		gGL.texCoord2f(0, height);		gGL.vertex2i(0, height);
		gGL.texCoord2f(width, height);	gGL.vertex2i(width, height);
		gGL.end();
	}
	else
	{
		gViewerWindow->draw();
	}

	// <CV:David>
	if (gRift3DEnabled)
	{
		gPipeline.mUIScreen.flush();

		gPipeline.mScreen.bindTarget();
		gSplatTextureRectProgram.bind();
		gGL.getTexUnit(0)->bind(&gPipeline.mUIScreen);

		S32 uiDepth = LLAppViewer::instance()->getRiftUIDepth();
		S32 offset = (gRiftCurrentEye == 0) ? uiDepth + gRiftLensOffset : -uiDepth - gRiftLensOffset;
		S32 width = gRiftHBuffer;
		S32 height = gRiftVBuffer;
		LLGLEnable blend(GL_BLEND);
		gGL.setColorMask(true, false);
		gGL.color4f(1,1,1,1);
		gGL.begin(LLRender::QUADS);
			gGL.texCoord2f(0, height);		gGL.vertex2i(offset, height);
			gGL.texCoord2f(0, 0);			gGL.vertex2i(offset, 0);
			gGL.texCoord2f(width, 0);		gGL.vertex2i(offset + width, 0);
			gGL.texCoord2f(width, height);	gGL.vertex2i(offset + width, height);
		gGL.end();
		gGL.flush();

		gSplatTextureRectProgram.unbind();
		gPipeline.mScreen.flush();
	}
	// </CV:David>

	// reset current origin for font rendering, in case of tiling render
	LLFontGL::sCurOrigin.set(0, 0);
}

void render_disconnected_background()
{
	if (LLGLSLShader::sNoFixedFunction)
	{
		gUIProgram.bind();
	}

	gGL.color4f(1,1,1,1);
	if (!gDisconnectedImagep && gDisconnected)
	{
		LL_INFOS() << "Loading last bitmap..." << LL_ENDL;

		std::string temp_str;
		temp_str = gDirUtilp->getLindenUserDir() + gDirUtilp->getDirDelimiter() + SCREEN_LAST_FILENAME;

		LLPointer<LLImageBMP> image_bmp = new LLImageBMP;
		if( !image_bmp->load(temp_str) )
		{
			//LL_INFOS() << "Bitmap load failed" << LL_ENDL;
			return;
		}
		
		LLPointer<LLImageRaw> raw = new LLImageRaw;
		if (!image_bmp->decode(raw, 0.0f))
		{
			LL_INFOS() << "Bitmap decode failed" << LL_ENDL;
			gDisconnectedImagep = NULL;
			return;
		}

		U8 *rawp = raw->getData();
		S32 npixels = (S32)image_bmp->getWidth()*(S32)image_bmp->getHeight();
		for (S32 i = 0; i < npixels; i++)
		{
			S32 sum = 0;
			sum = *rawp + *(rawp+1) + *(rawp+2);
			sum /= 3;
			*rawp = ((S32)sum*6 + *rawp)/7;
			rawp++;
			*rawp = ((S32)sum*6 + *rawp)/7;
			rawp++;
			*rawp = ((S32)sum*6 + *rawp)/7;
			rawp++;
		}

		
		raw->expandToPowerOfTwo();
		gDisconnectedImagep = LLViewerTextureManager::getLocalTexture(raw.get(), FALSE );
		gStartTexture = gDisconnectedImagep;
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	}

	// Make sure the progress view always fills the entire window.
	S32 width = gViewerWindow->getWindowWidthScaled();
	S32 height = gViewerWindow->getWindowHeightScaled();

	if (gDisconnectedImagep)
	{
		LLGLSUIDefault gls_ui;
		gViewerWindow->setup2DRender();
		gGL.pushMatrix();
		{
			// scale ui to reflect UIScaleFactor
			// this can't be done in setup2DRender because it requires a
			// pushMatrix/popMatrix pair
			const LLVector2& display_scale = gViewerWindow->getDisplayScale();
			gGL.scalef(display_scale.mV[VX], display_scale.mV[VY], 1.f);

			gGL.getTexUnit(0)->bind(gDisconnectedImagep);
			gGL.color4f(1.f, 1.f, 1.f, 1.f);
			gl_rect_2d_simple_tex(width, height);
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		}
		gGL.popMatrix();
	}
	gGL.flush();

	if (LLGLSLShader::sNoFixedFunction)
	{
		gUIProgram.unbind();
	}

}

void display_cleanup()
{
	gDisconnectedImagep = NULL;
}

// <CV:David>
void setRiftSDKRendering(bool on)
{
	if (on)
	{
		if (gRiftInitialized)
		{
			LL_INFOS() << "Oculus Rift: Started Rift rendering" << LL_ENDL;
		}
		else
		{
			LL_WARNS("InitInfo") << "Oculus Rift: Could not start Rift rendering!" << LL_ENDL;
			LLNotificationsUtil::add("AlertCouldNotStartRiftRendering", LLSD());
		}
	}
	else
	{
		LL_INFOS("InitInfo") << "Oculus Rift: Ended Rift rendering" << LL_ENDL;
	}
}
// </CV:David>
