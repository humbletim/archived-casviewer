/**
 * @file llpanelplaceprofile.cpp
 * @brief Displays place profile in Side Tray.
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llpanelplaceprofile.h"

#include "llparcel.h"

#include "llqueryflags.h"

#include "lliconctrl.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "lltexteditor.h"

#include "llaccordionctrl.h"
#include "llaccordionctrltab.h"
#include "llagent.h"
#include "llagentui.h"
#include "llappviewer.h"
#include "llcallbacklist.h"
#include "llfloaterbuycurrency.h"
#include "llstatusbar.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"

static LLRegisterPanelClassWrapper<LLPanelPlaceProfile> t_place_profile("panel_place_profile");

LLPanelPlaceProfile::LLPanelPlaceProfile()
:	LLPanelPlaceInfo(),
	mForSalePanel(NULL),
	mYouAreHerePanel(NULL),
	mSelectedParcelID(-1)
{}

// virtual
LLPanelPlaceProfile::~LLPanelPlaceProfile()
{}

// virtual
BOOL LLPanelPlaceProfile::postBuild()
{
	LLPanelPlaceInfo::postBuild();

	mForSalePanel = getChild<LLPanel>("for_sale_panel");
	mYouAreHerePanel = getChild<LLPanel>("here_panel");
	gIdleCallbacks.addFunction(&LLPanelPlaceProfile::updateYouAreHereBanner, this);

	//Icon value should contain sale price of last selected parcel.
	mForSalePanel->getChild<LLIconCtrl>("icon_for_sale")->
				setMouseDownCallback(boost::bind(&LLPanelPlaceProfile::onForSaleBannerClick, this));

	mParcelOwner = getChild<LLTextBox>("owner_value");
	mLastVisited = getChild<LLTextBox>("last_visited_value");

	mRatingText = getChild<LLTextBox>("rating_value");
	mVoiceText = getChild<LLTextBox>("voice_value");
	mFlyText = getChild<LLTextBox>("fly_value");
	mPushText = getChild<LLTextBox>("push_value");
	mBuildText = getChild<LLTextBox>("build_value");
	mScriptsText = getChild<LLTextBox>("scripts_value");
	mDamageText = getChild<LLTextBox>("damage_value");

	mRegionNameText = getChild<LLTextBox>("region_name");
	mRegionTypeText = getChild<LLTextBox>("region_type");
	mRegionRatingText = getChild<LLTextBox>("region_rating");
	mRegionOwnerText = getChild<LLTextBox>("region_owner");
	mRegionGroupText = getChild<LLTextBox>("region_group");

	mEstateNameText = getChild<LLTextBox>("estate_name");
	mEstateRatingText = getChild<LLTextBox>("estate_rating");
	mEstateOwnerText = getChild<LLTextBox>("estate_owner");
	mCovenantText = getChild<LLTextEditor>("covenant");

	mSalesPriceText = getChild<LLTextBox>("sales_price");
	mAreaText = getChild<LLTextBox>("area");
	mTrafficText = getChild<LLTextBox>("traffic");
	mPrimitivesText = getChild<LLTextBox>("primitives");
	mParcelScriptsText = getChild<LLTextBox>("parcel_scripts");
	mTerraformLimitsText = getChild<LLTextBox>("terraform_limits");
	mSubdivideText = getChild<LLTextEditor>("subdivide");
	mResaleText = getChild<LLTextEditor>("resale");
	mSaleToText = getChild<LLTextBox>("sale_to");

	return TRUE;
}

// virtual
void LLPanelPlaceProfile::resetLocation()
{
	LLPanelPlaceInfo::resetLocation();

	mForSalePanel->setVisible(FALSE);
	mYouAreHerePanel->setVisible(FALSE);

	std::string not_available = getString("not_available");
	mParcelOwner->setValue(not_available);
	mLastVisited->setValue(not_available);

	mRatingText->setText(not_available);
	mVoiceText->setText(not_available);
	mFlyText->setText(not_available);
	mPushText->setText(not_available);
	mBuildText->setText(not_available);
	mParcelScriptsText->setText(not_available);
	mDamageText->setText(not_available);

	mRegionNameText->setValue(not_available);
	mRegionTypeText->setValue(not_available);
	mRegionRatingText->setValue(not_available);
	mRegionOwnerText->setValue(not_available);
	mRegionGroupText->setValue(not_available);

	mEstateNameText->setValue(not_available);
	mEstateRatingText->setValue(not_available);
	mEstateOwnerText->setValue(not_available);
	mCovenantText->setValue(not_available);

	mSalesPriceText->setValue(not_available);
	mAreaText->setValue(not_available);
	mTrafficText->setValue(not_available);
	mPrimitivesText->setValue(not_available);
	mParcelScriptsText->setValue(not_available);
	mTerraformLimitsText->setValue(not_available);
	mSubdivideText->setValue(not_available);
	mResaleText->setValue(not_available);
	mSaleToText->setValue(not_available);
}

// virtual
void LLPanelPlaceProfile::setInfoType(INFO_TYPE type)
{
	bool is_info_type_agent = type == AGENT;
	bool is_info_type_teleport_history = type == TELEPORT_HISTORY;

	getChild<LLTextBox>("maturity_label")->setVisible(!is_info_type_agent);
	mMaturityRatingText->setVisible(!is_info_type_agent);

	getChild<LLTextBox>("owner_label")->setVisible(is_info_type_agent);
	mParcelOwner->setVisible(is_info_type_agent);

	getChild<LLTextBox>("last_visited_label")->setVisible(is_info_type_teleport_history);
	mLastVisited->setVisible(is_info_type_teleport_history);

	getChild<LLAccordionCtrl>("advanced_info_accordion")->setVisible(is_info_type_agent);

	switch(type)
	{
		case AGENT:
		case PLACE:
		default:
			mCurrentTitle = getString("title_place");
		break;

		case TELEPORT_HISTORY:
			mCurrentTitle = getString("title_teleport_history");
		break;
	}

	LLPanelPlaceInfo::setInfoType(type);
}

// virtual
void LLPanelPlaceProfile::processParcelInfo(const LLParcelData& parcel_data)
{
	LLPanelPlaceInfo::processParcelInfo(parcel_data);

	// HACK: Flag 0x2 == adult region,
	// Flag 0x1 == mature region, otherwise assume PG
	std::string rating = LLViewerRegion::accessToString(SIM_ACCESS_PG);
	if (parcel_data.flags & 0x2)
	{
		rating = LLViewerRegion::accessToString(SIM_ACCESS_ADULT);
	}
	else if (parcel_data.flags & 0x1)
	{
		rating = LLViewerRegion::accessToString(SIM_ACCESS_MATURE);
	}

	mMaturityRatingText->setValue(rating);
	mRatingText->setValue(rating);

	//update for_sale banner, here we should use DFQ_FOR_SALE instead of PF_FOR_SALE
	//because we deal with remote parcel response format
	bool is_for_sale = (parcel_data.flags & DFQ_FOR_SALE) &&
					 mInfoType == AGENT ? TRUE : FALSE;
	mForSalePanel->setVisible(is_for_sale);
}

void LLPanelPlaceProfile::displaySelectedParcelInfo(LLParcel* parcel,
													LLViewerRegion* region,
													const LLVector3d& pos_global,
													bool is_current_parcel)
{
	if (!region || !parcel)
		return;

	// send EstateCovenantInfo message
	LLMessageSystem *msg = gMessageSystem;
	msg->newMessage("EstateCovenantRequest");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID,	gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
	msg->sendReliable(region->getHost());

	LLParcelData parcel_data;

	// HACK: Converting sim access flags to the format
	// returned by remote parcel response.
	switch(region->getSimAccess())
	{
	case SIM_ACCESS_MATURE:
		parcel_data.flags = 0x1;
		break;

	case SIM_ACCESS_ADULT:
		parcel_data.flags = 0x2;
		break;

	default:
		parcel_data.flags = 0;
	}
	parcel_data.desc = parcel->getDesc();
	parcel_data.name = parcel->getName();
	parcel_data.sim_name = region->getName();
	parcel_data.snapshot_id = parcel->getSnapshotID();
	mPosRegion.setVec((F32)fmod(pos_global.mdV[VX], (F64)REGION_WIDTH_METERS),
					  (F32)fmod(pos_global.mdV[VY], (F64)REGION_WIDTH_METERS),
					  (F32)pos_global.mdV[VZ]);
	parcel_data.global_x = pos_global.mdV[VX];
	parcel_data.global_y = pos_global.mdV[VY];
	parcel_data.global_z = pos_global.mdV[VZ];

	std::string on = getString("on");
	std::string off = getString("off");

	// Processing parcel characteristics
	if (parcel->getParcelFlagAllowVoice())
	{
		mVoiceText->setText(on);
	}
	else
	{
		mVoiceText->setText(off);
	}

	if (!region->getBlockFly() && parcel->getAllowFly())
	{
		mFlyText->setText(on);
	}
	else
	{
		mFlyText->setText(off);
	}

	if (region->getRestrictPushObject() || parcel->getRestrictPushObject())
	{
		mPushText->setText(off);
	}
	else
	{
		mPushText->setText(on);
	}

	if (parcel->getAllowModify())
	{
		mBuildText->setText(on);
	}
	else
	{
		mBuildText->setText(off);
	}

	if((region->getRegionFlags() & REGION_FLAGS_SKIP_SCRIPTS) ||
	   (region->getRegionFlags() & REGION_FLAGS_ESTATE_SKIP_SCRIPTS) ||
	   !parcel->getAllowOtherScripts())
	{
		mScriptsText->setText(off);
	}
	else
	{
		mScriptsText->setText(on);
	}

	if (region->getAllowDamage() || parcel->getAllowDamage())
	{
		mDamageText->setText(on);
	}
	else
	{
		mDamageText->setText(off);
	}

	mRegionNameText->setText(region->getName());
	mRegionTypeText->setText(region->getSimProductName());
	mRegionRatingText->setText(region->getSimAccessString());

	// Determine parcel owner
	if (parcel->isPublic())
	{
		mParcelOwner->setText(getString("public"));
		mRegionOwnerText->setText(getString("public"));
	}
	else
	{
		if (parcel->getIsGroupOwned())
		{
			mRegionOwnerText->setText(getString("group_owned_text"));

			if(!parcel->getGroupID().isNull())
			{
				// FIXME: Using parcel group as region group.
				gCacheName->get(parcel->getGroupID(), TRUE,
								boost::bind(&LLPanelPlaceInfo::nameUpdatedCallback, mRegionGroupText, _2, _3));

				gCacheName->get(parcel->getGroupID(), TRUE,
								boost::bind(&LLPanelPlaceInfo::nameUpdatedCallback, mParcelOwner, _2, _3));
			}
			else
			{
				std::string owner = getString("none_text");
				mRegionGroupText->setText(owner);
				mParcelOwner->setText(owner);
			}
		}
		else
		{
			// Figure out the owner's name
			gCacheName->get(parcel->getOwnerID(), FALSE,
							boost::bind(&LLPanelPlaceInfo::nameUpdatedCallback, mParcelOwner, _2, _3));
			gCacheName->get(region->getOwner(), FALSE,
							boost::bind(&LLPanelPlaceInfo::nameUpdatedCallback, mRegionOwnerText, _2, _3));
		}

		if(LLParcel::OS_LEASE_PENDING == parcel->getOwnershipStatus())
		{
			mRegionOwnerText->setText(mRegionOwnerText->getText() + getString("sale_pending_text"));
		}
	}

	mEstateRatingText->setText(region->getSimAccessString());

	S32 area;
	S32 claim_price;
	S32 rent_price;
	F32 dwell;
	BOOL for_sale = parcel->getForSale();
	LLViewerParcelMgr::getInstance()->getDisplayInfo(&area,
													 &claim_price,
													 &rent_price,
													 &for_sale,
													 &dwell);
	if (for_sale)
	{
		// Adding "For Sale" flag in remote parcel response format.
		parcel_data.flags |= DFQ_FOR_SALE;

		const LLUUID& auth_buyer_id = parcel->getAuthorizedBuyerID();
		if(auth_buyer_id.notNull())
		{
			gCacheName->get(auth_buyer_id, TRUE,
							boost::bind(&LLPanelPlaceInfo::nameUpdatedCallback, mSaleToText, _2, _3));

			// Show sales info to a specific person or a group he belongs to.
			if (auth_buyer_id != gAgent.getID() && !gAgent.isInGroup(auth_buyer_id))
			{
				for_sale = FALSE;
			}
		}
		else
		{
			mSaleToText->setText(getString("anyone"));
		}

		const U8* sign = (U8*)getString("price_text").c_str();
		const U8* sqm = (U8*)getString("area_text").c_str();

		mSalesPriceText->setText(llformat("%s%d ", sign, parcel->getSalePrice()));
		mAreaText->setText(llformat("%d %s", area, sqm));
		mTrafficText->setText(llformat("%.0f", dwell));

		// Can't have more than region max tasks, regardless of parcel
		// object bonus factor.
		S32 primitives = llmin(llround(parcel->getMaxPrimCapacity() * parcel->getParcelPrimBonus()),
							   (S32)region->getMaxTasks());

		const U8* available = (U8*)getString("available").c_str();
		const U8* allocated = (U8*)getString("allocated").c_str();

		mPrimitivesText->setText(llformat("%d %s, %d %s", primitives, available, parcel->getPrimCount(), allocated));

		if (parcel->getAllowOtherScripts())
		{
			mParcelScriptsText->setText(getString("all_residents_text"));
		}
		else if (parcel->getAllowGroupScripts())
		{
			mParcelScriptsText->setText(getString("group_text"));
		}
		else
		{
			mParcelScriptsText->setText(off);
		}

		mTerraformLimitsText->setText(parcel->getAllowTerraform() ? on : off);

		if (region->getRegionFlags() & REGION_FLAGS_ALLOW_PARCEL_CHANGES)
		{
			mSubdivideText->setText(getString("can_change"));
		}
		else
		{
			mSubdivideText->setText(getString("can_not_change"));
		}
		if (region->getRegionFlags() & REGION_FLAGS_BLOCK_LAND_RESELL)
		{
			mResaleText->setText(getString("can_not_resell"));
		}
		else
		{
			mResaleText->setText(getString("can_resell"));
		}
	}

	mSelectedParcelID = parcel->getLocalID();
	mLastSelectedRegionID = region->getRegionID();
	processParcelInfo(parcel_data);

	mYouAreHerePanel->setVisible(is_current_parcel);
	getChild<LLAccordionCtrlTab>("sales_tab")->setVisible(for_sale);
}

void LLPanelPlaceProfile::updateEstateName(const std::string& name)
{
	mEstateNameText->setText(name);
}

void LLPanelPlaceProfile::updateEstateOwnerName(const std::string& name)
{
	mEstateOwnerText->setText(name);
}

void LLPanelPlaceProfile::updateCovenantText(const std::string &text)
{
	mCovenantText->setText(text);
}

void LLPanelPlaceProfile::updateLastVisitedText(const LLDate &date)
{
	if (date.isNull())
	{
		mLastVisited->setText(getString("unknown"));
	}
	else
	{
		std::string timeStr = getString("acquired_date");
		LLSD substitution;
		substitution["datetime"] = (S32) date.secondsSinceEpoch();
		LLStringUtil::format (timeStr, substitution);
		mLastVisited->setText(timeStr);
	}
}

void LLPanelPlaceProfile::onForSaleBannerClick()
{
	LLViewerParcelMgr* mgr = LLViewerParcelMgr::getInstance();
	LLParcelSelectionHandle hParcel = mgr->getFloatingParcelSelection();
	LLViewerRegion* selected_region =  mgr->getSelectionRegion();
	if(!hParcel.isNull() && selected_region)
	{
		if(hParcel->getParcel()->getLocalID() == mSelectedParcelID &&
				mLastSelectedRegionID ==selected_region->getRegionID())
		{
			if(hParcel->getParcel()->getSalePrice() - gStatusBar->getBalance() > 0)
			{
				LLFloaterBuyCurrency::buyCurrency("Buying selected land ", hParcel->getParcel()->getSalePrice());
			}
			else
			{
				LLViewerParcelMgr::getInstance()->startBuyLand();
			}
		}
		else
		{
			LL_WARNS("Places") << "User  is trying  to buy remote parcel.Operation is not supported"<< LL_ENDL;
		}

	}
}

// static
void LLPanelPlaceProfile::updateYouAreHereBanner(void* userdata)
{
	//YouAreHere Banner should be displayed only for selected places,
	// If you want to display it for landmark or teleport history item, you should check by mParcelId

	LLPanelPlaceProfile* self = static_cast<LLPanelPlaceProfile*>(userdata);
	if(!self->getVisible())
		return;

	if(!gDisconnected)
	{
		static F32 radius = gSavedSettings.getF32("YouAreHereDistance");

		BOOL display_banner = gAgent.getRegion()->getRegionID() == self->mLastSelectedRegionID &&
										LLAgentUI::checkAgentDistance(self->mPosRegion, radius);

		self->mYouAreHerePanel->setVisible(display_banner);
	}
}
