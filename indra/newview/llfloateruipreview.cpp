/**
 * @file llfloateruipreview.cpp
 * @brief Tool for previewing and editing floaters, plus localization tool integration
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

// Tool for previewing floaters and panels for localization and UI design purposes.
// See: https://wiki.lindenlab.com/wiki/GUI_Preview_And_Localization_Tools
// See: https://jira.lindenlab.com/browse/DEV-16869

// *TODO: Translate error messgaes using notifications/alerts.xml

#include "llviewerprecompiledheaders.h"	// Precompiled headers

#include "llfloateruipreview.h"			// Own header

// Internal utility
#include "llrender.h"
#include "llsdutil.h"
#include "llxmltree.h"
#include "llviewerwindow.h"

// XUI
#include "lluictrlfactory.h"
#include "llcombobox.h"
#include "llnotifications.h"
#include "llresizebar.h"
#include "llscrolllistitem.h"
#include "llscrolllistctrl.h"
#include "llfilepicker.h"
#include "lldraghandle.h"
#include "lllayoutstack.h"
#include "lltooltip.h"
#include "llviewermenu.h"
#include "llrngwriter.h"
#include "llfloater.h"			// superclass
#include "llfloaterreg.h"
#include "llscrollcontainer.h"	// scroll container for overlapping elements
#include "lllivefile.h"					// live file poll/stat/reload

// Boost (for linux/unix command-line execv)
#include <boost/tokenizer.hpp>
#include <boost/shared_ptr.hpp>

// External utility
#include <string>
#include <list>
#include <map>

#if LL_DARWIN
#include <CoreFoundation/CFURL.h>
#endif

// Static initialization
static const S32 PRIMARY_FLOATER = 1;
static const S32 SECONDARY_FLOATER = 2;

class LLOverlapPanel;
static LLDefaultChildRegistry::Register<LLOverlapPanel> register_overlap_panel("overlap_panel");

static std::string get_xui_dir()
{
	std::string delim = gDirUtilp->getDirDelimiter();
	return gDirUtilp->getSkinBaseDir() + delim + "default" + delim + "xui" + delim;
}

// Forward declarations to avoid header dependencies
class LLEventTimer;
class LLColor;
class LLScrollListCtrl;
class LLComboBox;
class LLButton;
class LLLineEditor;
class LLXmlTreeNode;
class LLFloaterUIPreview;
class LLFadeEventTimer;

class LLLocalizationResetForcer;
class LLGUIPreviewLiveFile;
class LLFadeEventTimer;
class LLPreviewedFloater;

// Implementation of custom overlapping element display panel
class LLOverlapPanel : public LLPanel
{
public:
	struct Params : public LLInitParam::Block<Params, LLPanel::Params>
	{
		Params() {}
	};
	LLOverlapPanel(Params p = Params()) : LLPanel(p),
		mSpacing(10),
		// mClickedElement(NULL),
		mLastClickedElement(NULL)
	{
		mOriginalWidth = getRect().getWidth();
		mOriginalHeight = getRect().getHeight();
	}
	virtual void draw();
	
	typedef std::map<LLView*, std::list<LLView*> >	OverlapMap;
	OverlapMap mOverlapMap;						// map, of XUI element to a list of XUI elements it overlaps
	
	// LLView *mClickedElement;
	LLView *mLastClickedElement;
	int mOriginalWidth, mOriginalHeight, mSpacing;
};


class LLFloaterUIPreview : public LLFloater
{
public:
	// Setup
	LLFloaterUIPreview(const LLSD& key);
	virtual ~LLFloaterUIPreview();

	std::string getLocStr(S32 ID);							// fetches the localization string based on what is selected in the drop-down menu
	void displayFloater(BOOL click, S32 ID, bool save = false);			// needs to be public so live file can call it when it finds an update

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onClose(bool app_quitting);

	void refreshList();										// refresh list (empty it out and fill it up from scratch)
	void addFloaterEntry(const std::string& path);			// add a single file's entry to the list of floaters
	
	static BOOL containerType(LLView* viewp);				// check if the element is a container type and tree traverses need to look at its children
	
public:	
	LLPreviewedFloater*			mDisplayedFloater;			// the floater which is currently being displayed
	LLPreviewedFloater*			mDisplayedFloater_2;			// the floater which is currently being displayed
	LLGUIPreviewLiveFile*		mLiveFile;					// live file for checking for updates to the currently-displayed XML file
	LLOverlapPanel*				mOverlapPanel;				// custom overlapping elements panel
	// BOOL						mHighlightingDiffs;			// bool for whether localization diffs are being highlighted or not
	BOOL						mHighlightingOverlaps;		// bool for whether overlapping elements are being highlighted

	// typedef std::map<std::string,std::pair<std::list<std::string>,std::list<std::string> > > DiffMap; // this version copies the lists etc., and thus is bad memory-wise
	typedef std::list<std::string> StringList;
	typedef boost::shared_ptr<StringList> StringListPtr;
	typedef std::map<std::string, std::pair<StringListPtr,StringListPtr> > DiffMap;
	DiffMap mDiffsMap;							// map, of filename to pair of list of changed element paths and list of errors

private:
	// XUI elements for this floater
	LLScrollListCtrl*			mFileList;							// scroll list control for file list
	LLLineEditor*				mEditorPathTextBox;					// text field for path to editor executable
	LLLineEditor*				mEditorArgsTextBox;					// text field for arguments to editor executable
	LLLineEditor*				mDiffPathTextBox;					// text field for path to diff file
	LLButton*					mDisplayFloaterBtn;					// button to display primary floater
	LLButton*					mDisplayFloaterBtn_2;				// button to display secondary floater
	LLButton*					mEditFloaterBtn;					// button to edit floater
	LLButton*					mExecutableBrowseButton;			// button to browse for executable
	LLButton*					mCloseOtherButton;					// button to close primary displayed floater
	LLButton*					mCloseOtherButton_2;				// button to close secondary displayed floater
	LLButton*					mDiffBrowseButton;					// button to browse for diff file
	LLButton*					mToggleHighlightButton;				// button to toggle highlight of files/elements with diffs
	LLButton*					mToggleOverlapButton;				// button to togle overlap panel/highlighting
	LLComboBox*					mLanguageSelection;					// combo box for primary language selection
	LLComboBox*					mLanguageSelection_2;				// combo box for secondary language selection
	LLScrollContainer*			mOverlapScrollView;					// overlapping elements scroll container
	S32							mLastDisplayedX, mLastDisplayedY;	// stored position of last floater so the new one opens up in the same place
	std::string 				mDelim;								// the OS-specific delimiter character (/ or \) (*TODO: this shouldn't be needed, right?)

	std::string					mSavedEditorPath;					// stored editor path so closing this floater doesn't reset it
	std::string					mSavedEditorArgs;					// stored editor args so closing this floater doesn't reset it
	std::string					mSavedDiffPath;						// stored diff file path so closing this floater doesn't reset it

	// Internal functionality
	static void popupAndPrintWarning(std::string& warning);			// pop up a warning
	std::string getLocalizedDirectory();							// build and return the path to the XUI directory for the currently-selected localization
	void scanDiffFile(LLXmlTreeNode* file_node);					// scan a given XML node for diff entries and highlight them in its associated file
	void highlightChangedElements();								// look up the list of elements to highlight and highlight them in the current floater
	void highlightChangedFiles();									// look up the list of changed files to highlight and highlight them in the scroll list
	void findOverlapsInChildren(LLView* parent);					// fill the map below with element overlap information
	static BOOL overlapIgnorable(LLView* viewp);					// check it the element can be ignored for overlap/localization purposes

	// check if two elements overlap using their rectangles
	// used instead of llrect functions because by adding a few pixels of leeway I can cut down drastically on the number of overlaps
	BOOL elementOverlap(LLView* view1, LLView* view2);

	// Button/drop-down action listeners (self explanatory)
	void onClickDisplayFloater(S32 id);
	void onClickSaveFloater(S32 id);
	void onClickSaveAll(S32 id);
	void onClickEditFloater();
	void onClickBrowseForEditor();
	void onClickBrowseForDiffs();
	void onClickToggleDiffHighlighting();
	void onClickToggleOverlapping();
	void onClickCloseDisplayedFloater(S32 id);
	void onLanguageComboSelect(LLUICtrl* ctrl);
	void onClickExportSchema();
	void onClickShowRectangles(const LLSD& data);
};

//----------------------------------------------------------------------------
// Local class declarations
// Reset object to ensure that when we change the current language setting for preview purposes,
// it automatically is reset.  Constructed on the stack at the start of the method; the reset
// occurs as it falls out of scope at the end of the method.  See llfloateruipreview.cpp for usage.
class LLLocalizationResetForcer
{
public:
	LLLocalizationResetForcer(LLFloaterUIPreview* floater, S32 ID);
	virtual ~LLLocalizationResetForcer();

private:
	std::string mSavedLocalization;	// the localization before we change it
};

// Implementation of live file
// When a floater is being previewed, any saved changes to its corresponding
// file cause the previewed floater to be reloaded
class LLGUIPreviewLiveFile : public LLLiveFile
{
public:
	LLGUIPreviewLiveFile(std::string path, std::string name, LLFloaterUIPreview* parent);
	virtual ~LLGUIPreviewLiveFile();
	LLFloaterUIPreview* mParent;
	LLFadeEventTimer* mFadeTimer;	// timer for fade-to-yellow-and-back effect to warn that file has been reloaded
	BOOL mFirstFade;				// setting this avoids showing the fade reload warning on first load
	std::string mFileName;
protected:
	bool loadFile();
};

// Implementation of graphical fade in/out (on timer) for when XUI files are updated
class LLFadeEventTimer : public LLEventTimer
{
public:
	LLFadeEventTimer(F32 refresh, LLGUIPreviewLiveFile* parent);
	BOOL tick();
	LLGUIPreviewLiveFile* mParent;
private:
	BOOL mFadingOut;			// fades in then out; this is toggled in between
	LLColor4 mOriginalColor;	// original color; color is reset to this after fade is coimplete
};

// Implementation of previewed floater
// Used to override draw and mouse handler
class LLPreviewedFloater : public LLFloater
{
public:
	LLPreviewedFloater(LLFloaterUIPreview* floater)
		: LLFloater(LLSD()),
		  mFloaterUIPreview(floater)
	{
	}
	virtual void draw();
	BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);
	BOOL handleToolTip(S32 x, S32 y, MASK mask);
	BOOL selectElement(LLView* parent, int x, int y, int depth);	// select element to display its overlappers

	LLFloaterUIPreview* mFloaterUIPreview;

	// draw widget outlines
	static bool	sShowRectangles;
};

bool LLPreviewedFloater::sShowRectangles = false;

//----------------------------------------------------------------------------

// Localization reset forcer -- ensures that when localization is temporarily changed for previewed floater, it is reset
// Changes are made here
LLLocalizationResetForcer::LLLocalizationResetForcer(LLFloaterUIPreview* floater, S32 ID)
{
	mSavedLocalization = LLUI::sSettingGroups["config"]->getString("Language");				// save current localization setting
	LLUI::sSettingGroups["config"]->setString("Language", floater->getLocStr(ID));// hack language to be the one we want to preview floaters in
	LLUI::setupPaths();														// forcibly reset XUI paths with this new language
}

// Actually reset in destructor
// Changes are reversed here
LLLocalizationResetForcer::~LLLocalizationResetForcer()
{
	LLUI::sSettingGroups["config"]->setString("Language", mSavedLocalization);	// reset language to what it was before we changed it
	LLUI::setupPaths();														// forcibly reset XUI paths with this new language
}

// Live file constructor
// Needs full path for LLLiveFile but needs just file name for this code, hence the reduntant arguments; easier than separating later
LLGUIPreviewLiveFile::LLGUIPreviewLiveFile(std::string path, std::string name, LLFloaterUIPreview* parent)
        : mFileName(name),
		mParent(parent),
		mFirstFade(TRUE),
		mFadeTimer(NULL),
		LLLiveFile(path, 1.0)
{}

LLGUIPreviewLiveFile::~LLGUIPreviewLiveFile()
{
	mParent->mLiveFile = NULL;
	if(mFadeTimer)
	{
		mFadeTimer->mParent = NULL;
		// deletes itself; see lltimer.cpp
	}
}

// Live file load
bool LLGUIPreviewLiveFile::loadFile()
{
	mParent->displayFloater(FALSE,1);	// redisplay the floater
	if(mFirstFade)	// only fade if it wasn't just clicked on; can't use "clicked" BOOL below because of an oddity with setting LLLiveFile initial state
	{
		mFirstFade = FALSE;
	}
	else
	{
		if(mFadeTimer)
		{
			mFadeTimer->mParent = NULL;
		}
		mFadeTimer = new LLFadeEventTimer(0.05f,this);
	}
	return true;
}

// Initialize fade event timer
LLFadeEventTimer::LLFadeEventTimer(F32 refresh, LLGUIPreviewLiveFile* parent)
	: mParent(parent),
	mFadingOut(TRUE),
	LLEventTimer(refresh)
{
	mOriginalColor = mParent->mParent->mDisplayedFloater->getBackgroundColor();
}

// Single tick of fade event timer: increment the color
BOOL LLFadeEventTimer::tick()
{
	float diff = 0.04f;
	if(TRUE == mFadingOut)	// set fade for in/out color direction
	{
		diff = -diff;
	}

	if(NULL == mParent)	// no more need to tick, so suicide
	{
		delete this;
		return FALSE;
	}

	// Set up colors
	LLColor4 bg_color = mParent->mParent->mDisplayedFloater->getBackgroundColor();
	LLSD colors = bg_color.getValue();
	LLSD colors_old = colors;

	// Tick colors
	colors[0] = colors[0].asReal() - diff; if(colors[0].asReal() < mOriginalColor.getValue()[0].asReal()) { colors[0] = colors_old[0]; }
	colors[1] = colors[1].asReal() - diff; if(colors[1].asReal() < mOriginalColor.getValue()[1].asReal()) { colors[1] = colors_old[1]; }
	colors[2] = colors[2].asReal() + diff; if(colors[2].asReal() > mOriginalColor.getValue()[2].asReal()) { colors[2] = colors_old[2]; }

	// Clamp and set colors
	bg_color.setValue(colors);
	bg_color.clamp();	// make sure we didn't exceed [0,1]
	mParent->mParent->mDisplayedFloater->setBackgroundColor(bg_color);

	if(bg_color[2] <= 0.0f)	// end of fade out, start fading in
	{
		mFadingOut = FALSE;
	}

	return FALSE;
}

// Constructor
LLFloaterUIPreview::LLFloaterUIPreview(const LLSD& key)
  : LLFloater(key),
	mDisplayedFloater(NULL),
	mDisplayedFloater_2(NULL),
	mLiveFile(NULL),
	// sHighlightingDiffs(FALSE),
	mHighlightingOverlaps(FALSE),
	mLastDisplayedX(0),
	mLastDisplayedY(0)
{
	// called from floater reg: LLUICtrlFactory::getInstance()->buildFloater(this, "floater_ui_preview.xml");
}

// Destructor
LLFloaterUIPreview::~LLFloaterUIPreview()
{
	// spawned floaters are deleted automatically, so we don't need to delete them here

	// save contents of textfields so it can be restored later if the floater is created again this session
	mSavedEditorPath = mEditorPathTextBox->getText();
	mSavedEditorArgs = mEditorArgsTextBox->getText();
	mSavedDiffPath   = mDiffPathTextBox->getText();

	// delete live file if it exists
	if(mLiveFile)
	{
		delete mLiveFile;
		mLiveFile = NULL;
	}
}

// Perform post-build setup (defined in superclass)
BOOL LLFloaterUIPreview::postBuild()
{
	LLPanel* main_panel_tmp = getChild<LLPanel>("main_panel");				// get a pointer to the main panel in order to...
	mFileList = main_panel_tmp->getChild<LLScrollListCtrl>("name_list");	// save pointer to file list
	// Double-click opens the floater, for convenience
	mFileList->setDoubleClickCallback(boost::bind(&LLFloaterUIPreview::onClickDisplayFloater, this, PRIMARY_FLOATER));

	// get pointers to buttons and link to callbacks
	mLanguageSelection = main_panel_tmp->getChild<LLComboBox>("language_select_combo");
	mLanguageSelection->setCommitCallback(boost::bind(&LLFloaterUIPreview::onLanguageComboSelect, this, mLanguageSelection));
	mLanguageSelection_2 = main_panel_tmp->getChild<LLComboBox>("language_select_combo_2");
	mLanguageSelection_2->setCommitCallback(boost::bind(&LLFloaterUIPreview::onLanguageComboSelect, this, mLanguageSelection));
	LLPanel* editor_panel_tmp = main_panel_tmp->getChild<LLPanel>("editor_panel");
	mDisplayFloaterBtn = main_panel_tmp->getChild<LLButton>("display_floater");
	mDisplayFloaterBtn->setClickedCallback(boost::bind(&LLFloaterUIPreview::onClickDisplayFloater, this, PRIMARY_FLOATER));
	mDisplayFloaterBtn_2 = main_panel_tmp->getChild<LLButton>("display_floater_2");
	mDisplayFloaterBtn_2->setClickedCallback(boost::bind(&LLFloaterUIPreview::onClickDisplayFloater, this, SECONDARY_FLOATER));
	mToggleOverlapButton = main_panel_tmp->getChild<LLButton>("toggle_overlap_panel");
	mToggleOverlapButton->setClickedCallback(boost::bind(&LLFloaterUIPreview::onClickToggleOverlapping, this));
	mCloseOtherButton = main_panel_tmp->getChild<LLButton>("close_displayed_floater");
	mCloseOtherButton->setClickedCallback(boost::bind(&LLFloaterUIPreview::onClickCloseDisplayedFloater, this, PRIMARY_FLOATER));
	mCloseOtherButton_2 = main_panel_tmp->getChild<LLButton>("close_displayed_floater_2");
	mCloseOtherButton_2->setClickedCallback(boost::bind(&LLFloaterUIPreview::onClickCloseDisplayedFloater, this, SECONDARY_FLOATER));
	mEditFloaterBtn = main_panel_tmp->getChild<LLButton>("edit_floater");
	mEditFloaterBtn->setClickedCallback(boost::bind(&LLFloaterUIPreview::onClickEditFloater, this));
	mExecutableBrowseButton = editor_panel_tmp->getChild<LLButton>("browse_for_executable");
	LLPanel* vlt_panel_tmp = main_panel_tmp->getChild<LLPanel>("vlt_panel");
	mExecutableBrowseButton->setClickedCallback(boost::bind(&LLFloaterUIPreview::onClickBrowseForEditor, this));
	mDiffBrowseButton = vlt_panel_tmp->getChild<LLButton>("browse_for_vlt_diffs");
	mDiffBrowseButton->setClickedCallback(boost::bind(&LLFloaterUIPreview::onClickBrowseForDiffs, this));
	mToggleHighlightButton = vlt_panel_tmp->getChild<LLButton>("toggle_vlt_diff_highlight");
	mToggleHighlightButton->setClickedCallback(boost::bind(&LLFloaterUIPreview::onClickToggleDiffHighlighting, this));
	main_panel_tmp->getChild<LLButton>("save_floater")->setClickedCallback(boost::bind(&LLFloaterUIPreview::onClickSaveFloater, this, PRIMARY_FLOATER));
	main_panel_tmp->getChild<LLButton>("save_all_floaters")->setClickedCallback(boost::bind(&LLFloaterUIPreview::onClickSaveAll, this, PRIMARY_FLOATER));

	getChild<LLButton>("export_schema")->setClickedCallback(boost::bind(&LLFloaterUIPreview::onClickExportSchema, this));
	getChild<LLUICtrl>("show_rectangles")->setCommitCallback(
		boost::bind(&LLFloaterUIPreview::onClickShowRectangles, this, _2));

	// get pointers to text fields
	mEditorPathTextBox = editor_panel_tmp->getChild<LLLineEditor>("executable_path_field");
	mEditorArgsTextBox = editor_panel_tmp->getChild<LLLineEditor>("executable_args_field");
	mDiffPathTextBox = vlt_panel_tmp->getChild<LLLineEditor>("vlt_diff_path_field");

	// *HACK: restored saved editor path and args to textfields
	mEditorPathTextBox->setText(mSavedEditorPath);
	mEditorArgsTextBox->setText(mSavedEditorArgs);
	mDiffPathTextBox->setText(mSavedDiffPath);

	// Set up overlap panel
	mOverlapPanel = getChild<LLOverlapPanel>("overlap_panel");

	childSetVisible("overlap_scroll", mHighlightingOverlaps);
	
	mDelim = gDirUtilp->getDirDelimiter();	// initialize delimiter to dir sep slash

	// refresh list of available languages (EN will still be default)
	BOOL found = TRUE;
	BOOL found_en_us = FALSE;
	std::string language_directory;
	std::string xui_dir = get_xui_dir();	// directory containing localizations -- don't forget trailing delim
	mLanguageSelection->removeall();																				// clear out anything temporarily in list from XML
	while(found)																									// for every directory
	{
		if((found = gDirUtilp->getNextFileInDir(xui_dir, "*", language_directory, FALSE)))							// get next directory
		{
			std::string full_path = xui_dir + language_directory;
			if(LLFile::isfile(full_path.c_str()))																	// if it's not a directory, skip it
			{
				continue;
			}

			if(strncmp("template",language_directory.c_str(),8) && -1 == language_directory.find("."))				// if it's not the template directory or a hidden directory
			{
				if(!strncmp("en",language_directory.c_str(),5))													// remember if we've seen en, so we can make it default
				{
					found_en_us = TRUE;
				}
				else
				{
					mLanguageSelection->add(std::string(language_directory));											// add it to the language selection dropdown menu
					mLanguageSelection_2->add(std::string(language_directory));
				}
			}
		}
	}
	if(found_en_us)
	{
		mLanguageSelection->add(std::string("en"),ADD_TOP);															// make en first item if we found it
		mLanguageSelection_2->add(std::string("en"),ADD_TOP);	
	}
	else
	{
		std::string warning = std::string("No EN localization found; check your XUI directories!");
		popupAndPrintWarning(warning);
	}
	mLanguageSelection->selectFirstItem();																			// select the first item
	mLanguageSelection_2->selectFirstItem();

	refreshList();																									// refresh the list of available floaters

	return TRUE;
}

// Callback for language combo box selection: refresh current floater when you change languages
void LLFloaterUIPreview::onLanguageComboSelect(LLUICtrl* ctrl)
{
	LLComboBox* caller = dynamic_cast<LLComboBox*>(ctrl);
	if (!caller)
		return;
	if(caller->getName() == std::string("language_select_combo"))
	{
		if(mDisplayedFloater)
		{
			onClickCloseDisplayedFloater(PRIMARY_FLOATER);
			displayFloater(TRUE,1);
		}
	}
	else
	{
		if(mDisplayedFloater_2)
		{
			onClickCloseDisplayedFloater(PRIMARY_FLOATER);
			displayFloater(TRUE,2);	// *TODO: make take an arg
		}
	}

}

void LLFloaterUIPreview::onClickExportSchema()
{
	//NOTE: schema generation not complete
	//gViewerWindow->setCursor(UI_CURSOR_WAIT);
	//std::string template_path = gDirUtilp->getExpandedFilename(LL_PATH_DEFAULT_SKIN, "xui", "schema");

	//typedef LLWidgetTypeRegistry::Registrar::registry_map_t::const_iterator registry_it;
	//registry_it end_it = LLWidgetTypeRegistry::defaultRegistrar().endItems();
	//for(registry_it it = LLWidgetTypeRegistry::defaultRegistrar().beginItems();
	//	it != end_it;
	//	++it)
	//{
	//	std::string widget_name = it->first;
	//	const LLInitParam::BaseBlock& block = 
	//		(*LLDefaultParamBlockRegistry::instance().getValue(*LLWidgetTypeRegistry::instance().getValue(widget_name)))();
	//	LLXMLNodePtr root_nodep = new LLXMLNode();
	//	LLRNGWriter().writeRNG(widget_name, root_nodep, block, "http://www.lindenlab.com/xui");

	//	std::string file_name(template_path + gDirUtilp->getDirDelimiter() + widget_name + ".rng");

	//	LLFILE* rng_file = LLFile::fopen(file_name.c_str(), "w");
	//	{
	//		LLXMLNode::writeHeaderToFile(rng_file);
	//		const bool use_type_decorations = false;
	//		root_nodep->writeToFile(rng_file, std::string(), use_type_decorations);
	//	}
	//	fclose(rng_file);
	//}
	//gViewerWindow->setCursor(UI_CURSOR_ARROW);
}

void LLFloaterUIPreview::onClickShowRectangles(const LLSD& data)
{
	LLPreviewedFloater::sShowRectangles = data.asBoolean();
}

// Close click handler -- delete my displayed floater if it exists
void LLFloaterUIPreview::onClose(bool app_quitting)
{
	if(!app_quitting && mDisplayedFloater)
	{
		onClickCloseDisplayedFloater(PRIMARY_FLOATER);
		onClickCloseDisplayedFloater(SECONDARY_FLOATER);
		delete mDisplayedFloater;
		mDisplayedFloater = NULL;
		delete mDisplayedFloater_2;
		mDisplayedFloater_2 = NULL;
	}
}

// Error handling (to avoid code repetition)
// *TODO: this is currently unlocalized.  Add to alerts/notifications.xml, someday, maybe.
void LLFloaterUIPreview::popupAndPrintWarning(std::string& warning)
{
	llwarns << warning << llendl;
	LLSD args;
	args["MESSAGE"] = warning;
	LLNotifications::instance().add("GenericAlert", args);
}

// Get localization string from drop-down menu
std::string LLFloaterUIPreview::getLocStr(S32 ID)
{
	if(ID == 1)
	{
		return mLanguageSelection->getSelectedItemLabel(0);
	}
	else
	{
		return mLanguageSelection_2->getSelectedItemLabel(0);
	}
}

// Get localized directory (build path from data directory to XUI files, substituting localization string in for language)
std::string LLFloaterUIPreview::getLocalizedDirectory()
{
	return get_xui_dir() + (getLocStr(1)) + mDelim; // e.g. "C:/Code/guipreview/indra/newview/skins/xui/en/";
}

// Refresh the list of floaters by doing a directory traverse for XML XUI floater files
// Could be used to grab any specific language's list of compatible floaters, but currently it's just used to get all of them
void LLFloaterUIPreview::refreshList()
{
	// Note: the mask doesn't seem to accept regular expressions, so there need to be two directory searches here
	mFileList->clearRows();		// empty list
	std::string name;
	BOOL found = TRUE;
	while(found)				// for every floater file that matches the pattern
	{
		if((found = gDirUtilp->getNextFileInDir(getLocalizedDirectory(), "floater_*.xml", name, FALSE)))	// get next file matching pattern
		{
			addFloaterEntry(name.c_str());	// and add it to the list (file name only; localization code takes care of rest of path)
		}
	}
	found = TRUE;
	while(found)				// for every inspector file that matches the pattern
	{
		if((found = gDirUtilp->getNextFileInDir(getLocalizedDirectory(), "inspect_*.xml", name, FALSE)))	// get next file matching pattern
		{
			addFloaterEntry(name.c_str());	// and add it to the list (file name only; localization code takes care of rest of path)
		}
	}
	found = TRUE;
	while(found)				// for every menu file that matches the pattern
	{
		if((found = gDirUtilp->getNextFileInDir(getLocalizedDirectory(), "menu_*.xml", name, FALSE)))	// get next file matching pattern
		{
			addFloaterEntry(name.c_str());	// and add it to the list (file name only; localization code takes care of rest of path)
		}
	}
	found = TRUE;
	while(found)				// for every panel file that matches the pattern
	{
		if((found = gDirUtilp->getNextFileInDir(getLocalizedDirectory(), "panel_*.xml", name, FALSE)))	// get next file matching pattern
		{
			addFloaterEntry(name.c_str());	// and add it to the list (file name only; localization code takes care of rest of path)
		}
	}

	found = TRUE;
	while(found)				// for every sidepanel file that matches the pattern
	{
		if((found = gDirUtilp->getNextFileInDir(getLocalizedDirectory(), "sidepanel_*.xml", name, FALSE)))	// get next file matching pattern
		{
			addFloaterEntry(name.c_str());	// and add it to the list (file name only; localization code takes care of rest of path)
		}
	}

	if(!mFileList->isEmpty())	// if there were any matching files, just select the first one (so we don't have to worry about disabling buttons when no entry is selected)
	{
		mFileList->selectFirstItem();
	}
}

// Add a single entry to the list of available floaters
// Note: no deduplification (shouldn't be necessary)
void LLFloaterUIPreview::addFloaterEntry(const std::string& path)
{
	LLUUID* entry_id = new LLUUID();				// create a new UUID
	entry_id->generate(path);
	const LLUUID& entry_id_ref = *entry_id;			// get a reference to the UUID for the LLSD block

	// fill LLSD column entry: initialize row/col structure
	LLSD row;
	row["id"] = entry_id_ref;
	LLSD& columns = row["columns"];

	// Get name of floater:
	LLXmlTree xml_tree;
	std::string full_path = getLocalizedDirectory() + path;			// get full path
	BOOL success = xml_tree.parseFile(full_path.c_str(), TRUE);		// parse xml
	std::string entry_name;
	std::string entry_title;
	if(success)
	{
		// get root (or error handle)
		LLXmlTreeNode* root_floater = xml_tree.getRoot();
		if (!root_floater)
		{
			std::string warning = std::string("No root node found in XUI file: ") + path;
			popupAndPrintWarning(warning);
			return;
		}

		// get name
		root_floater->getAttributeString("name",entry_name);
		if(std::string("") == entry_name)
		{
			entry_name = "Error: unable to load " + std::string(path);	// set to error state if load fails
		}

		// get title
		root_floater->getAttributeString("title",entry_title); // some don't have a title, and some have title = "(unknown)", so just leave it blank if it fails
	}
	else
	{
		std::string warning = std::string("Unable to parse XUI file: ") + path;	// error handling
		popupAndPrintWarning(warning);
		if(mLiveFile)
		{
			delete mLiveFile;
			mLiveFile = NULL;
		}
		return;
	}

	// Fill floater title column
	columns[0]["column"] = "title_column";
	columns[0]["type"] = "text";
	columns[0]["value"] = entry_title;

	// Fill floater path column
	columns[1]["column"] = "file_column";
	columns[1]["type"] = "text";
	columns[1]["value"] = std::string(path);

	// Fill floater name column
	columns[2]["column"] = "top_level_node_column";
	columns[2]["type"] = "text";
	columns[2]["value"] = entry_name;

	mFileList->addElement(row);		// actually add to list
}

// Respond to button click to display/refresh currently-selected floater
void LLFloaterUIPreview::onClickDisplayFloater(S32 caller_id)
{
	displayFloater(TRUE, caller_id);
}

// Saves the current floater/panel
void LLFloaterUIPreview::onClickSaveFloater(S32 caller_id)
{
	displayFloater(TRUE, caller_id, true);
}

// Saves all floater/panels
void LLFloaterUIPreview::onClickSaveAll(S32 caller_id)
{
	int listSize = mFileList->getItemCount();

	for (int index = 0; index < listSize; index++)
	{
		mFileList->selectNthItem(index);
		displayFloater(TRUE, caller_id, true);
	}
}

// Given path to floater or panel XML file "filename.xml",
// returns "filename_new.xml"
static std::string append_new_to_xml_filename(const std::string& path)
{
	std::string full_filename = gDirUtilp->findSkinnedFilename(LLUI::getLocalizedSkinPath(), path);
	std::string::size_type extension_pos = full_filename.rfind(".xml");
	full_filename.resize(extension_pos);
	full_filename += "_new.xml";
	return full_filename;
}

// Actually display the floater
// Only set up a new live file if this came from a click (at which point there should be no existing live file), rather than from the live file's update itself;
// otherwise, we get an infinite loop as the live file keeps recreating itself.  That means this function is generally called twice.
void LLFloaterUIPreview::displayFloater(BOOL click, S32 ID, bool save)
{
	// Convince UI that we're in a different language (the one selected on the drop-down menu)
	LLLocalizationResetForcer reset_forcer(this, ID);						// save old language in reset forcer object (to be reset upon destruction when it falls out of scope)

	LLPreviewedFloater** floaterp = (ID == 1 ? &(mDisplayedFloater) : &(mDisplayedFloater_2));
	if(ID == 1)
	{
		BOOL floater_already_open = mDisplayedFloater != NULL;
		if(floater_already_open)											// if we are already displaying a floater
		{
			mLastDisplayedX = mDisplayedFloater->calcScreenRect().mLeft;	// save floater's last known position to put the new one there
			mLastDisplayedY = mDisplayedFloater->calcScreenRect().mBottom;
			delete mDisplayedFloater;							// delete it (this closes it too)
			mDisplayedFloater = NULL;							// and reset the pointer
		}
	}
	else
	{
		if(mDisplayedFloater_2 != NULL)
		{
			delete mDisplayedFloater_2;
			mDisplayedFloater_2 = NULL;
		}
	}

	std::string path = mFileList->getSelectedItemLabel(1);		// get the path of the currently-selected floater
	if(std::string("") == path)											// if no item is selected
	{
		return;															// ignore click (this can only happen with empty list; otherwise an item is always selected)
	}

	*floaterp = new LLPreviewedFloater(this);

	if(!strncmp(path.c_str(),"floater_",8)
		|| !strncmp(path.c_str(), "inspect_", 8))		// if it's a floater
	{
		if (save)
		{
			LLXMLNodePtr floater_write = new LLXMLNode();			
			LLUICtrlFactory::getInstance()->buildFloater(*floaterp, path, floater_write);	// just build it

			if (!floater_write->isNull())
			{
				std::string full_filename = append_new_to_xml_filename(path);
				LLFILE* floater_temp = LLFile::fopen(full_filename.c_str(), "w");
				LLXMLNode::writeHeaderToFile(floater_temp);
				const bool use_type_decorations = false;
				floater_write->writeToFile(floater_temp, std::string(), use_type_decorations);
				fclose(floater_temp);
			}
		}
		else
		{
			LLUICtrlFactory::getInstance()->buildFloater(*floaterp, path, NULL);	// just build it
			(*floaterp)->openFloater((*floaterp)->getKey());
			(*floaterp)->setCanResize((*floaterp)->isResizable());
		}

	}
	else if (!strncmp(path.c_str(),"menu_",5))								// if it's a menu
	{
		if (save)
		{	
			LLXMLNodePtr menu_write = new LLXMLNode();	
			LLMenuGL* menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>(path, gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance(), menu_write);

			if (!menu_write->isNull())
			{
				std::string full_filename = append_new_to_xml_filename(path);
				LLFILE* menu_temp = LLFile::fopen(full_filename.c_str(), "w");
				LLXMLNode::writeHeaderToFile(menu_temp);
				const bool use_type_decorations = false;
				menu_write->writeToFile(menu_temp, std::string(), use_type_decorations);
				fclose(menu_temp);
			}

			delete menu;
		}
	}
	else																// if it is a panel...
	{
		const LLFloater::Params& floater_params = LLFloater::getDefaultParams();
		S32 floater_header_size = floater_params.header_height;

		LLPanel::Params panel_params;
		LLPanel* panel = LLUICtrlFactory::create<LLPanel>(panel_params);	// create a new panel

		if (save)
		{
			LLXMLNodePtr panel_write = new LLXMLNode();
			LLUICtrlFactory::getInstance()->buildPanel(panel, path, panel_write);		// build it
			
			if (!panel_write->isNull())
			{
				std::string full_filename = append_new_to_xml_filename(path);
				LLFILE* panel_temp = LLFile::fopen(full_filename.c_str(), "w");
				LLXMLNode::writeHeaderToFile(panel_temp);
				const bool use_type_decorations = false;
				panel_write->writeToFile(panel_temp, std::string(), use_type_decorations);
				fclose(panel_temp);
			}
		}
		else
		{
			LLUICtrlFactory::getInstance()->buildPanel(panel, path);		// build it
			LLRect new_size = panel->getRect();								// get its rectangle
			panel->setOrigin(0,0);											// reset its origin point so it's not offset by -left or other XUI attributes
			(*floaterp)->setTitle(path);									// use the file name as its title, since panels have no guaranteed meaningful name attribute
			panel->setUseBoundingRect(TRUE);								// enable the use of its outer bounding rect (normally disabled because it's O(n) on the number of sub-elements)
			panel->updateBoundingRect();									// update bounding rect
			LLRect bounding_rect = panel->getBoundingRect();				// get the bounding rect
			LLRect new_rect = panel->getRect();								// get the panel's rect
			new_rect.unionWith(bounding_rect);								// union them to make sure we get the biggest one possible
			(*floaterp)->reshape(new_rect.getWidth(), new_rect.getHeight() + floater_header_size);	// reshape floater to match the union rect's dimensions
			panel->reshape(new_rect.getWidth(), new_rect.getHeight());		// reshape panel to match the union rect's dimensions as well (both are needed)
			(*floaterp)->addChild(panel);					// add panel as child
			(*floaterp)->openFloater();						// open floater (needed?)
		}
	}

	if(ID == 1)
	{
		(*floaterp)->setOrigin(mLastDisplayedX, mLastDisplayedY);
	}

	// *HACK: Remove ability to close it; if you close it, its destructor gets called, but we don't know it's null and try to delete it again,
	// resulting in a double free
	(*floaterp)->setCanClose(FALSE);
	
	if(ID == 1)
	{
		mCloseOtherButton->setEnabled(TRUE);	// enable my floater's close button
	}
	else
	{
		mCloseOtherButton_2->setEnabled(TRUE);
	}

	// Add localization to title so user knows whether it's localized or defaulted to en
	std::string full_path = getLocalizedDirectory() + path;
	std::string floater_lang = "EN";
	llstat dummy;
	if(!LLFile::stat(full_path.c_str(), &dummy))	// if the file does not exist
	{
		floater_lang = getLocStr(ID);
	}
	std::string new_title = (*floaterp)->getTitle() + std::string(" [") + floater_lang +
						(ID == 1 ? " - Primary" : " - Secondary") + std::string("]");
	(*floaterp)->setTitle(new_title);

	(*floaterp)->center();
	addDependentFloater(*floaterp);

	if(click && ID == 1 && !save)
	{
		// set up live file to track it
		if(mLiveFile)
		{
			delete mLiveFile;
			mLiveFile = NULL;
		}
		mLiveFile = new LLGUIPreviewLiveFile(std::string(full_path.c_str()),std::string(path.c_str()),this);
		mLiveFile->checkAndReload();
		mLiveFile->addToEventTimer();
	}

	if(ID == 1)
	{
		mToggleOverlapButton->setEnabled(TRUE);
	}

	if(LLView::sHighlightingDiffs && click && ID == 1)
	{
		highlightChangedElements();
	}

	if(ID == 1)
	{
		mOverlapPanel->mOverlapMap.clear();
		LLView::sPreviewClickedElement = NULL;	// stop overlapping elements from drawing
		mOverlapPanel->mLastClickedElement = NULL;
		findOverlapsInChildren((LLView*)mDisplayedFloater);

		// highlight and enable them
		if(mHighlightingOverlaps)
		{
			for(LLOverlapPanel::OverlapMap::iterator iter = mOverlapPanel->mOverlapMap.begin(); iter != mOverlapPanel->mOverlapMap.end(); ++iter)
			{
				LLView* viewp = iter->first;
				LLView::sPreviewHighlightedElements.insert(viewp);
			}
		}
		else if(LLView::sHighlightingDiffs)
		{
			highlightChangedElements();
		}
	}

	// NOTE: language is reset here automatically when the reset forcer object falls out of scope (see header for details)
}

// Respond to button click to edit currently-selected floater
void LLFloaterUIPreview::onClickEditFloater()
{
	std::string file_name = mFileList->getSelectedItemLabel(1);	// get the file name of the currently-selected floater
	if(std::string("") == file_name)										// if no item is selected
	{
		return;															// ignore click
	}
	std::string path = getLocalizedDirectory() + file_name;

	// stat file to see if it exists (some localized versions may not have it there are no diffs, and then we try to open an nonexistent file)
	llstat dummy;
	if(LLFile::stat(path.c_str(), &dummy))								// if the file does not exist
	{
		std::string warning = "No file for this floater exists in the selected localization.  Opening the EN version instead.";
		popupAndPrintWarning(warning);

		path = get_xui_dir() + mDelim + "en" + mDelim + file_name; // open the en version instead, by default
	}

	// get executable path
	const char* exe_path_char;
	std::string path_in_textfield = mEditorPathTextBox->getText();
	if(std::string("") != path_in_textfield)	// if the text field is not emtpy, use its path
	{
		exe_path_char = path_in_textfield.c_str();
	}
	else if (!LLUI::sSettingGroups["config"]->getString("XUIEditor").empty())
	{
		exe_path_char = LLUI::sSettingGroups["config"]->getString("XUIEditor").c_str();
	}
	else									// otherwise use the path specified by the environment variable
	{
		exe_path_char = getenv("LL_XUI_EDITOR");
	}

	// error check executable path
	if(NULL == exe_path_char)
	{
		std::string warning = "Select an editor by setting the environment variable LL_XUI_EDITOR or specifying its path in the \"Editor Path\" field.";
		popupAndPrintWarning(warning);
		return;
	}
	std::string exe_path = exe_path_char;	// do this after error check, otherwise internal strlen call fails on bad char*

	// remove any quotes; they're added back in later where necessary
	int found_at;
	while((found_at = exe_path.find("\"")) != -1 || (found_at = exe_path.find("'")) != -1)
	{
		exe_path.erase(found_at,1);
	}

	llstat s;
	if(!LLFile::stat(exe_path.c_str(), &s)) // If the executable exists
	{
		// build paths and arguments
		std::string args;
		std::string custom_args = mEditorArgsTextBox->getText();
		int position_of_file = custom_args.find(std::string("%FILE%"), 0);	// prepare to replace %FILE% with actual file path
		std::string first_part_of_args = "";
		std::string second_part_of_args = "";
		if(-1 == position_of_file)	// default: Executable.exe File.xml
		{
			args = std::string("\"") + path + std::string("\"");			// execute the command Program.exe "File.xml"
		}
		else						// use advanced command-line arguments, e.g. "Program.exe -safe File.xml" -windowed for "-safe %FILE% -windowed"
		{
			first_part_of_args = custom_args.substr(0,position_of_file);											// get part of args before file name
			second_part_of_args = custom_args.substr(position_of_file+6,custom_args.length());						// get part of args after file name
			custom_args = first_part_of_args + std::string("\"") + path + std::string("\"") + second_part_of_args;	// replace %FILE% with "<file path>" and put back together
			args = custom_args;																						// and save in the variable that is actually used
		}

		// find directory in which executable resides by taking everything after last slash
		int last_slash_position = exe_path.find_last_of(mDelim);
		if(-1 == last_slash_position)
		{
			std::string warning = std::string("Unable to find a valid path to the specified executable for XUI XML editing: ") + exe_path;
			popupAndPrintWarning(warning);
			return;
		}
        std::string exe_dir = exe_path.substr(0,last_slash_position); // strip executable off, e.g. get "C:\Program Files\TextPad 5" (with or without trailing slash)

#if LL_WINDOWS
		PROCESS_INFORMATION pinfo;
		STARTUPINFOA sinfo;
		memset(&sinfo, 0, sizeof(sinfo));
		memset(&pinfo, 0, sizeof(pinfo));

		std::string exe_name = exe_path.substr(last_slash_position+1);
		args = exe_name + std::string(" ") + args;				// and prepend the executable name, so we get 'Program.exe "Arg1"'

		char *args2 = new char[args.size() + 1];	// Windows requires that the second parameter to CreateProcessA be a writable (non-const) string...
		strcpy(args2, args.c_str());

		if(!CreateProcessA(exe_path.c_str(), args2, NULL, NULL, FALSE, 0, NULL, exe_dir.c_str(), &sinfo, &pinfo))
		{
			// DWORD dwErr = GetLastError();
			std::string warning = "Creating editor process failed!";
			popupAndPrintWarning(warning);
		}
		else
		{
			// foo = pinfo.dwProcessId; // get your pid here if you want to use it later on
			// sGatewayHandle = pinfo.hProcess;
			CloseHandle(pinfo.hThread); // stops leaks - nothing else
		}

		delete[] args2;
#else	// if !LL_WINDOWS
		// This code was copied from the code to run SLVoice, with some modification; should work in UNIX (Mac/Darwin or Linux)
		{
			std::vector<std::string> arglist;
			arglist.push_back(exe_path.c_str());

			// Split the argument string into separate strings for each argument
			typedef boost::tokenizer< boost::char_separator<char> > tokenizer;
			boost::char_separator<char> sep("","\" ", boost::drop_empty_tokens);

			tokenizer tokens(args, sep);
			tokenizer::iterator token_iter;
			BOOL inside_quotes = FALSE;
			BOOL last_was_space = FALSE;
			for(token_iter = tokens.begin(); token_iter != tokens.end(); ++token_iter)
			{
				if(!strncmp("\"",(*token_iter).c_str(),2))
				{
					inside_quotes = !inside_quotes;
				}
				else if(!strncmp(" ",(*token_iter).c_str(),2))
				{
					if(inside_quotes)
					{
						arglist.back().append(std::string(" "));
						last_was_space = TRUE;
					}
				}
				else
				{
					std::string to_push = *token_iter;
					if(last_was_space)
					{
						arglist.back().append(to_push);
						last_was_space = FALSE;
					}
					else
					{
						arglist.push_back(to_push);
					}
				}
			}
			
			// create an argv vector for the child process
			char **fakeargv = new char*[arglist.size() + 1];
			int i;
			for(i=0; i < arglist.size(); i++)
				fakeargv[i] = const_cast<char*>(arglist[i].c_str());

			fakeargv[i] = NULL;

			fflush(NULL); // flush all buffers before the child inherits them
			pid_t id = vfork();
			if(id == 0)
			{
				// child
				execv(exe_path.c_str(), fakeargv);

				// If we reach this point, the exec failed.
				// Use _exit() instead of exit() per the vfork man page.
				std::string warning = "Creating editor process failed (vfork/execv)!";
				popupAndPrintWarning(warning);
				_exit(0);
			}

			// parent
			delete[] fakeargv;
			// sGatewayPID = id;
		}
#endif	// LL_WINDOWS
	}
	else
	{
		std::string warning = "Unable to find path to external XML editor for XUI preview tool";
		popupAndPrintWarning(warning);
	}
}

// Respond to button click to browse for an executable with which to edit XML files
void LLFloaterUIPreview::onClickBrowseForEditor()
{
	// create load dialog box
	LLFilePicker::ELoadFilter type = (LLFilePicker::ELoadFilter)((intptr_t)((void*)LLFilePicker::FFLOAD_ALL));	// nothing for *.exe so just use all
	LLFilePicker& picker = LLFilePicker::instance();
	if (!picker.getOpenFile(type))	// user cancelled -- do nothing
	{
		return;
	}

	// put the selected path into text field
	const std::string chosen_path = picker.getFirstFile();
	std::string executable_path = chosen_path;
#if LL_DARWIN
	// on Mac, if it's an application bundle, figure out the actual path from the Info.plist file
	CFStringRef path_cfstr = CFStringCreateWithCString(kCFAllocatorDefault, chosen_path.c_str(), kCFStringEncodingMacRoman);		// get path as a CFStringRef
	CFURLRef path_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path_cfstr, kCFURLPOSIXPathStyle, TRUE);			// turn it into a CFURLRef
	CFBundleRef chosen_bundle = CFBundleCreate(kCFAllocatorDefault, path_url);												// get a handle for the bundle
	if(NULL != chosen_bundle)
	{
		CFDictionaryRef bundleInfoDict = CFBundleGetInfoDictionary(chosen_bundle);												// get the bundle's dictionary
		if(NULL != bundleInfoDict)
		{
			CFStringRef executable_cfstr = (CFStringRef)CFDictionaryGetValue(bundleInfoDict, CFSTR("CFBundleExecutable"));	// get the name of the actual executable (e.g. TextEdit or firefox-bin)
			int max_file_length = 256;																						// (max file name length is 255 in OSX)
			char executable_buf[max_file_length];
			if(CFStringGetCString(executable_cfstr, executable_buf, max_file_length, kCFStringEncodingMacRoman))			// convert CFStringRef to char*
			{
				executable_path += std::string("/Contents/MacOS/") + std::string(executable_buf);							// append path to executable directory and then executable name to exec path
			}
			else
			{
				std::string warning = "Unable to get CString from CFString for executable path";
				popupAndPrintWarning(warning);
			}
		}
		else
		{
			std::string warning = "Unable to get bundle info dictionary from application bundle";
			popupAndPrintWarning(warning);
		}
	}
	else
	{
		if(-1 != executable_path.find(".app"))	// only warn if this path actually had ".app" in it, i.e. it probably just wasn'nt an app bundle and that's okay
		{
			std::string warning = std::string("Unable to get bundle from path \"") + chosen_path + std::string("\"");
			popupAndPrintWarning(warning);
		}
	}

#endif
	mEditorPathTextBox->setText(std::string(executable_path));	// copy the path to the executable to the textfield for display and later fetching
}

// Respond to button click to browse for a VLT-generated diffs file
void LLFloaterUIPreview::onClickBrowseForDiffs()
{
	// create load dialog box
	LLFilePicker::ELoadFilter type = (LLFilePicker::ELoadFilter)((intptr_t)((void*)LLFilePicker::FFLOAD_XML));	// nothing for *.exe so just use all
	LLFilePicker& picker = LLFilePicker::instance();
	if (!picker.getOpenFile(type))	// user cancelled -- do nothing
	{
		return;
	}

	// put the selected path into text field
	const std::string chosen_path = picker.getFirstFile();
	mDiffPathTextBox->setText(std::string(chosen_path));	// copy the path to the executable to the textfield for display and later fetching
	if(LLView::sHighlightingDiffs)								// if we're already highlighting, toggle off and then on so we get the data from the new file
	{
		onClickToggleDiffHighlighting();
		onClickToggleDiffHighlighting();
	}
}

void LLFloaterUIPreview::onClickToggleDiffHighlighting()
{
	if(mHighlightingOverlaps)
	{
		onClickToggleOverlapping();
		mToggleOverlapButton->toggleState();
	}

	LLView::sPreviewHighlightedElements.clear();	// clear lists first
	mDiffsMap.clear();
	mFileList->clearHighlightedItems();

	if(LLView::sHighlightingDiffs)				// Turning highlighting off
	{
		LLView::sHighlightingDiffs = !sHighlightingDiffs;
		return;
	}
	else											// Turning highlighting on
	{
		// Get the file and make sure it exists
		std::string path_in_textfield = mDiffPathTextBox->getText();	// get file path
		BOOL error = FALSE;

		if(std::string("") == path_in_textfield)									// check for blank file
		{
			std::string warning = "Unable to highlight differences because no file was provided; fill in the relevant text field";
			popupAndPrintWarning(warning);
			error = TRUE;
		}

		llstat dummy;
		if(LLFile::stat(path_in_textfield.c_str(), &dummy) && !error)			// check if the file exists (empty check is reduntant but useful for the informative error message)
		{
			std::string warning = std::string("Unable to highlight differences because an invalid path to a difference file was provided:\"") + path_in_textfield + "\"";
			popupAndPrintWarning(warning);
			error = TRUE;
		}

		// Build a list of changed elements as given by the XML
		std::list<std::string> changed_element_names;
		LLXmlTree xml_tree;
		BOOL success = xml_tree.parseFile(path_in_textfield.c_str(), TRUE);

		if(success && !error)
		{
			LLXmlTreeNode* root_floater = xml_tree.getRoot();
			if(!strncmp("XuiDelta",root_floater->getName().c_str(),9))
			{
				for (LLXmlTreeNode* child = root_floater->getFirstChild();		// get the first child first, then below get the next one; otherwise the iterator is invalid (bug or feature in XML code?)
					 child != NULL;
 					 child = root_floater->getNextChild())	// get child for next iteration
				{
					if(!strncmp("file",child->getName().c_str(),5))
					{
						scanDiffFile(child);
					}
					else if(!strncmp("error",child->getName().c_str(),6))
					{
						std::string error_file, error_message;
						child->getAttributeString("filename",error_file);
						child->getAttributeString("message",error_message);
						if(mDiffsMap.find(error_file) != mDiffsMap.end())
						{
							mDiffsMap.insert(std::make_pair(error_file,std::make_pair(StringListPtr(new StringList), StringListPtr(new StringList))));
						}
						mDiffsMap[error_file].second->push_back(error_message);
					}
					else
					{
						std::string warning = std::string("Child was neither a file or an error, but rather the following:\"") + std::string(child->getName()) + "\"";
						popupAndPrintWarning(warning);
						error = TRUE;
						break;
					}
				}
			}
			else
			{
				std::string warning = std::string("Root node not named XuiDelta:\"") + path_in_textfield + "\"";
				popupAndPrintWarning(warning);
				error = TRUE;
			}
		}
		else if(!error)
		{
			std::string warning = std::string("Unable to create tree from XML:\"") + path_in_textfield + "\"";
			popupAndPrintWarning(warning);
			error = TRUE;
		}

		if(error)	// if we encountered an error, reset the button to off
		{
			mToggleHighlightButton->setToggleState(FALSE);		
		}
		else		// only toggle if we didn't encounter an error
		{
			LLView::sHighlightingDiffs = !sHighlightingDiffs;
			highlightChangedElements();		// *TODO: this is extraneous, right?
			highlightChangedFiles();			// *TODO: this is extraneous, right?
		}
	}
}

void LLFloaterUIPreview::scanDiffFile(LLXmlTreeNode* file_node)
{
	// Get file name
	std::string file_name;
	file_node->getAttributeString("name",file_name);
	if(std::string("") == file_name)
	{
		std::string warning = std::string("Empty file name encountered in differences:\"") + file_name + "\"";
		popupAndPrintWarning(warning);
		return;
	}

	// Get a list of changed elements
	// Get the first child first, then below get the next one; otherwise the iterator is invalid (bug or feature in XML code?)
	for (LLXmlTreeNode* child = file_node->getFirstChild(); child != NULL; child = file_node->getNextChild())
	{
		if(!strncmp("delta",child->getName().c_str(),6))
		{
			std::string id;
			child->getAttributeString("id",id);
			if(mDiffsMap.find(file_name) == mDiffsMap.end())
			{
				mDiffsMap.insert(std::make_pair(file_name,std::make_pair(StringListPtr(new StringList), StringListPtr(new StringList))));
			}
			mDiffsMap[file_name].first->push_back(std::string(id.c_str()));
		}
		else
		{
			std::string warning = std::string("Child of file was not a delta, but rather the following:\"") + std::string(child->getName()) + "\"";
			popupAndPrintWarning(warning);
			return;
		}
	}
}

void LLFloaterUIPreview::highlightChangedElements()
{
	if(NULL == mLiveFile)
	{
		return;
	}

	// Process differences first (we want their warnings to be shown underneath other warnings)
	StringListPtr changed_element_paths;
	DiffMap::iterator iterExists = mDiffsMap.find(mLiveFile->mFileName);
	if(iterExists != mDiffsMap.end())
	{
		changed_element_paths = mDiffsMap[mLiveFile->mFileName].first;		// retrieve list of changed element paths from map
	}

	for(std::list<std::string>::iterator iter = changed_element_paths->begin(); iter != changed_element_paths->end(); ++iter)	// for every changed element path
	{
		LLView* element = mDisplayedFloater;
		if(!strncmp(iter->c_str(),".",1))	// if it's the root floater itself
		{
			continue;
		}

		// Split element hierarchy path on period (*HACK: it's possible that the element name will have a period in it, in which case this won't work.  See https://wiki.lindenlab.com/wiki/Viewer_Localization_Tool_Documentation.)
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep(".");
		tokenizer tokens(*iter, sep);
		tokenizer::iterator token_iter;
		BOOL failed = FALSE;
		for(token_iter = tokens.begin(); token_iter != tokens.end(); ++token_iter)
		{
			element = element->findChild<LLView>(*token_iter,FALSE);	// try to find element: don't recur, and don't create if missing

			// if we still didn't find it...
			if(NULL == element)												
			{
				llinfos << "Unable to find element in XuiDelta file named \"" << *iter << "\" in file \"" << mLiveFile->mFileName <<
							"\". The element may no longer exist, the path may be incorrect, or it may not be a non-displayable element (not an LLView) such as a \"string\" type." << llendl;
				failed = TRUE;
				break;
			}
		}

		if(!failed)
		{
			// Now that we have a pointer to the actual element, add it to the list of elements to be highlighted
			std::set<LLView*>::iterator iter2 = std::find(LLView::sPreviewHighlightedElements.begin(), LLView::sPreviewHighlightedElements.end(), element);
			if(iter2 == LLView::sPreviewHighlightedElements.end())
			{
				LLView::sPreviewHighlightedElements.insert(element);
			}
		}
	}

	// Process errors second, so their warnings show up on top of other warnings
	StringListPtr error_list;
	if(iterExists != mDiffsMap.end())
	{
		error_list = mDiffsMap[mLiveFile->mFileName].second;
	}
	for(std::list<std::string>::iterator iter = error_list->begin(); iter != error_list->end(); ++iter)	// for every changed element path
	{
		std::string warning = std::string("Error listed among differences.  Filename: \"") + mLiveFile->mFileName + "\".  Message: \"" + *iter + "\"";
		popupAndPrintWarning(warning);
	}
}

void LLFloaterUIPreview::highlightChangedFiles()
{
	for(DiffMap::iterator iter = mDiffsMap.begin(); iter != mDiffsMap.end(); ++iter)	// for every file listed in diffs
	{
		LLScrollListItem* item = mFileList->getItemByLabel(std::string(iter->first), FALSE, 1);
		if(item)
		{
			item->setHighlighted(TRUE);
		}
	}
}

// Respond to button click to browse for an executable with which to edit XML files
void LLFloaterUIPreview::onClickCloseDisplayedFloater(S32 caller_id)
{
	if(caller_id == PRIMARY_FLOATER)
	{
		mCloseOtherButton->setEnabled(FALSE);
		mToggleOverlapButton->setEnabled(FALSE);

		if(mDisplayedFloater)
		{
			mLastDisplayedX = mDisplayedFloater->calcScreenRect().mLeft;
			mLastDisplayedY = mDisplayedFloater->calcScreenRect().mBottom;
			delete mDisplayedFloater;
			mDisplayedFloater = NULL;
		}

		if(mLiveFile)
		{
			delete mLiveFile;
			mLiveFile = NULL;
		}

		if(mToggleOverlapButton->getToggleState())
		{
			mToggleOverlapButton->toggleState();
			onClickToggleOverlapping();
		}

		LLView::sPreviewClickedElement = NULL;	// stop overlapping elements panel from drawing
		mOverlapPanel->mLastClickedElement = NULL;
	}
	else
	{
		mCloseOtherButton_2->setEnabled(FALSE);
		delete mDisplayedFloater_2;
		mDisplayedFloater_2 = NULL;
	}

}

void append_view_tooltip(LLView* tooltip_view, std::string *tooltip_msg)
{
	LLRect rect = tooltip_view->getRect();
	LLRect parent_rect = tooltip_view->getParent()->getRect();
	S32 left = rect.mLeft;
	// invert coordinate system for XUI top-left layout
	S32 top = parent_rect.getHeight() - rect.mTop;
	if (!tooltip_msg->empty())
	{
		tooltip_msg->append("\n");
	}
	std::string msg = llformat("%s %d, %d (%d x %d)",
		tooltip_view->getName().c_str(),
		left,
		top,
		rect.getWidth(),
		rect.getHeight() );
	tooltip_msg->append( msg );
}

BOOL LLPreviewedFloater::handleToolTip(S32 x, S32 y, MASK mask)
{
	if (!sShowRectangles)
	{
		return LLFloater::handleToolTip(x, y, mask);
	}

	S32 screen_x, screen_y;
	localPointToScreen(x, y, &screen_x, &screen_y);
	std::string tooltip_msg;
	LLView* tooltip_view = this;
	LLView::tree_iterator_t end_it = endTreeDFS();
	for (LLView::tree_iterator_t it = beginTreeDFS(); it != end_it; ++it)
	{
		LLView* viewp = *it;
		LLRect screen_rect;
		viewp->localRectToScreen(viewp->getLocalRect(), &screen_rect);
		if (!(viewp->getVisible()
			 && screen_rect.pointInRect(screen_x, screen_y)))
		{
			it.skipDescendants();
		}
		// only report xui names for LLUICtrls, not the various container LLViews

		else if (dynamic_cast<LLUICtrl*>(viewp))
		{
			// if we are in a new part of the tree (not a descendent of current tooltip_view)
			// then push the results for tooltip_view and start with a new potential view
			// NOTE: this emulates visiting only the leaf nodes that meet our criteria

			if (tooltip_view != this
				&& !viewp->hasAncestor(tooltip_view))
			{
				append_view_tooltip(tooltip_view, &tooltip_msg);
			}
			tooltip_view = viewp;
		}
	}

	append_view_tooltip(tooltip_view, &tooltip_msg);
	
	LLToolTipMgr::instance().show(LLToolTip::Params()
		.message(tooltip_msg)
		.max_width(400));
	return TRUE;
}

BOOL LLPreviewedFloater::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	selectElement(this,x,y,0);
	return TRUE;
}

// *NOTE: In order to hide all of the overlapping elements of the selected element so as to see it in context, here is what you would need to do:
// -This selectElement call fills the overlap panel as normal.  The element which is "selected" here is actually just an intermediate selection step;
// what you've really selected is a list of elements: the one you clicked on and everything that overlaps it.
// -The user then selects one of the elements from this list the overlap panel (click handling to the overlap panel would have to be added).
//  This becomes the final selection (as opposed to the intermediate selection that was just made).
// -Everything else that is currently displayed on the overlap panel should be hidden from view in the previewed floater itself (setVisible(FALSE)).
// -Subsequent clicks on other elements in the overlap panel (they should still be there) should make other elements the final selection.
// -On close or on the click of a new button, everything should be shown again and all selection state should be cleared.
//   ~Jacob, 8/08
BOOL LLPreviewedFloater::selectElement(LLView* parent, int x, int y, int depth)
{
	if(getVisible())
	{
		BOOL handled = FALSE;
		if(LLFloaterUIPreview::containerType(parent))
		{
			for(child_list_const_iter_t child_it = parent->getChildList()->begin(); child_it != parent->getChildList()->end(); ++child_it)
			{
				LLView* child = *child_it;
				S32 local_x = x - child->getRect().mLeft;
				S32 local_y = y - child->getRect().mBottom;
				if (child->pointInView(local_x, local_y) &&
					child->getVisible() &&
					selectElement(child, x, y, ++depth))
				{
					handled = TRUE;
					break;
				}
			}
		}

		if(!handled)
		{
			LLView::sPreviewClickedElement = parent;
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void LLPreviewedFloater::draw()
{
	if(NULL != mFloaterUIPreview)
	{
		// Set and unset sDrawPreviewHighlights flag so as to avoid using two flags
		if(mFloaterUIPreview->mHighlightingOverlaps)
		{
			LLView::sDrawPreviewHighlights = TRUE;
		}

		// If we're looking for truncations, draw debug rects for the displayed
		// floater only.
		bool old_debug_rects = LLView::sDebugRects;
		bool old_show_names = LLView::sDebugRectsShowNames;
		if (sShowRectangles)
		{
			LLView::sDebugRects = true;
			LLView::sDebugRectsShowNames = false;
		}

		LLFloater::draw();

		LLView::sDebugRects = old_debug_rects;
		LLView::sDebugRectsShowNames = old_show_names;

		if(mFloaterUIPreview->mHighlightingOverlaps)
		{
			LLView::sDrawPreviewHighlights = FALSE;
		}
	}
}

void LLFloaterUIPreview::onClickToggleOverlapping()
{
	if(LLView::sHighlightingDiffs)
	{
		onClickToggleDiffHighlighting();
		mToggleHighlightButton->toggleState();
	}
	LLView::sPreviewHighlightedElements.clear();	// clear lists first

	S32 width, height;
	getResizeLimits(&width, &height);	// illegal call of non-static member function
	if(mHighlightingOverlaps)
	{
		mHighlightingOverlaps = !mHighlightingOverlaps;
		// reset list of preview highlighted elements
		setRect(LLRect(getRect().mLeft,getRect().mTop,getRect().mRight - mOverlapPanel->getRect().getWidth(),getRect().mBottom));
		setResizeLimits(width - mOverlapPanel->getRect().getWidth(), height);
	}
	else
	{
		mHighlightingOverlaps = !mHighlightingOverlaps;
		displayFloater(FALSE,1);
		setRect(LLRect(getRect().mLeft,getRect().mTop,getRect().mRight + mOverlapPanel->getRect().getWidth(),getRect().mBottom));
		setResizeLimits(width + mOverlapPanel->getRect().getWidth(), height);
	}
	childSetVisible("overlap_scroll", mHighlightingOverlaps);
}

void LLFloaterUIPreview::findOverlapsInChildren(LLView* parent)
{
	if(parent->getChildCount() == 0 || !containerType(parent))	// if it has no children or isn't a container type, skip it
	{
		return;
	}

	// for every child of the parent
	for(child_list_const_iter_t child_it = parent->getChildList()->begin(); child_it != parent->getChildList()->end(); ++child_it)
	{
		LLView* child = *child_it;
		if(overlapIgnorable(child))
		{
			continue;
		}

		// for every sibling
		for(child_list_const_iter_t sibling_it = parent->getChildList()->begin(); sibling_it != parent->getChildList()->end(); ++sibling_it)	// for each sibling
		{
			LLView* sibling = *sibling_it;
			if(overlapIgnorable(sibling))
			{
				continue;
			}

			// if they overlap... (we don't care if they're visible or enabled -- we want to check those anyway, i.e. hidden tabs that can be later shown)
			if(sibling != child && elementOverlap(child, sibling))
			{
				mOverlapPanel->mOverlapMap[child].push_back(sibling);		// add to the map
			}
		}
		findOverlapsInChildren(child);						// recur
	}
}

// *HACK: don't overlap with the drag handle and various other elements
// This is using dynamic casts because there is no object-oriented way to tell which elements contain localizable text.  These are a few that are ignorable.
// *NOTE: If a list of elements which have localizable content were created, this function should return false if viewp's class is in that list.
BOOL LLFloaterUIPreview::overlapIgnorable(LLView* viewp)
{
	return	NULL != dynamic_cast<LLDragHandle*>(viewp) ||
			NULL != dynamic_cast<LLViewBorder*>(viewp) ||
			NULL != dynamic_cast<LLResizeBar*>(viewp);
}

// *HACK: these are the only two container types as of 8/08, per Richard
// This is using dynamic casts because there is no object-oriented way to tell which elements are containers.
BOOL LLFloaterUIPreview::containerType(LLView* viewp)
{
	return NULL != dynamic_cast<LLPanel*>(viewp) || NULL != dynamic_cast<LLLayoutStack*>(viewp);
}

// Check if two llview's rectangles overlap, with some tolerance
BOOL LLFloaterUIPreview::elementOverlap(LLView* view1, LLView* view2)
{
	LLSD rec1 = view1->getRect().getValue();
	LLSD rec2 = view2->getRect().getValue();
	int tolerance = 2;
	return (int)rec1[0] <= (int)rec2[2] - tolerance && 
		   (int)rec2[0] <= (int)rec1[2] - tolerance && 
		   (int)rec1[3] <= (int)rec2[1] - tolerance && 
		   (int)rec2[3] <= (int)rec1[1] - tolerance;
}

void LLOverlapPanel::draw()
{
	static const std::string current_selection_text("Current selection: ");
	static const std::string overlapper_text("Overlapper: ");
	LLColor4 text_color = LLColor4::grey;
	gGL.color4fv(text_color.mV);

	if(!LLView::sPreviewClickedElement)
	{
		LLUI::translate(5,getRect().getHeight()-20);	// translate to top-5,left-5
		LLView::sDrawPreviewHighlights = FALSE;
		LLFontGL::getFontSansSerifSmall()->renderUTF8(current_selection_text, 0, 0, 0, text_color,
				LLFontGL::LEFT, LLFontGL::BASELINE, LLFontGL::NORMAL, LLFontGL::NO_SHADOW, S32_MAX, S32_MAX, NULL, FALSE);
	}
	else
	{
		OverlapMap::iterator iterExists = mOverlapMap.find(LLView::sPreviewClickedElement);
		if(iterExists == mOverlapMap.end())
		{
			return;
		}

		std::list<LLView*> overlappers = mOverlapMap[LLView::sPreviewClickedElement];
		if(overlappers.size() == 0)
		{
			LLUI::translate(5,getRect().getHeight()-20);	// translate to top-5,left-5
			LLView::sDrawPreviewHighlights = FALSE;
			std::string current_selection = std::string(current_selection_text + LLView::sPreviewClickedElement->getName() + " (no elements overlap)");
			S32 text_width = LLFontGL::getFontSansSerifSmall()->getWidth(current_selection) + 10;
			LLFontGL::getFontSansSerifSmall()->renderUTF8(current_selection, 0, 0, 0, text_color,
					LLFontGL::LEFT, LLFontGL::BASELINE, LLFontGL::NORMAL, LLFontGL::NO_SHADOW, S32_MAX, S32_MAX, NULL, FALSE);
			// widen panel enough to fit this text
			LLRect rect = getRect();
			setRect(LLRect(rect.mLeft,rect.mTop,rect.getWidth() < text_width ? rect.mLeft + text_width : rect.mRight,rect.mTop));
			return;
		}

		// recalculate required with and height; otherwise use cached
		BOOL need_to_recalculate_bounds = FALSE;
		if(mLastClickedElement == NULL)
		{
			need_to_recalculate_bounds = TRUE;
		}

		if(NULL == mLastClickedElement)
		{
			mLastClickedElement = LLView::sPreviewClickedElement;
		}

		// recalculate bounds for scroll panel
		if(need_to_recalculate_bounds || LLView::sPreviewClickedElement->getName() != mLastClickedElement->getName())
		{
			// reset panel's rectangle to its default width and height (300x600)
			LLRect panel_rect = getRect();
			setRect(LLRect(panel_rect.mLeft,panel_rect.mTop,panel_rect.mLeft+getRect().getWidth(),panel_rect.mTop-getRect().getHeight()));

			LLRect rect;

			// change bounds for selected element
			int height_sum = mLastClickedElement->getRect().getHeight() + mSpacing + 80;
			rect = getRect();
			setRect(LLRect(rect.mLeft,rect.mTop,rect.getWidth() > mLastClickedElement->getRect().getWidth() + 5 ? rect.mRight : rect.mLeft + mLastClickedElement->getRect().getWidth() + 5, rect.mBottom));

			// and widen to accomodate text if that's wider
			std::string display_text = current_selection_text + LLView::sPreviewClickedElement->getName();
			S32 text_width = LLFontGL::getFontSansSerifSmall()->getWidth(display_text) + 10;
			rect = getRect();
			setRect(LLRect(rect.mLeft,rect.mTop,rect.getWidth() < text_width ? rect.mLeft + text_width : rect.mRight,rect.mTop));

			std::list<LLView*> overlappers = mOverlapMap[LLView::sPreviewClickedElement];
			for(std::list<LLView*>::iterator overlap_it = overlappers.begin(); overlap_it != overlappers.end(); ++overlap_it)
			{
				LLView* viewp = *overlap_it;
				height_sum += viewp->getRect().getHeight() + mSpacing*3;
		
				// widen panel's rectangle to accommodate widest overlapping element of this floater
				rect = getRect();
				setRect(LLRect(rect.mLeft,rect.mTop,rect.getWidth() > viewp->getRect().getWidth() + 5 ? rect.mRight : rect.mLeft + viewp->getRect().getWidth() + 5, rect.mBottom));
				
				// and widen to accomodate text if that's wider
				std::string display_text = overlapper_text + viewp->getName();
				S32 text_width = LLFontGL::getFontSansSerifSmall()->getWidth(display_text) + 10;
				rect = getRect();
				setRect(LLRect(rect.mLeft,rect.mTop,rect.getWidth() < text_width ? rect.mLeft + text_width : rect.mRight,rect.mTop));
			}
			// change panel's height to accommodate all element heights plus spacing between them
			rect = getRect();
			setRect(LLRect(rect.mLeft,rect.mTop,rect.mRight,rect.mTop-height_sum));
		}

		LLUI::translate(5,getRect().getHeight()-10);	// translate to top left
		LLView::sDrawPreviewHighlights = FALSE;

		// draw currently-selected element at top of overlappers
		LLUI::translate(0,-mSpacing);
		LLFontGL::getFontSansSerifSmall()->renderUTF8(current_selection_text + LLView::sPreviewClickedElement->getName(), 0, 0, 0, text_color,
				LLFontGL::LEFT, LLFontGL::BASELINE, LLFontGL::NORMAL, LLFontGL::NO_SHADOW, S32_MAX, S32_MAX, NULL, FALSE);
		LLUI::translate(0,-mSpacing-LLView::sPreviewClickedElement->getRect().getHeight());	// skip spacing distance + height
		LLView::sPreviewClickedElement->draw();

		for(std::list<LLView*>::iterator overlap_it = overlappers.begin(); overlap_it != overlappers.end(); ++overlap_it)
		{
			LLView* viewp = *overlap_it;

			// draw separating line
			LLUI::translate(0,-mSpacing);
			gl_line_2d(0,0,getRect().getWidth()-10,0,LLColor4(192.0f/255.0f,192.0f/255.0f,192.0f/255.0f));

			// draw name
			LLUI::translate(0,-mSpacing);
			LLFontGL::getFontSansSerifSmall()->renderUTF8(overlapper_text + viewp->getName(), 0, 0, 0, text_color,
					LLFontGL::LEFT, LLFontGL::BASELINE, LLFontGL::NORMAL, LLFontGL::NO_SHADOW, S32_MAX, S32_MAX, NULL, FALSE);

			// draw element
			LLUI::translate(0,-mSpacing-viewp->getRect().getHeight());	// skip spacing distance + height
			viewp->draw();
		}
		mLastClickedElement = LLView::sPreviewClickedElement;
	}
}

void LLFloaterUIPreviewUtil::registerFloater()
{
	LLFloaterReg::add("ui_preview", "floater_ui_preview.xml",
		&LLFloaterReg::build<LLFloaterUIPreview>);
}
