/*
 * @file fspanelcontactsets.h
 * @brief Contact sets UI defintions
 *
 * (C) 2013 Cinder Roxley @ Second Life <cinder.roxley@phoenixviewer.com>
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef FS_PANELCONTACTSETS_H
#define FS_PANELCONTACTSETS_H

#include "lggcontactsets.h"
#include "llavatarlist.h"
#include "llcombobox.h"
#include "llpanel.h"

#include <boost/signals2.hpp>

class FSPanelContactSets : public LLPanel
{
public:
	FSPanelContactSets();
	BOOL postBuild();
	void refreshSetList();
	
private:
	~FSPanelContactSets();
	
	void onSelectAvatar();
	void generateAvatarList(const std::string& contact_set);
	void onClickAddAvatar();
	void handlePickerCallback(const uuid_vec_t& ids, const std::string& set);
	void onClickRemoveAvatar();
	void onClickOpenProfile();
	void onClickStartIM();
	void onClickOfferTeleport();
	void onClickAddSet();
	void onClickRemoveSet();
	void onClickConfigureSet();
	void onClickRemoveDisplayName();
	void onClickSetPseudonym();
	void onClickRemovePseudonym();
	
	void refreshContactSets();
	void removeAvatarFromSet();
	void resetControls();
	
	void updateSets(LGGContactSets::EContactSetUpdate type);
	boost::signals2::connection mContactSetChangedConnection;
	
	uuid_vec_t mAvatarSelections;
	
	LLComboBox* mContactSetCombo;
	LLAvatarList* mAvatarList;
};

#endif // FS_PANELCONTACTSETS_H
