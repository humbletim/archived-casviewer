/** 
 * @file llfloaterpreference.h
 * @brief LLPreferenceCore class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

/*
 * App-wide preferences.  Note that these are not per-user,
 * because we need to load many preferences before we have
 * a login name.
 */

#ifndef LL_LLFLOATERPREFERENCE_H
#define LL_LLFLOATERPREFERENCE_H

#include "llfloater.h"
#include "llavatarpropertiesprocessor.h"
#include "llconversationlog.h"
#include "lllineeditor.h" // <FS:CR>
#include "llsearcheditor.h"

namespace nd
{
	namespace prefs
	{
		struct SearchData;
	}
}

class LLConversationLogObserver;
class LLPanelPreference;
class LLPanelLCD;
class LLPanelDebug;
class LLMessageSystem;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLSD;
class LLTextBox;
class LLComboBox;

typedef std::map<std::string, std::string> notifications_map;

typedef enum
	{
		GS_LOW_GRAPHICS,
		GS_MID_GRAPHICS,
		GS_HIGH_GRAPHICS,
		GS_ULTRA_GRAPHICS
		
	} EGraphicsSettings;


// Floater to control preferences (display, audio, bandwidth, general.
class LLFloaterPreference : public LLFloater, public LLAvatarPropertiesObserver, public LLConversationLogObserver
{
public: 
	LLFloaterPreference(const LLSD& key);
	~LLFloaterPreference();

	void apply();
	void cancel();
	/*virtual*/ void draw();
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& key);
	/*virtual*/	void onClose(bool app_quitting);
	/*virtual*/ void changed();
	/*virtual*/ void changed(const LLUUID& session_id, U32 mask) {};

	// static data update, called from message handler
	// <FS:Ansariel> Show email address in preferences (FIRE-1071)
	//static void updateUserInfo(const std::string& visibility, bool im_via_email);
	static void updateUserInfo(const std::string& visibility, bool im_via_email, const std::string& email);

	// refresh all the graphics preferences menus
	static void refreshEnabledGraphics();
	
	// translate user's do not disturb response message according to current locale if message is default, otherwise do nothing
	static void initDoNotDisturbResponse();

	void processProperties( void* pData, EAvatarProcessorType type );
	void processProfileProperties(const LLAvatarData* pAvatarData );
	void storeAvatarProperties( const LLAvatarData* pAvatarData );
	void saveAvatarProperties( void );
	void selectPrivacyPanel();
	void selectChatPanel();

// <FS:CR> Make onBtnOk() public for settings backup panel
//protected:
	void		onBtnOK();
protected:
// </FS:CR>
	void		onBtnCancel();
	void		onBtnApply();

	//void		onClickClearCache();			// Clear viewer texture cache, vfs, and VO cache on next startup // AO: was protected, moved to public
	void		onClickBrowserClearCache();		// Clear web history and caches as well as viewer caches above
	void		onLanguageChange();
	void		onNotificationsChange(const std::string& OptionName);
	void		onNameTagOpacityChange(const LLSD& newvalue);
	void		onConsoleOpacityChange(const LLSD& newvalue);	// <FS:CR> FIRE-1332 - Sepeate opacity settings for nametag and console chat

	// set value of "DoNotDisturbResponseChanged" in account settings depending on whether do not disturb response
	// <FS:Zi> Pie menu
	// make sure controls get greyed out or enabled when pie color override is toggled
	void onPieColorsOverrideChanged();
	// </FS:Zi> Pie menu

	// string differs from default after user changes.
	void onDoNotDisturbResponseChanged();
	// if the custom settings box is clicked
	void onChangeCustom();
	void updateMeterText(LLUICtrl* ctrl);
	void onOpenHardwareSettings();
	// callback for defaults
	void setHardwareDefaults();
	// callback for when client turns on shaders
	void onVertexShaderEnable();
	// <FS:AO> callback for local lights toggle
	void onLocalLightsEnable();

	// callback for commit in the "Single click on land" and "Double click on land" comboboxes.
	void onClickActionChange();
	// updates click/double-click action settings depending on controls values
	void updateClickActionSettings();
	// updates click/double-click action controls depending on values from settings.xml
	void updateClickActionControls();
	// <FS:PP> updates UI Sounds controls depending on values from settings.xml
	void updateUISoundsControls();
	
	// <FS:Zi> Optional Edit Appearance Lighting
	// make sure controls get greyed out or enabled when appearance camera movement is toggled
	void onAppearanceCameraChanged();
	// </FS:Zi> Optional Edit Appearance Lighting

	//<FS:Kadah> Font Selection
	void populateFontSelectionCombo();
	void loadFontPresetsFromDir(const std::string& dir, LLComboBox* font_selection_combo);
	//</FS:Kadah>
    
	// This function squirrels away the current values of the controls so that
	// cancel() can restore them.	
	void saveSettings();

public:

	void setCacheLocation(const LLStringExplicit& location);
	// <FS:Ansariel> Sound cache
	void setSoundCacheLocation(const LLStringExplicit& location);
	void onClickSetSoundCache();
	void onClickBrowseSoundCache();
	void onClickResetSoundCache();
	// </FS:Ansariel>

	// <FS:Ansariel> FIRE-2912: Reset voice button
	void onClickResetVoice();

	void onClickSetCache();
	void onClickBrowseCache();
	void onClickBrowseCrashLogs();
	void onClickBrowseChatLogDir();
	void onClickResetCache();
	void onClickClearCache(); // AO: was protected, moved to public
	void onClickCookies();
	void onClickJavascript();
	void onClickBrowseSettingsDir();
	void onClickSkin(LLUICtrl* ctrl,const LLSD& userdata);
	void onSelectSkin();
	void onClickSetKey();
	void setKey(KEY key);
	void onClickSetMiddleMouse();
	// void onClickSetSounds();	//<FS:KC> Handled centrally now
	void onClickPreviewUISound(const LLSD& ui_sound_id); // <FS:PP> FIRE-8190: Preview function for "UI Sounds" Panel
	void setPreprocInclude();
	void onClickEnablePopup();
	void onClickDisablePopup();	
	void resetAllIgnored();
	void setAllIgnored();
	void onClickLogPath();
	bool moveTranscriptsAndLog();
	//[FIX FIRE-2765 : SJ] Making sure Reset button resets works
	void onClickResetLogPath();
	void enableHistory();
	// <FS:Ansariel> Show email address in preferences (FIRE-1071)
	//void setPersonalInfo(const std::string& visibility, bool im_via_email);
	void setPersonalInfo(const std::string& visibility, bool im_via_email, const std::string& email);
	// </FS:Ansariel> Show email address in preferences (FIRE-1071)
	void refreshEnabledState();
	void disableUnavailableSettings();
	void onCommitWindowedMode();
	void refresh();	// Refresh enable/disable
	// if the quality radio buttons are changed
	void onChangeQuality(const LLSD& data);
	void onClickClearSettings();
	void onClickChatOnlineNotices();
	void onClickClearSpamList();
	//void callback_clear_settings(const LLSD& notification, const LLSD& response);
	// <FS:Ansariel> Clear inventory cache button
	void onClickInventoryClearCache();
	// <FS:Ansariel> Clear web browser cache button
	void onClickWebBrowserClearCache();
	
	void updateSliderText(LLSliderCtrl* ctrl, LLTextBox* text_box);
	void refreshUI();

	void onCommitParcelMediaAutoPlayEnable();
	void onCommitMediaEnabled();
	void onCommitMusicEnabled();
	void applyResolution();
	void onChangeMaturity();
	void onClickBlockList();
	void onClickProxySettings();
	void onClickTranslationSettings();
	void onClickAutoReplace();
	void onClickSpellChecker();
	void applyUIColor(LLUICtrl* ctrl, const LLSD& param);
	void getUIColor(LLUICtrl* ctrl, const LLSD& param);
	void onLogChatHistorySaved();	
	void buildPopupLists();
	static void refreshSkin(void* data);
	void selectPanel(const LLSD& name);


	// <CV:David>
	void onChangeWalkSpeed();
	// </CV:David>

	// <CV:David>
	void onChangeOutputType();
	void onClickResetEyeSeparation();
	void onClickResetScreenDistance();
	void onChangeRiftOperationMode();
	void onRiftStrafeEnable();
	void onRiftHeadReorientsEnable();
	void onChangeRiftHeadReorientsAfter();
	void onClickResetRiftHeadReorientsAfter();
	void onChangeRiftHeadReorientsSpeed();
	void onClickResetRiftHeadReorientsSpeed();
	void onClickResetRiftUIDepth();
	void onClickResetRiftFOVMultiplier();
	void onClickResetRiftPixelDensity();
	void onChangeRiftMouseMode();
	void onRiftMouseHorizontalEnable();
	void onRiftHmdSettingsChanged();
	// </CV:David>

	// <CV:David>
	#if LL_WINDOWS
		void onKinectEnable();
		void onClickResetKinectSensitivity();
		void onKinectSwapFlyUpAndFlyDown();
	#endif
	// </CV:David>

private:

	void onDeleteTranscripts();
	void onDeleteTranscriptsResponse(const LLSD& notification, const LLSD& response);
	void updateDeleteTranscriptsButton();

	static std::string sSkin;
	notifications_map mNotificationOptions;
	bool mClickActionDirty; ///< Set to true when the click/double-click options get changed by user.
	bool mGotPersonalInfo;
	bool mOriginalIMViaEmail;
	bool mLanguageChanged;
	bool mAvatarDataInitialized;
	std::string mPriorInstantMessageLogPath;
	
	bool mOriginalHideOnlineStatus;
	std::string mDirectoryVisibility;
	
	LLAvatarData mAvatarProperties;

	LLSearchEditor *mFilterEdit;
	void onUpdateFilterTerm(bool force = false);

	nd::prefs::SearchData *mSearchData;
	void collectSearchableItems();
};

class LLPanelPreference : public LLPanel
{
public:
	LLPanelPreference();
	/*virtual*/ BOOL postBuild();
	
	virtual ~LLPanelPreference();

	virtual void apply();
	virtual void cancel();
	// void setControlFalse(const LLSD& user_data);	//<FS:KC> Handled centrally now
	virtual void setHardwareDefaults(){};

	// Disables "Allow Media to auto play" check box only when both
	// "Streaming Music" and "Media" are unchecked. Otherwise enables it.
	void updateMediaAutoPlayCheckbox(LLUICtrl* ctrl);

	// This function squirrels away the current values of the controls so that
	// cancel() can restore them.
	virtual void saveSettings();
	
	class Updater;

protected:
	typedef std::map<LLControlVariable*, LLSD> control_values_map_t;
	control_values_map_t mSavedValues;

private:
	//for "Only friends and groups can call or IM me"
	static void showFriendsOnlyWarning(LLUICtrl*, const LLSD&);

	static void showCustomPortWarning(LLUICtrl*, const LLSD&); // -WoLf

 	//for "Show my Favorite Landmarks at Login"
	static void showFavoritesOnLoginWarning(LLUICtrl* checkbox, const LLSD& value);

	// <FS:Ansariel> Only enable Growl checkboxes if Growl is usable
	void onEnableGrowlChanged();
	// <FS:Ansariel> Flash chat toolbar button notification
	void onChatWindowChanged();
	// <FS:Ansariel> Exodus' mouselook combat feature
	void updateMouselookCombatFeatures();

	// <FS:Ansariel> Minimap pick radius transparency
	void updateMapPickRadiusTransparency(const LLSD& value);
	F32 mOriginalMapPickRadiusTransparency;

	typedef std::map<std::string, LLColor4> string_color_map_t;
	string_color_map_t mSavedColors;

	//<FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
	//Updater* mBandWidthUpdater;
};

class LLPanelPreferenceGraphics : public LLPanelPreference
{
public:
	BOOL postBuild();
	void draw();
	void apply();
	void cancel();
	void saveSettings();
	void setHardwareDefaults();
protected:
	bool hasDirtyChilds();
	void resetDirtyChilds();
	

	LLButton*	mButtonApply;
};

// [SL:KB] - Catznip Viewer-Skins
class LLPanelPreferenceSkins : public LLPanelPreference
{
public:
	LLPanelPreferenceSkins();
	
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void apply();
	/*virtual*/ void cancel();
	void callbackRestart(const LLSD& notification, const LLSD& response);	// <FS:CR> Callback for restart dialogs
protected:
	void onSkinChanged();
	void onSkinThemeChanged();
	void refreshSkinList();
	void refreshSkinThemeList();
	void refreshPreviewImage(); // <FS:PP> FIRE-1689: Skins preview image
	void showSkinChangeNotification();
	
protected:
	std::string m_Skin;
	LLComboBox* m_pSkinCombo;
	std::string m_SkinTheme;
	LLComboBox* m_pSkinThemeCombo;
	LLButton*	m_pSkinPreview; // <FS:PP> FIRE-1689: Skins preview image
	LLSD        m_SkinsInfo;
	std::string	m_SkinName;
	std::string	m_SkinThemeName;
};
// [/SL:KB]

// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2010-10-21 (Catznip-2.6.0a) | Added: Catznip-2.2.0c
class LLPanelPreferenceCrashReports : public LLPanelPreference
{
public:
	LLPanelPreferenceCrashReports();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void apply();
	/*virtual*/ void cancel();

	void refresh();
};
// [/SL:KB]

// <FS:CR> Settings Backup
class FSPanelPreferenceBackup : public LLPanelPreference
{
public:
	FSPanelPreferenceBackup();
	/*virtual*/ BOOL postBuild();
	
protected:
	// <FS:Zi> Backup settings
	void onClickSetBackupSettingsPath();
	void onClickSelectAll();
	void onClickDeselectAll();
	void onClickBackupSettings();
	void onClickRestoreSettings();
	
	void doSelect(BOOL all);												// calls applySelection for each list
	void applySelection(LLScrollListCtrl* control, BOOL all);				// selects or deselects all items in a scroll list
	void doRestoreSettings(const LLSD& notification, const LLSD& response);	// callback for restore dialog
	void onQuitConfirmed(const LLSD& notification, const LLSD& response);	// callback for finished restore dialog
	// </FS:Zi>
};

#ifdef OPENSIM // <FS:AW optional opensim support>
// <FS:AW  opensim preferences>
class LLPanelPreferenceOpensim : public LLPanelPreference
{
public:
	LLPanelPreferenceOpensim();
// <FS:AW  grid management>
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void apply();
	/*virtual*/ void cancel();

protected:

	void onClickAddGrid();
	void addedGrid(bool success);
	void onClickClearGrid();
	void onClickRefreshGrid();
	void onClickSaveGrid();
	void onClickRemoveGrid();
	void onSelectGrid();
	bool removeGridCB(const LLSD& notification, const LLSD& response);
// </FS:AW  grid management>
// <FS:AW  opensim search support>
	void onClickClearDebugSearchURL();
	void onClickPickDebugSearchURL();
// </FS:AW  opensim search support>

	void refreshGridList(bool success = true);
	LLScrollListCtrl* mGridListControl;
private:
	LLLineEditor* mEditorGridName;
	LLLineEditor* mEditorGridURI;
	LLLineEditor* mEditorLoginPage;
	LLLineEditor* mEditorHelperURI;
	LLLineEditor* mEditorWebsite;
	LLLineEditor* mEditorSupport;
	LLLineEditor* mEditorRegister;
	LLLineEditor* mEditorPassword;
	LLLineEditor* mEditorSearch;
	LLLineEditor* mEditorGridMessage;
};
// </FS:AW  opensim preferences>
#endif // OPENSIM // <FS:AW optional opensim support>

class LLFloaterPreferenceProxy : public LLFloater
{
public: 
	LLFloaterPreferenceProxy(const LLSD& key);
	~LLFloaterPreferenceProxy();

	/// show off our menu
	static void show();
	void cancel();
	
protected:
	BOOL postBuild();
	void onOpen(const LLSD& key);
	void onClose(bool app_quitting);
	void saveSettings();
	void onBtnOk();
	void onBtnCancel();

	void onChangeSocksSettings();

private:
	
	bool mSocksSettingsDirty;
	typedef std::map<LLControlVariable*, LLSD> control_values_map_t;
	control_values_map_t mSavedValues;
};


#endif  // LL_LLPREFERENCEFLOATER_H
