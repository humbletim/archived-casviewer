/** 
 * @file llinventoryobserver.h
 * @brief LLInventoryObserver class header file
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLINVENTORYOBSERVERS_H
#define LL_LLINVENTORYOBSERVERS_H

#include "lluuid.h"
#include <string>
#include <vector>

class LLViewerInventoryCategory;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryObserver
//
//   A simple abstract base class that can relay messages when the inventory 
//   changes.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryObserver
{
public:
	// This enumeration is a way to refer to what changed in a more
	// human readable format. You can mask the value provided by
	// chaged() to see if the observer is interested in the change.
	enum 
	{
		NONE 			= 0,
		LABEL 			= 1,	// Name changed
		INTERNAL 		= 2,	// Internal change (e.g. asset uuid different)
		ADD 			= 4,	// Something added
		REMOVE 			= 8,	// Something deleted
		STRUCTURE 		= 16,	// Structural change (e.g. item or folder moved)
		CALLING_CARD 	= 32,	// Calling card change (e.g. online, grant status, cancel)
		GESTURE 		= 64,
		REBUILD 		= 128, 	// Item UI changed (e.g. item type different)
		SORT 			= 256, 	// Folder needs to be resorted.
		ALL 			= 0xffffffff
	};
	LLInventoryObserver();
	virtual ~LLInventoryObserver();
	virtual void changed(U32 mask) = 0;
	std::string mMessageName; // used by Agent Inventory Service only. [DEV-20328]
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryFetchItemsObserver
//
//   Fetches inventory items, calls done() when all inventory has arrived. 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryFetchItemsObserver : public LLInventoryObserver
{
public:
	LLInventoryFetchItemsObserver(bool retry_if_missing = false); // deprecated
	LLInventoryFetchItemsObserver(const LLUUID& item_id, bool retry_if_missing = false); // single item
	LLInventoryFetchItemsObserver(const uuid_vec_t& item_ids, bool retry_if_missing = false); // multiple items
	virtual void changed(U32 mask);

	bool isEverythingComplete() const;
	void setItems(const uuid_vec_t& ids) { mIDs = ids; }
	void startFetch();

	virtual void done() {};

protected:
	bool mRetryIfMissing;
	uuid_vec_t mComplete;
	uuid_vec_t mIncomplete;
	uuid_vec_t mIDs;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryFetchDescendentsObserver
//
//   Fetches children of a category/folder, calls done() when all 
//   inventory has arrived. 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInventoryFetchDescendentsObserver : public LLInventoryObserver
{
public:
	LLInventoryFetchDescendentsObserver();
	LLInventoryFetchDescendentsObserver(const LLUUID& cat_id);
	LLInventoryFetchDescendentsObserver(const uuid_vec_t& cat_ids);
	virtual void changed(U32 mask);

	void setFolders(const uuid_vec_t& cat_ids) { mIDs = cat_ids; }
	void setFolders(const LLUUID& cat_id) { mIDs.clear(); mIDs.push_back(cat_id); }
	void startFetch();
	bool isEverythingComplete() const;
	virtual void done() = 0;

protected:
	bool isComplete(LLViewerInventoryCategory* cat);
	uuid_vec_t mIncomplete;
	uuid_vec_t mComplete;
	uuid_vec_t mIDs;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryFetchComboObserver
//
// This class does an appropriate combination of fetch descendents and
// item fetches based on completion of categories and items. Much like
// the fetch and fetch descendents, this will call done() when everything
// has arrived.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInventoryFetchComboObserver : public LLInventoryObserver
{
public:
	LLInventoryFetchComboObserver() : mDone(false) {}
	virtual void changed(U32 mask);

	void startFetch(const uuid_vec_t& folder_ids, const uuid_vec_t& item_ids);

	virtual void done() = 0;

protected:
	bool mDone;
	uuid_vec_t mCompleteFolders;
	uuid_vec_t mIncompleteFolders;
	uuid_vec_t mCompleteItems;
	uuid_vec_t mIncompleteItems;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryExistenceObserver
//
// This class is used as a base class for doing somethign when all the
// observed item ids exist in the inventory somewhere. You can derive
// a class from this class and implement the done() method to do
// something useful.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryExistenceObserver : public LLInventoryObserver
{
public:
	LLInventoryExistenceObserver() {}
	virtual void changed(U32 mask);

	void watchItem(const LLUUID& id);

protected:
	virtual void done() = 0;
	uuid_vec_t mExist;
	uuid_vec_t mMIA;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryAddedObserver
//
// This class is used as a base class for doing something when 
// a new item arrives in inventory.
// It does not watch for a certain UUID, rather it acts when anything is added
// Derive a class from this class and implement the done() method to do
// something useful.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryAddedObserver : public LLInventoryObserver
{
public:
	LLInventoryAddedObserver() : mAdded() {}
	virtual void changed(U32 mask);

protected:
	virtual void done() = 0;

	uuid_vec_t mAdded;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryTransactionObserver
//
// Class which can be used as a base class for doing something when an
// inventory transaction completes.
//
// *NOTE: This class is not quite complete. Avoid using unless you fix up it's
// functionality gaps.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryTransactionObserver : public LLInventoryObserver
{
public:
	LLInventoryTransactionObserver(const LLTransactionID& transaction_id);
	virtual void changed(U32 mask);

protected:
	virtual void done(const uuid_vec_t& folders, const uuid_vec_t& items) = 0;

	LLTransactionID mTransactionID;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryCompletionObserver
//
//   Base class for doing something when when all observed items are locally 
//   complete.  Implements the changed() method of LLInventoryObserver 
//   and declares a new method named done() which is called when all watched items 
//   have complete information in the inventory model.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryCompletionObserver : public LLInventoryObserver
{
public:
	LLInventoryCompletionObserver() {}
	virtual void changed(U32 mask);

	void watchItem(const LLUUID& id);

protected:
	virtual void done() = 0;

	uuid_vec_t mComplete;
	uuid_vec_t mIncomplete;
};

#endif // LL_LLINVENTORYOBSERVERS_H

