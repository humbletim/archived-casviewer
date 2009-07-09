/** 
 * @file llvoavatar.cpp
 * @brief Implementation of LLVOAvatar class which is a derivation fo LLViewerObject
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llvoavatarself.h"
#include "llvoavatar.h"

#include <stdio.h>
#include <ctype.h>

#include "audioengine.h"
#include "noise.h"

// TODO: Seraph - Remove unnecessary headers.  These are copied from llvoavatar.h.
#include "llagent.h" //  Get state values from here
#include "llagentwearables.h"
#include "llviewercontrol.h"
#include "lldrawpoolavatar.h"
#include "lldriverparam.h"
#include "lleditingmotion.h"
#include "llemote.h"
#include "llface.h"
#include "llfirstuse.h"
#include "llheadrotmotion.h"
#include "llhudeffecttrail.h"
#include "llhudmanager.h"
#include "llfloaterinventory.h"
#include "llkeyframefallmotion.h"
#include "llkeyframestandmotion.h"
#include "llkeyframewalkmotion.h"
#include "llmutelist.h"
#include "llselectmgr.h"
#include "llsprite.h"
#include "lltargetingmotion.h"
#include "lltexlayer.h"
#include "lltexglobalcolor.h"
#include "lltoolgrab.h"	// for needsRenderBeam
#include "lltoolmgr.h" // for needsRenderBeam
#include "lltoolmorph.h"
#include "lltrans.h"
#include "llviewercamera.h"
#include "llviewerimagelist.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerstats.h"
#include "llvovolume.h"
#include "llworld.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llsky.h"
#include "llanimstatelabels.h"
#include "llgesturemgr.h" //needed to trigger the voice gesticulations
#include "llvoiceclient.h"
#include "llvoicevisualizer.h" // Ventrella

#include "boost/lexical_cast.hpp"

using namespace LLVOAvatarDefines;

/*********************************************************************************
 **                                                                             **
 ** Begin private LLVOAvatarSelf Support classes
 **
 **/

struct LocalTextureData
{
	LocalTextureData() : 
		mIsBakedReady(FALSE), 
		mDiscard(MAX_DISCARD_LEVEL+1), 
		mImage(NULL), 
		mWearableID(IMG_DEFAULT_AVATAR),
		mTexEntry(NULL)
	{}
	LLPointer<LLViewerImage> mImage;
	BOOL mIsBakedReady;
	S32 mDiscard;
	LLUUID mWearableID;	// UUID of the wearable that this texture belongs to, not of the image itself
	LLTextureEntry *mTexEntry;
};

//-----------------------------------------------------------------------------
// Callback data
//-----------------------------------------------------------------------------
struct LLAvatarTexData
{
	LLAvatarTexData(const LLUUID& id, ETextureIndex index) : 
		mAvatarID(id), 
		mIndex(index) 
	{}
	LLUUID			mAvatarID;
	ETextureIndex	mIndex;
};

/**
 **
 ** End LLVOAvatarSelf Support classes
 **                                                                             **
 *********************************************************************************/


//-----------------------------------------------------------------------------
// Static Data
//-----------------------------------------------------------------------------
S32 LLVOAvatarSelf::sScratchTexBytes = 0;
LLMap< LLGLenum, LLGLuint*> LLVOAvatarSelf::sScratchTexNames;
LLMap< LLGLenum, F32*> LLVOAvatarSelf::sScratchTexLastBindTime;


/*********************************************************************************
 **                                                                             **
 ** Begin LLVOAvatarSelf Constructor routines
 **
 **/

LLVOAvatarSelf::LLVOAvatarSelf(const LLUUID& id,
							   const LLPCode pcode,
							   LLViewerRegion* regionp) :
	LLVOAvatar(id, pcode, regionp),
	mScreenp(NULL),
	mLastRegionHandle(0),
	mRegionCrossingCount(0)
{
	gAgent.setAvatarObject(this);
	gAgentWearables.setAvatarObject(this);
	
	lldebugs << "Marking avatar as self " << id << llendl;
	
	for (U32 i = 0; i < TEX_NUM_INDICES; i++)
	{
		mLocalTextureDatas[(ETextureIndex)i].push_back(new LocalTextureData);
	}
}

void LLVOAvatarSelf::initInstance()
{
	BOOL status = TRUE;
	// creates hud joint(mScreen) among other things
	status &= loadAvatarSelf();

	// adds attachment points to mScreen among other things
	LLVOAvatar::initInstance();

	status &= buildMenus();
	if (!status)
	{
		llerrs << "Unable to load user's avatar" << llendl;
		return;
	}
}

// virtual
void LLVOAvatarSelf::markDead()
{
	mBeam = NULL;
	LLVOAvatar::markDead();
}

BOOL LLVOAvatarSelf::loadAvatarSelf()
{
	BOOL success = TRUE;
	// avatar_skeleton.xml
	if (!buildSkeletonSelf(sAvatarSkeletonInfo))
	{
		llwarns << "avatar file: buildSkeleton() failed" << llendl;
		return FALSE;
	}
	// TODO: make loadLayersets() called only by self.
	//success &= loadLayersets();

	return success;
}

BOOL LLVOAvatarSelf::buildSkeletonSelf(const LLVOAvatarSkeletonInfo *info)
{
	LLMemType mt(LLMemType::MTYPE_AVATAR);

	// add special-purpose "screen" joint
	mScreenp = new LLViewerJoint("mScreen", NULL);
	// for now, put screen at origin, as it is only used during special
	// HUD rendering mode
	F32 aspect = LLViewerCamera::getInstance()->getAspect();
	LLVector3 scale(1.f, aspect, 1.f);
	mScreenp->setScale(scale);
	mScreenp->setWorldPosition(LLVector3::zero);
	return TRUE;
}

BOOL LLVOAvatarSelf::buildMenus()
{
	//-------------------------------------------------------------------------
	// build the attach and detach menus
	//-------------------------------------------------------------------------
	gAttachBodyPartPieMenus[0] = NULL;

	LLContextMenu::Params params;
	params.label(LLTrans::getString("BodyPartsRightArm") + " >");
	params.name(params.label);
	params.visible(false);
	gAttachBodyPartPieMenus[1] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsHead") + " >");
	params.name(params.label);
	gAttachBodyPartPieMenus[2] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsLeftArm") + " >");
	params.name(params.label);
	gAttachBodyPartPieMenus[3] = LLUICtrlFactory::create<LLContextMenu> (params);

	gAttachBodyPartPieMenus[4] = NULL;

	params.label(LLTrans::getString("BodyPartsLeftLeg") + " >");
	params.name(params.label);
	gAttachBodyPartPieMenus[5] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsTorso") + " >");
	params.name(params.label);
	gAttachBodyPartPieMenus[6] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsRightLeg") + " >");
	params.name(params.label);
	gAttachBodyPartPieMenus[7] = LLUICtrlFactory::create<LLContextMenu> (params);

	gDetachBodyPartPieMenus[0] = NULL;

	params.label(LLTrans::getString("BodyPartsRightArm") + " >");
	params.name(params.label);
	gDetachBodyPartPieMenus[1] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsHead") + " >");
	params.name(params.label);
	gDetachBodyPartPieMenus[2] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsLeftArm") + " >");
	params.name(params.label);
	gDetachBodyPartPieMenus[3] = LLUICtrlFactory::create<LLContextMenu> (params);

	gDetachBodyPartPieMenus[4] = NULL;

	params.label(LLTrans::getString("BodyPartsLeftLeg") + " >");
	params.name(params.label);
	gDetachBodyPartPieMenus[5] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsTorso") + " >");
	params.name(params.label);
	gDetachBodyPartPieMenus[6] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsRightLeg") + " >");
	params.name(params.label);
	gDetachBodyPartPieMenus[7] = LLUICtrlFactory::create<LLContextMenu> (params);

	for (S32 i = 0; i < 8; i++)
	{
		if (gAttachBodyPartPieMenus[i])
		{
			gAttachPieMenu->appendContextSubMenu( gAttachBodyPartPieMenus[i] );
		}
		else
		{
			BOOL attachment_found = FALSE;
			for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
				 iter != mAttachmentPoints.end(); )
			{
				attachment_map_t::iterator curiter = iter++;
				LLViewerJointAttachment* attachment = curiter->second;
				if (attachment->getGroup() == i)
				{
					LLMenuItemCallGL::Params item_params;
						
					std::string sub_piemenu_name = attachment->getName();
					if (LLTrans::getString(sub_piemenu_name) != "")
					{
						item_params.label = LLTrans::getString(sub_piemenu_name);
					}
					else
					{
						item_params.label = sub_piemenu_name;
					}
					item_params.name =(item_params.label );
					item_params.on_click.function_name = "Object.AttachToAvatar";
					item_params.on_click.parameter = curiter->first;
					item_params.on_enable.function_name = "Object.EnableWear";
					item_params.on_enable.parameter = curiter->first;
					LLMenuItemCallGL* item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);

					gAttachPieMenu->addChild(item);

					attachment_found = TRUE;
					break;

				}
			}

			if (!attachment_found)
			{
				gAttachPieMenu->addSeparator();
			}
		}

		if (gDetachBodyPartPieMenus[i])
		{
			gDetachPieMenu->appendContextSubMenu( gDetachBodyPartPieMenus[i] );
		}
		else
		{
			BOOL attachment_found = FALSE;
			for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
				 iter != mAttachmentPoints.end(); )
			{
				attachment_map_t::iterator curiter = iter++;
				LLViewerJointAttachment* attachment = curiter->second;
				if (attachment->getGroup() == i)
				{
					LLMenuItemCallGL::Params item_params;
					std::string sub_piemenu_name = attachment->getName();
					if (LLTrans::getString(sub_piemenu_name) != "")
					{
						item_params.label = LLTrans::getString(sub_piemenu_name);
					}
					else
					{
						item_params.label = sub_piemenu_name;
					}
					item_params.name =(item_params.label );
					item_params.on_click.function_name = "Attachment.Detach";
					item_params.on_click.parameter = curiter->first;
					item_params.on_enable.function_name = "Attachment.EnableDetach";
					item_params.on_enable.parameter = curiter->first;
					LLMenuItemCallGL* item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);

					gDetachPieMenu->addChild(item);
						
					attachment_found = TRUE;
					break;
				}
			}

			if (!attachment_found)
			{
				gDetachPieMenu->addSeparator();
			}
		}
	}

	// add screen attachments
	for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
		 iter != mAttachmentPoints.end(); )
	{
		attachment_map_t::iterator curiter = iter++;
		LLViewerJointAttachment* attachment = curiter->second;
		if (attachment->getGroup() == 8)
		{
			LLMenuItemCallGL::Params item_params;
			std::string sub_piemenu_name = attachment->getName();
			if (LLTrans::getString(sub_piemenu_name) != "")
			{
				item_params.label = LLTrans::getString(sub_piemenu_name);
			}
			else
			{
				item_params.label = sub_piemenu_name;
			}
			item_params.name =(item_params.label );
			item_params.on_click.function_name = "Object.AttachToAvatar";
			item_params.on_click.parameter = curiter->first;
			item_params.on_enable.function_name = "Object.EnableWear";
			item_params.on_enable.parameter = curiter->first;			
			LLMenuItemCallGL* item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);
			gAttachScreenPieMenu->addChild(item);

			item_params.on_click.function_name = "Attachment.DetachFromPoint";
			item_params.on_click.parameter = curiter->first;
			item_params.on_enable.function_name = "Attachment.PointFilled";
			item_params.on_enable.parameter = curiter->first;
			item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);
			gDetachScreenPieMenu->addChild(item);
		}
	}

	for (S32 pass = 0; pass < 2; pass++)
	{
		// *TODO: Skinning - gAttachSubMenu is an awful, awful hack
		if (!gAttachSubMenu)
		{
			break;
		}
		for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
			 iter != mAttachmentPoints.end(); )
		{
			attachment_map_t::iterator curiter = iter++;
			LLViewerJointAttachment* attachment = curiter->second;
			if (attachment->getIsHUDAttachment() != (pass == 1))
			{
				continue;
			}
			LLMenuItemCallGL::Params item_params;
			std::string sub_piemenu_name = attachment->getName();
			if (LLTrans::getString(sub_piemenu_name) != "")
			{
				item_params.label = LLTrans::getString(sub_piemenu_name);
			}
			else
			{
				item_params.label = sub_piemenu_name;
			}
			item_params.name =(item_params.label );
			item_params.on_click.function_name = "Object.AttachToAvatar";
			item_params.on_click.parameter = curiter->first;
			item_params.on_enable.function_name = "Object.EnableWear";
			item_params.on_enable.parameter = curiter->first;
			//* TODO: Skinning:
			//LLSD params;
			//params["index"] = curiter->first;
			//params["label"] = attachment->getName();
			//item->addEventHandler("on_enable", LLMenuItemCallGL::MenuCallback().function_name("Attachment.Label").parameter(params));
				
			LLMenuItemCallGL* item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);
			gAttachSubMenu->addChild(item);

			item_params.on_click.function_name = "Attachment.DetachFromPoint";
			item_params.on_click.parameter = curiter->first;
			item_params.on_enable.function_name = "Attachment.PointFilled";
			item_params.on_enable.parameter = curiter->first;
			//* TODO: Skinning: item->addEventHandler("on_enable", LLMenuItemCallGL::MenuCallback().function_name("Attachment.Label").parameter(params));

			item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);
			gDetachSubMenu->addChild(item);
		}
		if (pass == 0)
		{
			// put separator between non-hud and hud attachments
			gAttachSubMenu->addSeparator();
			gDetachSubMenu->addSeparator();
		}
	}

	for (S32 group = 0; group < 8; group++)
	{
		// skip over groups that don't have sub menus
		if (!gAttachBodyPartPieMenus[group] || !gDetachBodyPartPieMenus[group])
		{
			continue;
		}

		std::multimap<S32, S32> attachment_pie_menu_map;

		// gather up all attachment points assigned to this group, and throw into map sorted by pie slice number
		for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
			 iter != mAttachmentPoints.end(); )
		{
			attachment_map_t::iterator curiter = iter++;
			LLViewerJointAttachment* attachment = curiter->second;
			if(attachment->getGroup() == group)
			{
				// use multimap to provide a partial order off of the pie slice key
				S32 pie_index = attachment->getPieSlice();
				attachment_pie_menu_map.insert(std::make_pair(pie_index, curiter->first));
			}
		}

		// add in requested order to pie menu, inserting separators as necessary
		S32 cur_pie_slice = 0;
		for (std::multimap<S32, S32>::iterator attach_it = attachment_pie_menu_map.begin();
			 attach_it != attachment_pie_menu_map.end(); ++attach_it)
		{
			S32 requested_pie_slice = attach_it->first;
			S32 attach_index = attach_it->second;
			while (cur_pie_slice < requested_pie_slice)
			{
				gAttachBodyPartPieMenus[group]->addSeparator();
				gDetachBodyPartPieMenus[group]->addSeparator();
				cur_pie_slice++;
			}

			LLViewerJointAttachment* attachment = get_if_there(mAttachmentPoints, attach_index, (LLViewerJointAttachment*)NULL);
			if (attachment)
			{
				LLMenuItemCallGL::Params item_params;
				item_params.name = attachment->getName();
				item_params.label = attachment->getName();
				item_params.on_click.function_name = "Object.AttachToAvatar";
				item_params.on_click.parameter = attach_index;
				item_params.on_enable.function_name = "Object.EnableWear";
				item_params.on_enable.parameter = attach_index;

				LLMenuItemCallGL* item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);
				gAttachBodyPartPieMenus[group]->addChild(item);
					
				item_params.on_click.function_name = "Attachment.DetachFromPoint";
				item_params.on_click.parameter = attach_index;
				item_params.on_enable.function_name = "Attachment.PointFilled";
				item_params.on_enable.parameter = attach_index;
				item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);
				gDetachBodyPartPieMenus[group]->addChild(item);
				cur_pie_slice++;
			}
		}
	}
	return TRUE;
}

LLVOAvatarSelf::~LLVOAvatarSelf()
{
	gAgent.setAvatarObject(NULL);
	gAgentWearables.setAvatarObject(NULL);
	delete mScreenp;
	mScreenp = NULL;

	for (localtexture_map_t::iterator iter = mLocalTextureDatas.begin();
		 iter != mLocalTextureDatas.end();
		 iter++)
	{
		localtexture_vec_t &local_textures = iter->second;
		for (U32 i = 0; i < local_textures.size(); i++)
		{
			LocalTextureData* loc_tex_data = local_textures[i];
			delete loc_tex_data;
			local_textures[i] = NULL;
		}
	}
}

/**
 **
 ** End LLVOAvatarSelf Constructor routines
 **                                                                             **
 *********************************************************************************/

//virtual
BOOL LLVOAvatarSelf::loadLayersets()
{
	BOOL success = TRUE;
	for (LLVOAvatarXmlInfo::layer_info_list_t::const_iterator iter = sAvatarXmlInfo->mLayerInfoList.begin();
		 iter != sAvatarXmlInfo->mLayerInfoList.end(); 
		 iter++)
	{
		// Construct a layerset for each one specified in avatar_lad.xml and initialize it as such.
		const LLTexLayerSetInfo *info = *iter;
		LLTexLayerSet* layer_set = new LLTexLayerSet( this );
		
		if (!layer_set->setInfo(info))
		{
			stop_glerror();
			delete layer_set;
			llwarns << "avatar file: layer_set->parseData() failed" << llendl;
			return FALSE;
		}

		// scan baked textures and associate the layerset with the appropriate one
		EBakedTextureIndex baked_index = BAKED_NUM_INDICES;
		for (LLVOAvatarDictionary::BakedTextures::const_iterator baked_iter = LLVOAvatarDictionary::getInstance()->getBakedTextures().begin();
			 baked_iter != LLVOAvatarDictionary::getInstance()->getBakedTextures().end();
			 baked_iter++)
		{
			const LLVOAvatarDictionary::BakedEntry *baked_dict = baked_iter->second;
			if (layer_set->isBodyRegion(baked_dict->mName))
			{
				baked_index = baked_iter->first;
				// ensure both structures are aware of each other
				mBakedTextureDatas[baked_index].mTexLayerSet = layer_set;
				layer_set->setBakedTexIndex(baked_index);
				break;
			}
		}
		// if no baked texture was found, warn and cleanup
		if (baked_index == BAKED_NUM_INDICES)
		{
			llwarns << "<layer_set> has invalid body_region attribute" << llendl;
			delete layer_set;
			return FALSE;
		}

		// scan morph masks and let any affected layers know they have an associated morph
		for (LLVOAvatar::morph_list_t::const_iterator morph_iter = mBakedTextureDatas[baked_index].mMaskedMorphs.begin();
			morph_iter != mBakedTextureDatas[baked_index].mMaskedMorphs.end();
			morph_iter++)
		{
			LLMaskedMorph *morph = *morph_iter;
			LLTexLayer * layer = layer_set->findLayerByName(morph->mLayer);
			if (layer)
			{
				layer->setHasMorph(TRUE);
			}
			else
			{
				llwarns << "Could not find layer named " << morph->mLayer << " to set morph flag" << llendl;
				success = FALSE;
			}
		}
	}
	return success;
}
// virtual
BOOL LLVOAvatarSelf::updateCharacter(LLAgent &agent)
{
	LLMemType mt(LLMemType::MTYPE_AVATAR);

	// update screen joint size
	if (mScreenp)
	{
		F32 aspect = LLViewerCamera::getInstance()->getAspect();
		LLVector3 scale(1.f, aspect, 1.f);
		mScreenp->setScale(scale);
		mScreenp->updateWorldMatrixChildren();
		resetHUDAttachments();
	}
	return LLVOAvatar::updateCharacter(agent);
}

// virtual
LLJoint *LLVOAvatarSelf::getJoint(const std::string &name)
{
	if (mScreenp)
	{
		LLJoint* jointp = mScreenp->findJoint(name);
		if (jointp) return jointp;
	}
	return LLVOAvatar::getJoint(name);
}

// virtual
void LLVOAvatarSelf::requestStopMotion(LLMotion* motion)
{
	// Only agent avatars should handle the stop motion notifications.

	// Notify agent that motion has stopped
	gAgent.requestStopMotion(motion);
}

// virtual
void LLVOAvatarSelf::stopMotionFromSource(const LLUUID& source_id)
{
	for (AnimSourceIterator motion_it = mAnimationSources.find(source_id); motion_it != mAnimationSources.end(); )
	{
		gAgent.sendAnimationRequest(motion_it->second, ANIM_REQUEST_STOP);
		mAnimationSources.erase(motion_it++);
	}

	LLViewerObject* object = gObjectList.findObject(source_id);
	if (object)
	{
		object->mFlags &= ~FLAGS_ANIM_SOURCE;
	}
}

// virtual
void LLVOAvatarSelf::setLocalTextureTE(U8 te, LLViewerImage* image, BOOL set_by_user, U32 index)
{
	if (te >= TEX_NUM_INDICES)
	{
		llassert(0);
		return;
	}

	if (getTEImage(te)->getID() == image->getID())
	{
		return;
	}

	if (isIndexBakedTexture((ETextureIndex)te))
	{
		llassert(0);
		return;
	}

	LLTexLayerSet* layer_set = getLayerSet((ETextureIndex)te);
	if (layer_set)
	{
		invalidateComposite(layer_set, set_by_user);
	}

	setTEImage(te, image);
	updateMeshTextures();

	if (gAgent.cameraCustomizeAvatar())
	{
		LLVisualParamHint::requestHintUpdates();
	}
}

//virtual
void LLVOAvatarSelf::removeMissingBakedTextures()
{	
	BOOL removed = FALSE;
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		const S32 te = mBakedTextureDatas[i].mTextureIndex;
		if (getTEImage(te)->isMissingAsset())
		{
			setTEImage(te, gImageList.getImage(IMG_DEFAULT_AVATAR));
			removed = TRUE;
		}
	}

	if (removed)
	{
		for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
		{
			invalidateComposite(mBakedTextureDatas[i].mTexLayerSet, FALSE);
		}
		updateMeshTextures();
		requestLayerSetUploads();
	}
}

//virtual
void LLVOAvatarSelf::updateRegion(LLViewerRegion *regionp)
{
	if (regionp->getHandle() != mLastRegionHandle)
	{
		if (mLastRegionHandle != 0)
		{
			++mRegionCrossingCount;
			F64 delta = (F64)mRegionCrossingTimer.getElapsedTimeF32();
			F64 avg = (mRegionCrossingCount == 1) ? 0 : LLViewerStats::getInstance()->getStat(LLViewerStats::ST_CROSSING_AVG);
			F64 delta_avg = (delta + avg*(mRegionCrossingCount-1)) / mRegionCrossingCount;
			LLViewerStats::getInstance()->setStat(LLViewerStats::ST_CROSSING_AVG, delta_avg);
			
			F64 max = (mRegionCrossingCount == 1) ? 0 : LLViewerStats::getInstance()->getStat(LLViewerStats::ST_CROSSING_MAX);
			max = llmax(delta, max);
			LLViewerStats::getInstance()->setStat(LLViewerStats::ST_CROSSING_MAX, max);
		}
		mLastRegionHandle = regionp->getHandle();
	}
	mRegionCrossingTimer.reset();
}

//--------------------------------------------------------------------
// draw tractor beam when editing objects
//--------------------------------------------------------------------
//virtual
void LLVOAvatarSelf::idleUpdateTractorBeam()
{
	// This is only done for yourself (maybe it should be in the agent?)
	if (!needsRenderBeam() || !mIsBuilt)
	{
		mBeam = NULL;
	}
	else if (!mBeam || mBeam->isDead())
	{
		// VEFFECT: Tractor Beam
		mBeam = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_BEAM);
		mBeam->setColor(LLColor4U(gAgent.getEffectColor()));
		mBeam->setSourceObject(this);
		mBeamTimer.reset();
	}

	if (!mBeam.isNull())
	{
		LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();

		if (gAgent.mPointAt.notNull())
		{
			// get point from pointat effect
			mBeam->setPositionGlobal(gAgent.mPointAt->getPointAtPosGlobal());
			mBeam->triggerLocal();
		}
		else if (selection->getFirstRootObject() && 
				selection->getSelectType() != SELECT_TYPE_HUD)
		{
			LLViewerObject* objectp = selection->getFirstRootObject();
			mBeam->setTargetObject(objectp);
		}
		else
		{
			mBeam->setTargetObject(NULL);
			LLTool *tool = LLToolMgr::getInstance()->getCurrentTool();
			if (tool->isEditing())
			{
				if (tool->getEditingObject())
				{
					mBeam->setTargetObject(tool->getEditingObject());
				}
				else
				{
					mBeam->setPositionGlobal(tool->getEditingPointGlobal());
				}
			}
			else
			{
				const LLPickInfo& pick = gViewerWindow->getLastPick();
				mBeam->setPositionGlobal(pick.mPosGlobal);
			}

		}
		if (mBeamTimer.getElapsedTimeF32() > 0.25f)
		{
			mBeam->setColor(LLColor4U(gAgent.getEffectColor()));
			mBeam->setNeedsSendToSim(TRUE);
			mBeamTimer.reset();
		}
	}
}

//-----------------------------------------------------------------------------
// restoreMeshData()
//-----------------------------------------------------------------------------
// virtual
void LLVOAvatarSelf::restoreMeshData()
{
	LLMemType mt(LLMemType::MTYPE_AVATAR);
	
	//llinfos << "Restoring" << llendl;
	mMeshValid = TRUE;
	updateJointLODs();
	updateAttachmentVisibility(gAgent.getCameraMode());

	// force mesh update as LOD might not have changed to trigger this
	gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_GEOMETRY, TRUE);
}



//-----------------------------------------------------------------------------
// updateAttachmentVisibility()
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::updateAttachmentVisibility(U32 camera_mode)
{
	for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
		 iter != mAttachmentPoints.end(); )
	{
		attachment_map_t::iterator curiter = iter++;
		LLViewerJointAttachment* attachment = curiter->second;
		if (attachment->getIsHUDAttachment())
		{
			attachment->setAttachmentVisibility(TRUE);
		}
		else
		{
			switch (camera_mode)
			{
				case CAMERA_MODE_MOUSELOOK:
					if (LLVOAvatar::sVisibleInFirstPerson && attachment->getVisibleInFirstPerson())
					{
						attachment->setAttachmentVisibility(TRUE);
					}
					else
					{
						attachment->setAttachmentVisibility(FALSE);
					}
					break;
				default:
					attachment->setAttachmentVisibility(TRUE);
					break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// updatedWearable( EWearableType type )
// forces an update to any baked textures relevant to type.
// Should be called only on saving the wearable
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::wearableUpdated( EWearableType type )
{
	for (LLVOAvatarDictionary::BakedTextures::const_iterator baked_iter = LLVOAvatarDictionary::getInstance()->getBakedTextures().begin();
		 baked_iter != LLVOAvatarDictionary::getInstance()->getBakedTextures().end();
		 baked_iter++)
	{
		const LLVOAvatarDictionary::BakedEntry *baked_dict = baked_iter->second;
		const LLVOAvatarDefines::EBakedTextureIndex index = baked_iter->first;
		if (baked_dict)
		{
			for (LLVOAvatarDefines::wearables_vec_t::const_iterator type_iter = baked_dict->mWearables.begin();
				type_iter != baked_dict->mWearables.end();
				type_iter++)
			{
				const EWearableType comp_type = *type_iter;
				if (comp_type == type)
				{
					if (mBakedTextureDatas[index].mTexLayerSet)
					{
						mBakedTextureDatas[index].mTexLayerSet->requestUpdate();
						mBakedTextureDatas[index].mTexLayerSet->requestUpload();
					}
					break;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// isWearingAttachment()
//-----------------------------------------------------------------------------
// Warning: include_linked_items = TRUE makes this operation expensive.
BOOL LLVOAvatarSelf::isWearingAttachment( const LLUUID& inv_item_id , BOOL include_linked_items ) const
{
	for (attachment_map_t::const_iterator iter = mAttachmentPoints.begin(); 
		 iter != mAttachmentPoints.end(); )
	{
		attachment_map_t::const_iterator curiter = iter++;
		const LLViewerJointAttachment* attachment = curiter->second;
		if( attachment->getItemID() == inv_item_id )
		{
			return TRUE;
		}
	}

	if (include_linked_items)
	{
		LLInventoryModel::item_array_t item_array;
		gInventory.collectLinkedItems(inv_item_id, item_array);
		for (LLInventoryModel::item_array_t::const_iterator iter = item_array.begin();
			 iter != item_array.end();
			 iter++)
		{
			const LLViewerInventoryItem *linked_item = (*iter);
			const LLUUID &item_id = linked_item->getUUID();
			for (attachment_map_t::const_iterator iter = mAttachmentPoints.begin(); 
				 iter != mAttachmentPoints.end(); )
			{
				attachment_map_t::const_iterator curiter = iter++;
				const LLViewerJointAttachment* attachment = curiter->second;
				if( attachment->getItemID() == item_id )
				{
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// getWornAttachment()
//-----------------------------------------------------------------------------
LLViewerObject* LLVOAvatarSelf::getWornAttachment( const LLUUID& inv_item_id ) const
{
	for (attachment_map_t::const_iterator iter = mAttachmentPoints.begin(); 
		 iter != mAttachmentPoints.end(); )
	{
		attachment_map_t::const_iterator curiter = iter++;
		const LLViewerJointAttachment* attachment = curiter->second;
		if( attachment->getItemID() == inv_item_id )
		{
			return attachment->getObject();
		}
	}
	return NULL;
}

const std::string LLVOAvatarSelf::getAttachedPointName(const LLUUID& inv_item_id) const
{
	for (attachment_map_t::const_iterator iter = mAttachmentPoints.begin(); 
		 iter != mAttachmentPoints.end(); )
	{
		attachment_map_t::const_iterator curiter = iter++;
		const LLViewerJointAttachment* attachment = curiter->second;
		if( attachment->getItemID() == inv_item_id )
		{
			return attachment->getName();
		}
	}

	return LLStringUtil::null;
}

//virtual
LLViewerJointAttachment *LLVOAvatarSelf::attachObject(LLViewerObject *viewer_object)
{
	LLViewerJointAttachment *attachment = LLVOAvatar::attachObject(viewer_object);
	if (!attachment)
	{
		return 0;
	}

	updateAttachmentVisibility(gAgent.getCameraMode());
	
	// Then make sure the inventory is in sync with the avatar.
	gInventory.addChangedMask(LLInventoryObserver::LABEL, attachment->getItemID());
	gInventory.notifyObservers();

	return attachment;
}

// virtual
void LLVOAvatarSelf::localTextureLoaded(BOOL success, LLViewerImage *src_vi, LLImageRaw* src_raw, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata)
{	
	//llinfos << "onLocalTextureLoaded: " << src_vi->getID() << llendl;

	const LLUUID& src_id = src_vi->getID();
	LLAvatarTexData *data = (LLAvatarTexData *)userdata;
	if (success)
	{
		ETextureIndex index = data->mIndex;
		if (!isIndexLocalTexture(index)) return;
		LocalTextureData *local_tex_data = getLocalTextureData(index,0);
		if (!local_tex_data->mIsBakedReady &&
			local_tex_data->mImage.notNull() &&
			(local_tex_data->mImage->getID() == src_id) &&
			discard_level < local_tex_data->mDiscard)
		{
			local_tex_data->mDiscard = discard_level;
			if (!gAgent.cameraCustomizeAvatar())
			{
				requestLayerSetUpdate(index);
			}
			else if (gAgent.cameraCustomizeAvatar())
			{
				LLVisualParamHint::requestHintUpdates();
			}
			updateMeshTextures();
		}
	}
	else if (final)
	{
		ETextureIndex index = data->mIndex;
		if (!isIndexLocalTexture(index)) return;
		LocalTextureData *local_tex_data = getLocalTextureData(index,0);
		// Failed: asset is missing
		if (!local_tex_data->mIsBakedReady &&
			local_tex_data->mImage.notNull() &&
			local_tex_data->mImage->getID() == src_id)
		{
			local_tex_data->mDiscard = 0;
			requestLayerSetUpdate(index);
			updateMeshTextures();
		}
	}
}

// virtual
/* //unused
BOOL LLVOAvatarSelf::getLocalTextureRaw(ETextureIndex index, LLImageRaw* image_raw) const
{
	if (!isIndexLocalTexture(index)) return FALSE;
	if (getLocalTextureID(index) == IMG_DEFAULT_AVATAR)	return TRUE;

	const LocalTextureData *local_tex_data = getLocalTextureData(index)[0];
	if (local_tex_data->mImage->readBackRaw(-1, image_raw, false))
	{

		return TRUE;
	}
	
	// No data loaded yet
	setLocalTexture((ETextureIndex)index, getTEImage(index), FALSE); // <-- non-const, move this elsewhere
	return FALSE;
}
*/

// virtual
BOOL LLVOAvatarSelf::getLocalTextureGL(ETextureIndex type, LLImageGL** image_gl_pp, U32 index) const
{
	*image_gl_pp = NULL;

	if (!isIndexLocalTexture(type)) return FALSE;
	if (getLocalTextureID(type, index) == IMG_DEFAULT_AVATAR) return TRUE;

	const LocalTextureData *local_tex_data = getLocalTextureData(type, index);
	if (!local_tex_data)
	{
		return FALSE;
	}
	*image_gl_pp = local_tex_data->mImage;
	return TRUE;
}

const LLUUID& LLVOAvatarSelf::getLocalTextureID(ETextureIndex type, U32 index) const
{
	if (!isIndexLocalTexture(type)) return IMG_DEFAULT_AVATAR;

	const LocalTextureData *local_tex_data = getLocalTextureData(type,index);
	if (local_tex_data && local_tex_data->mImage.notNull())
	{
		return local_tex_data->mImage->getID();
	}
	return IMG_DEFAULT_AVATAR;
}

//-----------------------------------------------------------------------------
// isLocalTextureDataAvailable()
// Returns true if at least the lowest quality discard level exists for every texture
// in the layerset.
//-----------------------------------------------------------------------------
BOOL LLVOAvatarSelf::isLocalTextureDataAvailable(const LLTexLayerSet* layerset) const
{
	/* if (layerset == mBakedTextureDatas[BAKED_HEAD].mTexLayerSet)
	   return getLocalDiscardLevel(TEX_HEAD_BODYPAINT) >= 0; */
	for (LLVOAvatarDictionary::BakedTextures::const_iterator baked_iter = LLVOAvatarDictionary::getInstance()->getBakedTextures().begin();
		 baked_iter != LLVOAvatarDictionary::getInstance()->getBakedTextures().end();
		 baked_iter++)
	{
		const EBakedTextureIndex baked_index = baked_iter->first;
		if (layerset == mBakedTextureDatas[baked_index].mTexLayerSet)
		{
			BOOL ret = true;
			const LLVOAvatarDictionary::BakedEntry *baked_dict = baked_iter->second;
			for (texture_vec_t::const_iterator local_tex_iter = baked_dict->mLocalTextures.begin();
				 local_tex_iter != baked_dict->mLocalTextures.end();
				 local_tex_iter++)
			{
				const ETextureIndex tex_index = *local_tex_iter;
				ret &= (getLocalDiscardLevel(tex_index) >= 0);
			}
			return ret;
		}
	}
	llassert(0);
	return FALSE;
}

//-----------------------------------------------------------------------------
// virtual
// isLocalTextureDataFinal()
// Returns true if the highest quality discard level exists for every texture
// in the layerset.
//-----------------------------------------------------------------------------
BOOL LLVOAvatarSelf::isLocalTextureDataFinal(const LLTexLayerSet* layerset) const
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		if (layerset == mBakedTextureDatas[i].mTexLayerSet)
		{
			const LLVOAvatarDictionary::BakedEntry *baked_dict = LLVOAvatarDictionary::getInstance()->getBakedTexture((EBakedTextureIndex)i);
			for (texture_vec_t::const_iterator local_tex_iter = baked_dict->mLocalTextures.begin();
				 local_tex_iter != baked_dict->mLocalTextures.end();
				 local_tex_iter++)
			{
				if (getLocalDiscardLevel(*local_tex_iter) != 0)
				{
					return FALSE;
				}
			}
			return TRUE;
		}
	}
	llassert(0);
	return FALSE;
}

BOOL LLVOAvatarSelf::isTextureDefined(LLVOAvatarDefines::ETextureIndex type, U32 index) const
{
	LLUUID id;
	if (isIndexLocalTexture(type))
	{
		id = getLocalTextureID(type, index);
	}
	else
	{
		id = getTEImage(type)->getID();
	}

	return (id != IMG_DEFAULT_AVATAR && 
			id != IMG_DEFAULT);
}

//-----------------------------------------------------------------------------
// virtual
// requestLayerSetUploads()
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::requestLayerSetUploads()
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		BOOL layer_baked = isTextureDefined(mBakedTextureDatas[i].mTextureIndex);
		if (!layer_baked && mBakedTextureDatas[i].mTexLayerSet)
		{
			mBakedTextureDatas[i].mTexLayerSet->requestUpload();
		}
	}
}

bool LLVOAvatarSelf::areTexturesCurrent() const
{
	return !hasPendingBakedUploads() && gAgentWearables.areWearablesLoaded();
}

// virtual
bool LLVOAvatarSelf::hasPendingBakedUploads() const
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		BOOL upload_pending = (mBakedTextureDatas[i].mTexLayerSet && mBakedTextureDatas[i].mTexLayerSet->getComposite()->uploadPending());
		if (upload_pending)
		{
			return true;
		}
	}
	return false;
}

void LLVOAvatarSelf::invalidateComposite( LLTexLayerSet* layerset, BOOL set_by_user )
{
	if( !layerset || !layerset->getUpdatesEnabled() )
	{
		return;
	}
	// llinfos << "LLVOAvatar::invalidComposite() " << layerset->getBodyRegion() << llendl;

	invalidateMorphMasks(layerset->getBakedTexIndex());
	layerset->requestUpdate();

	if( set_by_user )
	{
		llassert(isSelf());

		ETextureIndex baked_te = getBakedTE( layerset );
		setTEImage( baked_te, gImageList.getImage(IMG_DEFAULT_AVATAR) );
		layerset->requestUpload();
	}
}

void LLVOAvatarSelf::invalidateAll()
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		invalidateComposite(mBakedTextureDatas[i].mTexLayerSet, TRUE);
	}
	updateMeshTextures();
}

//-----------------------------------------------------------------------------
// setCompositeUpdatesEnabled()
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::setCompositeUpdatesEnabled( BOOL b )
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		if (mBakedTextureDatas[i].mTexLayerSet )
		{
			mBakedTextureDatas[i].mTexLayerSet->setUpdatesEnabled( b );
		}
	}
}

void LLVOAvatarSelf::setupComposites()
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		BOOL layer_baked = isTextureDefined(mBakedTextureDatas[i].mTextureIndex);
		if (mBakedTextureDatas[i].mTexLayerSet)
		{
			mBakedTextureDatas[i].mTexLayerSet->setUpdatesEnabled(!layer_baked);
		}
	}
}

void LLVOAvatarSelf::updateComposites()
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		if (mBakedTextureDatas[i].mTexLayerSet 
			&& ((i != BAKED_SKIRT) || isWearingWearableType(WT_SKIRT)))
		{
			mBakedTextureDatas[i].mTexLayerSet->updateComposite();
		}
	}
}

// virtual
S32 LLVOAvatarSelf::getLocalDiscardLevel(ETextureIndex type, U32 index) const
{
	if (!isIndexLocalTexture(type)) return FALSE;

	const LocalTextureData *local_tex_data = getLocalTextureData(type,index);
	if (local_tex_data)
	{
		if (type >= 0
			&& getLocalTextureID(type,index) != IMG_DEFAULT_AVATAR
			&& !local_tex_data->mImage->isMissingAsset())
		{
			return local_tex_data->mImage->getDiscardLevel();
		}
		else
		{
			// We don't care about this (no image associated with the layer) treat as fully loaded.
			return 0;
		}
	}
	return 0;
}

// virtual
// Counts the memory footprint of local textures.
void LLVOAvatarSelf::getLocalTextureByteCount(S32* gl_bytes) const
{
	*gl_bytes = 0;
	for (S32 type = 0; type < TEX_NUM_INDICES; type++)
	{
		if (!isIndexLocalTexture((ETextureIndex)type)) continue;
		const localtexture_vec_t & local_tex_vec = getLocalTextureData((ETextureIndex)type);
		for (U32 num = 0; num < local_tex_vec.size(); num++)
		{
			const LocalTextureData *local_tex_data = local_tex_vec[num];
			if (local_tex_data)
			{
				const LLViewerImage* image_gl = local_tex_data->mImage;
				if (image_gl)
				{
					S32 bytes = (S32)image_gl->getWidth() * image_gl->getHeight() * image_gl->getComponents();
					
					if (image_gl->getHasGLTexture())
					{
						*gl_bytes += bytes;
					}
				}
			}
		}
	}
}

// virtual 
void LLVOAvatarSelf::setLocalTexture(ETextureIndex type, LLViewerImage* tex, BOOL baked_version_ready, U32 index)
{
	if (!isIndexLocalTexture(type)) return;

	S32 desired_discard = isSelf() ? 0 : 2;
	LocalTextureData *local_tex_data = getLocalTextureData(type,index);
	if (!baked_version_ready)
	{
		if (tex != local_tex_data->mImage || local_tex_data->mIsBakedReady)
		{
			local_tex_data->mDiscard = MAX_DISCARD_LEVEL+1;
		}
		if (tex->getID() != IMG_DEFAULT_AVATAR)
		{
			if (local_tex_data->mDiscard > desired_discard)
			{
				S32 tex_discard = tex->getDiscardLevel();
				if (tex_discard >= 0 && tex_discard <= desired_discard)
				{
					local_tex_data->mDiscard = tex_discard;
					if (isSelf() && !gAgent.cameraCustomizeAvatar())
					{
						requestLayerSetUpdate(type);
					}
					else if (isSelf() && gAgent.cameraCustomizeAvatar())
					{
						LLVisualParamHint::requestHintUpdates();
					}
				}
				else
				{
					tex->setLoadedCallback(onLocalTextureLoaded, desired_discard, TRUE, FALSE, new LLAvatarTexData(getID(), type));
				}
			}
			tex->setMinDiscardLevel(desired_discard);
		}
	}
	local_tex_data->mIsBakedReady = baked_version_ready;
	local_tex_data->mImage = tex;
}

// virtual
void LLVOAvatarSelf::dumpLocalTextures() const
{
	llinfos << "Local Textures:" << llendl;

	/* ETextureIndex baked_equiv[] = {
	   TEX_UPPER_BAKED,
	   if (isTextureDefined(baked_equiv[i])) */
	for (LLVOAvatarDictionary::Textures::const_iterator iter = LLVOAvatarDictionary::getInstance()->getTextures().begin();
		 iter != LLVOAvatarDictionary::getInstance()->getTextures().end();
		 iter++)
	{
		const LLVOAvatarDictionary::TextureEntry *texture_dict = iter->second;
		if (!texture_dict->mIsLocalTexture || !texture_dict->mIsUsedByBakedTexture)
			continue;

		const EBakedTextureIndex baked_index = texture_dict->mBakedTextureIndex;
		const ETextureIndex baked_equiv = LLVOAvatarDictionary::getInstance()->getBakedTexture(baked_index)->mTextureIndex;

		const std::string &name = texture_dict->mName;
		const LocalTextureData *local_tex_data = getLocalTextureData(iter->first,0);
		if (isTextureDefined(baked_equiv))
		{
#if LL_RELEASE_FOR_DOWNLOAD
			// End users don't get to trivially see avatar texture IDs, makes textures
			// easier to steal. JC
			llinfos << "LocTex " << name << ": Baked " << llendl;
#else
			llinfos << "LocTex " << name << ": Baked " << getTEImage(baked_equiv)->getID() << llendl;
#endif
		}
		else if (local_tex_data->mImage.notNull())
		{
			if (local_tex_data->mImage->getID() == IMG_DEFAULT_AVATAR)
			{
				llinfos << "LocTex " << name << ": None" << llendl;
			}
			else
			{
				const LLViewerImage* image = local_tex_data->mImage;

				llinfos << "LocTex " << name << ": "
						<< "Discard " << image->getDiscardLevel() << ", "
						<< "(" << image->getWidth() << ", " << image->getHeight() << ") " 
#if !LL_RELEASE_FOR_DOWNLOAD
					// End users don't get to trivially see avatar texture IDs,
					// makes textures easier to steal
						<< image->getID() << " "
#endif
						<< "Priority: " << image->getDecodePriority()
						<< llendl;
			}
		}
		else
		{
			llinfos << "LocTex " << name << ": No LLViewerImage" << llendl;
		}
	}
}

//-----------------------------------------------------------------------------
// static 
// onLocalTextureLoaded()
//-----------------------------------------------------------------------------

void LLVOAvatarSelf::onLocalTextureLoaded(BOOL success, LLViewerImage *src_vi, LLImageRaw* src_raw, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata)
{
	LLAvatarTexData *data = (LLAvatarTexData *)userdata;
	LLVOAvatarSelf *self = (LLVOAvatarSelf *)gObjectList.findObject(data->mAvatarID);
	if (self)
	{
		// We should only be handling local textures for ourself
		self->localTextureLoaded(success, src_vi, src_raw, aux_src, discard_level, final, userdata);
	}
	// ensure data is cleaned up
	if (final || !success)
	{
		delete data;
	}
}


const LLVOAvatarSelf::localtexture_vec_t &LLVOAvatarSelf::getLocalTextureData(LLVOAvatarDefines::ETextureIndex type) const
{
	localtexture_map_t::const_iterator found_localtexture = mLocalTextureDatas.find(type);
	if (found_localtexture != mLocalTextureDatas.end())
	{
		const localtexture_vec_t &local_tex_data = found_localtexture->second;
		return local_tex_data;
	}
	llassert(0);
	static localtexture_vec_t ret;
	return ret;
}

const LocalTextureData*	LLVOAvatarSelf::getLocalTextureData(LLVOAvatarDefines::ETextureIndex type, U32 index ) const
{
	const localtexture_vec_t &local_tex_array = getLocalTextureData(type);

	if (index >= local_tex_array.size())
	{
		return NULL;
	}

	return local_tex_array[index];
}

LLVOAvatarSelf::localtexture_vec_t &LLVOAvatarSelf::getLocalTextureData(LLVOAvatarDefines::ETextureIndex type)
{
	localtexture_map_t::iterator found_localtexture = mLocalTextureDatas.find(type);
	if (found_localtexture != mLocalTextureDatas.end())
	{
		localtexture_vec_t &local_tex_data = found_localtexture->second;
		return local_tex_data;
	}
	llassert(0);
	static localtexture_vec_t ret;
	return ret;
}

LocalTextureData*	LLVOAvatarSelf::getLocalTextureData(LLVOAvatarDefines::ETextureIndex type, U32 index )
{
	localtexture_vec_t &local_tex_array = getLocalTextureData(type);

	if (index >= local_tex_array.size())
	{
		return NULL;
	}

	return local_tex_array[index];
}

// static
void LLVOAvatarSelf::dumpTotalLocalTextureByteCount()
{
	S32 gl_bytes = 0;
	gAgent.getAvatarObject()->getLocalTextureByteCount(&gl_bytes);
	llinfos << "Total Avatar LocTex GL:" << (gl_bytes/1024) << "KB" << llendl;
}

BOOL LLVOAvatarSelf::updateIsFullyLoaded()
{
	BOOL loading = FALSE;

	// do we have a shape?
	if (visualParamWeightsAreDefault())
	{
		loading = TRUE;
	}

	if (!isTextureDefined(TEX_HAIR))
	{
		loading = TRUE;
	}

	if (!mPreviousFullyLoaded)
	{
		if (!isLocalTextureDataAvailable(mBakedTextureDatas[BAKED_LOWER].mTexLayerSet) &&
			(!isTextureDefined(TEX_LOWER_BAKED)))
		{
			loading = TRUE;
		}

		if (!isLocalTextureDataAvailable(mBakedTextureDatas[BAKED_UPPER].mTexLayerSet) &&
			(!isTextureDefined(TEX_UPPER_BAKED)))
		{
			loading = TRUE;
		}

		for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
		{
			if (i == BAKED_SKIRT && !isWearingWearableType(WT_SKIRT))
				continue;

			BakedTextureData& texture_data = mBakedTextureDatas[i];
			if (!isTextureDefined(texture_data.mTextureIndex))
				continue;

			// Check for the case that texture is defined but not sufficiently loaded to display anything.
			LLViewerImage* baked_img = getImage( texture_data.mTextureIndex );
			if (!baked_img || !baked_img->getHasGLTexture())
			{
				loading = TRUE;
			}

		}

	}
	return processFullyLoadedChange(loading);
}

const LLUUID& LLVOAvatarSelf::grabLocalTexture(ETextureIndex type, U32 index) const
{
	if (canGrabLocalTexture(type, index))
	{
		return getTEImage( type )->getID();
	}
	return LLUUID::null;
}

BOOL LLVOAvatarSelf::canGrabLocalTexture(ETextureIndex type, U32 index) const
{
	// Check if the texture hasn't been baked yet.
	if (!isTextureDefined(type))
	{
		lldebugs << "getTEImage( " << (U32) type << " )->getID() == IMG_DEFAULT_AVATAR" << llendl;
		return FALSE;
	}

	if (gAgent.isGodlike())
		return TRUE;

	// Check permissions of textures that show up in the
	// baked texture.  We don't want people copying people's
	// work via baked textures.
	/* switch(type)
		case TEX_EYES_BAKED:
			textures.push_back(TEX_EYES_IRIS); */
	const LLVOAvatarDictionary::TextureEntry *texture_dict = LLVOAvatarDictionary::getInstance()->getTexture(type);
	if (!texture_dict->mIsUsedByBakedTexture) return FALSE;

	const EBakedTextureIndex baked_index = texture_dict->mBakedTextureIndex;
	const LLVOAvatarDictionary::BakedEntry *baked_dict = LLVOAvatarDictionary::getInstance()->getBakedTexture(baked_index);
	for (texture_vec_t::const_iterator iter = baked_dict->mLocalTextures.begin();
		 iter != baked_dict->mLocalTextures.end();
		 iter++)
	{
		const ETextureIndex t_index = (*iter);
		lldebugs << "Checking index " << (U32) t_index << llendl;
		const LLUUID& texture_id = getTEImage( t_index )->getID();
		if (texture_id != IMG_DEFAULT_AVATAR)
		{
			// Search inventory for this texture.
			LLViewerInventoryCategory::cat_array_t cats;
			LLViewerInventoryItem::item_array_t items;
			LLAssetIDMatches asset_id_matches(texture_id);
			gInventory.collectDescendentsIf(LLUUID::null,
											cats,
											items,
											LLInventoryModel::INCLUDE_TRASH,
											asset_id_matches);

			BOOL can_grab = FALSE;
			lldebugs << "item count for asset " << texture_id << ": " << items.count() << llendl;
			if (items.count())
			{
				// search for full permissions version
				for (S32 i = 0; i < items.count(); i++)
				{
					LLInventoryItem* itemp = items[i];
					LLPermissions item_permissions = itemp->getPermissions();
					if ( item_permissions.allowOperationBy(
								PERM_MODIFY, gAgent.getID(), gAgent.getGroupID()) &&
						 item_permissions.allowOperationBy(
								PERM_COPY, gAgent.getID(), gAgent.getGroupID()) &&
						 item_permissions.allowOperationBy(
								PERM_TRANSFER, gAgent.getID(), gAgent.getGroupID()) )
					{
						can_grab = TRUE;
						break;
					}
				}
			}
			if (!can_grab) return FALSE;
		}
	}

	return TRUE;
}

void LLVOAvatarSelf::addLocalTextureStats( ETextureIndex type, LLViewerImage* imagep,
										   F32 texel_area_ratio, BOOL render_avatar, BOOL covered_by_baked, U32 index )
{
	if (!isIndexLocalTexture(type)) return;

	if (!covered_by_baked)
	{
		if (getLocalTextureID(type, index) != IMG_DEFAULT_AVATAR)
		{
			F32 desired_pixels;
			desired_pixels = llmin(mPixelArea, (F32)getTexImageArea());
			imagep->setBoostLevel(getAvatarBoostLevel());
			imagep->addTextureStats( desired_pixels / texel_area_ratio );
			if (imagep->getDiscardLevel() < 0)
			{
				mHasGrey = TRUE; // for statistics gathering
			}
		}
		else
		{
			// texture asset is missing
			mHasGrey = TRUE; // for statistics gathering
		}
	}
}

//-----------------------------------------------------------------------------
// getBakedTE()
// Used by the LayerSet.  (Layer sets don't in general know what textures depend on them.)
//-----------------------------------------------------------------------------
ETextureIndex LLVOAvatarSelf::getBakedTE( const LLTexLayerSet* layerset ) const
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		if (layerset == mBakedTextureDatas[i].mTexLayerSet )
		{
			return mBakedTextureDatas[i].mTextureIndex;
		}
	}
	llassert(0);
	return TEX_HEAD_BAKED;
}


//-----------------------------------------------------------------------------
// setNewBakedTexture()
// A new baked texture has been successfully uploaded and we can start using it now.
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::setNewBakedTexture( ETextureIndex te, const LLUUID& uuid )
{
	// Baked textures live on other sims.
	LLHost target_host = getObjectHost();	
	setTEImage( te, gImageList.getImageFromHost( uuid, target_host ) );
	updateMeshTextures();
	dirtyMesh();

	LLVOAvatar::cullAvatarsByPixelArea();

	/* switch(te)
		case TEX_HEAD_BAKED:
			llinfos << "New baked texture: HEAD" << llendl; */
	const LLVOAvatarDictionary::TextureEntry *texture_dict = LLVOAvatarDictionary::getInstance()->getTexture(te);
	if (texture_dict->mIsBakedTexture)
	{
		llinfos << "New baked texture: " << texture_dict->mName << " UUID: " << uuid <<llendl;
	}
	else
	{
		llwarns << "New baked texture: unknown te " << te << llendl;
	}
	
	//	dumpAvatarTEs( "setNewBakedTexture() send" );
	// RN: throttle uploads
	if (!hasPendingBakedUploads())
	{
		gAgent.sendAgentSetAppearance();
	}
}

//-----------------------------------------------------------------------------
// setCachedBakedTexture()
// A baked texture id was received from a cache query, make it active
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::setCachedBakedTexture( ETextureIndex te, const LLUUID& uuid )
{
	setTETexture( te, uuid );

	/* switch(te)
		case TEX_HEAD_BAKED:
			if( mHeadLayerSet )
				mHeadLayerSet->cancelUpload(); */
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		if ( mBakedTextureDatas[i].mTextureIndex == te && mBakedTextureDatas[i].mTexLayerSet)
		{
			mBakedTextureDatas[i].mTexLayerSet->cancelUpload();
		}
	}
}

// static
void LLVOAvatarSelf::processRebakeAvatarTextures(LLMessageSystem* msg, void**)
{
	LLUUID texture_id;
	msg->getUUID("TextureData", "TextureID", texture_id);

	LLVOAvatarSelf* self = gAgent.getAvatarObject();
	if (!self) return;

	// If this is a texture corresponding to one of our baked entries, 
	// just rebake that layer set.
	BOOL found = FALSE;

	/* ETextureIndex baked_texture_indices[BAKED_NUM_INDICES] =
			TEX_HEAD_BAKED,
			TEX_UPPER_BAKED, */
	for (LLVOAvatarDictionary::Textures::const_iterator iter = LLVOAvatarDictionary::getInstance()->getTextures().begin();
		 iter != LLVOAvatarDictionary::getInstance()->getTextures().end();
		 iter++)
	{
		const ETextureIndex index = iter->first;
		const LLVOAvatarDictionary::TextureEntry *texture_dict = iter->second;
		if (texture_dict->mIsBakedTexture)
		{
			if (texture_id == self->getTEImage(index)->getID())
			{
				LLTexLayerSet* layer_set = self->getLayerSet(index);
				if (layer_set)
				{
					llinfos << "TAT: rebake - matched entry " << (S32)index << llendl;
					// Apparently set_by_user == force upload
					BOOL set_by_user = TRUE;
					self->invalidateComposite(layer_set, set_by_user);
					found = TRUE;
					LLViewerStats::getInstance()->incStat(LLViewerStats::ST_TEX_REBAKES);
				}
			}
		}
	}

	// If texture not found, rebake all entries.
	if (!found)
	{
		self->forceBakeAllTextures();
	}
	else
	{
		// Not sure if this is necessary, but forceBakeAllTextures() does it.
		self->updateMeshTextures();
	}
}

void LLVOAvatarSelf::forceBakeAllTextures(bool slam_for_debug)
{
	llinfos << "TAT: forced full rebake. " << llendl;

	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		ETextureIndex baked_index = mBakedTextureDatas[i].mTextureIndex;
		LLTexLayerSet* layer_set = getLayerSet(baked_index);
		if (layer_set)
		{
			if (slam_for_debug)
			{
				layer_set->setUpdatesEnabled(TRUE);
				layer_set->cancelUpload();
			}

			BOOL set_by_user = TRUE;
			invalidateComposite(layer_set, set_by_user);
			LLViewerStats::getInstance()->incStat(LLViewerStats::ST_TEX_REBAKES);
		}
		else
		{
			llwarns << "TAT: NO LAYER SET FOR " << (S32)baked_index << llendl;
		}
	}

	// Don't know if this is needed
	updateMeshTextures();
}

//-----------------------------------------------------------------------------
// requestLayerSetUpdate()
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::requestLayerSetUpdate(ETextureIndex index )
{
	/* switch(index)
		case LOCTEX_UPPER_BODYPAINT:  
		case LOCTEX_UPPER_SHIRT:
			if( mUpperBodyLayerSet )
				mUpperBodyLayerSet->requestUpdate(); */
	const LLVOAvatarDictionary::TextureEntry *texture_dict = LLVOAvatarDictionary::getInstance()->getTexture(index);
	if (!texture_dict->mIsLocalTexture || !texture_dict->mIsUsedByBakedTexture)
		return;
	const EBakedTextureIndex baked_index = texture_dict->mBakedTextureIndex;
	if (mBakedTextureDatas[baked_index].mTexLayerSet)
	{
		mBakedTextureDatas[baked_index].mTexLayerSet->requestUpdate();
	}
}

LLTexLayerSet* LLVOAvatarSelf::getLayerSet(ETextureIndex index) const
{
	/* switch(index)
		case TEX_HEAD_BAKED:
		case TEX_HEAD_BODYPAINT:
			return mHeadLayerSet; */
	const LLVOAvatarDictionary::TextureEntry *texture_dict = LLVOAvatarDictionary::getInstance()->getTexture(index);
	if (texture_dict->mIsUsedByBakedTexture)
	{
		const EBakedTextureIndex baked_index = texture_dict->mBakedTextureIndex;
		return mBakedTextureDatas[baked_index].mTexLayerSet;
	}
	return NULL;
}

// static
void LLVOAvatarSelf::onCustomizeStart()
{
	// We're no longer doing any baking or invalidating on entering 
	// appearance editing mode. Leaving function in place in case 
	// further changes require us to do something at this point - Nyx
}

// static
void LLVOAvatarSelf::onCustomizeEnd()
{
	LLVOAvatarSelf *avatarp = gAgent.getAvatarObject();
	if (avatarp)
	{
		avatarp->invalidateAll();
		avatarp->requestLayerSetUploads();
	}
}

// static
void LLVOAvatarSelf::onChangeSelfInvisible(BOOL newvalue)
{
	LLVOAvatarSelf *avatarp = gAgent.getAvatarObject();
	if (avatarp)
	{
		if (newvalue)
		{
			// we have just requested to set the avatar's baked textures to invisible
			avatarp->setInvisible(TRUE);
		}
		else
		{
			avatarp->setInvisible(FALSE);
		}
	}
}

void LLVOAvatarSelf::setInvisible(BOOL newvalue)
{
	if (newvalue)
	{
		setCompositeUpdatesEnabled(FALSE);
		for (U32 i = 0; i < mBakedTextureDatas.size(); i++ )
		{
			setNewBakedTexture(mBakedTextureDatas[i].mTextureIndex, IMG_INVISIBLE);
		}
		gAgent.sendAgentSetAppearance();
	}
	else
	{
		setCompositeUpdatesEnabled(TRUE);
		invalidateAll();
		requestLayerSetUploads();
		gAgent.sendAgentSetAppearance();
	}
}

//------------------------------------------------------------------------
// needsRenderBeam()
//------------------------------------------------------------------------
BOOL LLVOAvatarSelf::needsRenderBeam()
{
	if (gNoRender)
	{
		return FALSE;
	}
	LLTool *tool = LLToolMgr::getInstance()->getCurrentTool();

	BOOL is_touching_or_grabbing = (tool == LLToolGrab::getInstance() && LLToolGrab::getInstance()->isEditing());
	if (LLToolGrab::getInstance()->getEditingObject() && 
		LLToolGrab::getInstance()->getEditingObject()->isAttachment())
	{
		// don't render selection beam on hud objects
		is_touching_or_grabbing = FALSE;
	}
	return is_touching_or_grabbing || (mState & AGENT_STATE_EDITING && LLSelectMgr::getInstance()->shouldShowSelection());
}

// static
void LLVOAvatarSelf::deleteScratchTextures()
{
	for( LLGLuint* namep = sScratchTexNames.getFirstData(); 
		 namep; 
		 namep = sScratchTexNames.getNextData() )
	{
		LLImageGL::deleteTextures(1, (U32 *)namep );
		stop_glerror();
	}

	if( sScratchTexBytes )
	{
		lldebugs << "Clearing Scratch Textures " << (sScratchTexBytes/1024) << "KB" << llendl;

		sScratchTexNames.deleteAllData();
		sScratchTexLastBindTime.deleteAllData();
		LLImageGL::sGlobalTextureMemoryInBytes -= sScratchTexBytes;
		sScratchTexBytes = 0;
	}
}

BOOL LLVOAvatarSelf::bindScratchTexture( LLGLenum format )
{
	U32 texture_bytes = 0;
	GLuint gl_name = getScratchTexName( format, &texture_bytes );
	if( gl_name )
	{
		gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, gl_name);
		stop_glerror();

		F32* last_bind_time = sScratchTexLastBindTime.getIfThere( format );
		if( last_bind_time )
		{
			if( *last_bind_time != LLImageGL::sLastFrameTime )
			{
				*last_bind_time = LLImageGL::sLastFrameTime;
				LLImageGL::updateBoundTexMem(texture_bytes);
			}
		}
		else
		{
			LLImageGL::updateBoundTexMem(texture_bytes);
			sScratchTexLastBindTime.addData( format, new F32(LLImageGL::sLastFrameTime) );
		}
		return TRUE;
	}
	return FALSE;
}

LLGLuint LLVOAvatarSelf::getScratchTexName( LLGLenum format, U32* texture_bytes )
{
	S32 components;
	GLenum internal_format;
	switch( format )
	{
		case GL_LUMINANCE:			components = 1; internal_format = GL_LUMINANCE8;		break;
		case GL_ALPHA:				components = 1; internal_format = GL_ALPHA8;			break;
		case GL_COLOR_INDEX:		components = 1; internal_format = GL_COLOR_INDEX8_EXT;	break;
		case GL_LUMINANCE_ALPHA:	components = 2; internal_format = GL_LUMINANCE8_ALPHA8;	break;
		case GL_RGB:				components = 3; internal_format = GL_RGB8;				break;
		case GL_RGBA:				components = 4; internal_format = GL_RGBA8;				break;
		default:	llassert(0);	components = 4; internal_format = GL_RGBA8;				break;
	}

	*texture_bytes = components * SCRATCH_TEX_WIDTH * SCRATCH_TEX_HEIGHT;
	
	if( sScratchTexNames.checkData( format ) )
	{
		return *( sScratchTexNames.getData( format ) );
	}

	LLGLSUIDefault gls_ui;

	U32 name = 0;
	LLImageGL::generateTextures(1, &name );
	stop_glerror();

	gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, name);
	stop_glerror();

	LLImageGL::setManualImage(
		GL_TEXTURE_2D, 0, internal_format, 
		SCRATCH_TEX_WIDTH, SCRATCH_TEX_HEIGHT,
		format, GL_UNSIGNED_BYTE, NULL );
	stop_glerror();

	gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
	gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
	stop_glerror();

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	stop_glerror();

	sScratchTexNames.addData( format, new LLGLuint( name ) );

	sScratchTexBytes += *texture_bytes;
	LLImageGL::sGlobalTextureMemoryInBytes += *texture_bytes;
	return name;
}

// static 
void LLVOAvatarSelf::dumpScratchTextureByteCount()
{
	llinfos << "Scratch Texture GL: " << (sScratchTexBytes/1024) << "KB" << llendl;
}
