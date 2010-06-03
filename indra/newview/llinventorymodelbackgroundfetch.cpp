/** 
 * @file llinventorymodel.cpp
 * @brief Implementation of the inventory model used to track agent inventory.
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

#include "llviewerprecompiledheaders.h"
#include "llinventorymodelbackgroundfetch.h"

// Seraph clean this up
#include "llagent.h"
#include "llinventorypanel.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"
#include "llviewerwindow.h"
#include "llappviewer.h"
#include "llviewerregion.h"
#include "llcallbacklist.h"

const F32 MAX_TIME_FOR_SINGLE_FETCH = 10.f;
const S32 MAX_FETCH_RETRIES = 10;

LLInventoryModelBackgroundFetch::LLInventoryModelBackgroundFetch() :
	mBackgroundFetchActive(FALSE),
	mAllFoldersFetched(FALSE),
	mRecursiveInventoryFetchStarted(FALSE),
	mRecursiveLibraryFetchStarted(FALSE),
	mNumFetchRetries(0),
	mMinTimeBetweenFetches(0.3f),
	mMaxTimeBetweenFetches(10.f),
	mTimelyFetchPending(FALSE),
	mBulkFetchCount(0)
{
}

LLInventoryModelBackgroundFetch::~LLInventoryModelBackgroundFetch()
{
}

bool LLInventoryModelBackgroundFetch::isBulkFetchProcessingComplete()
{
	return mFetchQueue.empty() && mBulkFetchCount<=0;
}

bool LLInventoryModelBackgroundFetch::libraryFetchStarted()
{
	return mRecursiveLibraryFetchStarted;
}

bool LLInventoryModelBackgroundFetch::libraryFetchCompleted()
{
	return libraryFetchStarted() && fetchQueueContainsNoDescendentsOf(gInventory.getLibraryRootFolderID());
}

bool LLInventoryModelBackgroundFetch::libraryFetchInProgress()
{
	return libraryFetchStarted() && !libraryFetchCompleted();
}
	
bool LLInventoryModelBackgroundFetch::inventoryFetchStarted()
{
	return mRecursiveInventoryFetchStarted;
}

bool LLInventoryModelBackgroundFetch::inventoryFetchCompleted()
{
	return inventoryFetchStarted() && fetchQueueContainsNoDescendentsOf(gInventory.getRootFolderID());
}

bool LLInventoryModelBackgroundFetch::inventoryFetchInProgress()
{
	return inventoryFetchStarted() && !inventoryFetchCompleted();
}

bool LLInventoryModelBackgroundFetch::isEverythingFetched()
{
	return mAllFoldersFetched;
}

BOOL LLInventoryModelBackgroundFetch::backgroundFetchActive()
{
	return mBackgroundFetchActive;
}

void LLInventoryModelBackgroundFetch::start(const LLUUID& cat_id, BOOL recursive)
{
	if (!mAllFoldersFetched)
	{
		LL_DEBUGS("InventoryFetch") << "Start fetching category: " << cat_id << ", recursive: " << recursive << LL_ENDL;

		mBackgroundFetchActive = TRUE;
		if (cat_id.isNull())
		{
			if (!mRecursiveInventoryFetchStarted)
			{
				mRecursiveInventoryFetchStarted |= recursive;
				mFetchQueue.push_back(FetchQueueInfo(gInventory.getRootFolderID(), recursive));
				gIdleCallbacks.addFunction(&LLInventoryModelBackgroundFetch::backgroundFetchCB, NULL);
			}
			if (!mRecursiveLibraryFetchStarted)
			{
				mRecursiveLibraryFetchStarted |= recursive;
				mFetchQueue.push_back(FetchQueueInfo(gInventory.getLibraryRootFolderID(), recursive));
				gIdleCallbacks.addFunction(&LLInventoryModelBackgroundFetch::backgroundFetchCB, NULL);
			}
		}
		else
		{
			// specific folder requests go to front of queue
			if (mFetchQueue.empty() || mFetchQueue.front().mCatUUID != cat_id)
			{
				mFetchQueue.push_front(FetchQueueInfo(cat_id, recursive));
				gIdleCallbacks.addFunction(&LLInventoryModelBackgroundFetch::backgroundFetchCB, NULL);
			}
			if (cat_id == gInventory.getLibraryRootFolderID())
			{
				mRecursiveLibraryFetchStarted |= recursive;
			}
			if (cat_id == gInventory.getRootFolderID())
			{
				mRecursiveInventoryFetchStarted |= recursive;
			}
		}
	}
}

void LLInventoryModelBackgroundFetch::findLostItems()
{
	mBackgroundFetchActive = TRUE;
    mFetchQueue.push_back(FetchQueueInfo(LLUUID::null, TRUE));
    gIdleCallbacks.addFunction(&LLInventoryModelBackgroundFetch::backgroundFetchCB, NULL);
}

void LLInventoryModelBackgroundFetch::stopBackgroundFetch()
{
	if (mBackgroundFetchActive)
	{
		mBackgroundFetchActive = FALSE;
		gIdleCallbacks.deleteFunction(&LLInventoryModelBackgroundFetch::backgroundFetchCB, NULL);
		mBulkFetchCount=0;
		mMinTimeBetweenFetches=0.0f;
	}
}

void LLInventoryModelBackgroundFetch::setAllFoldersFetched()
{
	if (mRecursiveInventoryFetchStarted &&
		mRecursiveLibraryFetchStarted)
	{
		mAllFoldersFetched = TRUE;
	}
	stopBackgroundFetch();
}

void LLInventoryModelBackgroundFetch::backgroundFetchCB(void *)
{
	LLInventoryModelBackgroundFetch::instance().backgroundFetch();
}

void LLInventoryModelBackgroundFetch::backgroundFetch()
{
	if (mBackgroundFetchActive && gAgent.getRegion())
	{
		//If we'll be using the capability, we'll be sending batches and the background thing isn't as important.
		std::string url = gAgent.getRegion()->getCapability("WebFetchInventoryDescendents");   
		if (!url.empty()) 
		{
			bulkFetch(url);
			return;
		}
		
		//DEPRECATED OLD CODE FOLLOWS.
		// no more categories to fetch, stop fetch process
		if (mFetchQueue.empty())
		{
			llinfos << "Inventory fetch completed" << llendl;

			setAllFoldersFetched();
			return;
		}

		F32 fast_fetch_time = lerp(mMinTimeBetweenFetches, mMaxTimeBetweenFetches, 0.1f);
		F32 slow_fetch_time = lerp(mMinTimeBetweenFetches, mMaxTimeBetweenFetches, 0.5f);
		if (mTimelyFetchPending && mFetchTimer.getElapsedTimeF32() > slow_fetch_time)
		{
			// double timeouts on failure
			mMinTimeBetweenFetches = llmin(mMinTimeBetweenFetches * 2.f, 10.f);
			mMaxTimeBetweenFetches = llmin(mMaxTimeBetweenFetches * 2.f, 120.f);
			llinfos << "Inventory fetch times grown to (" << mMinTimeBetweenFetches << ", " << mMaxTimeBetweenFetches << ")" << llendl;
			// fetch is no longer considered "timely" although we will wait for full time-out
			mTimelyFetchPending = FALSE;
		}

		while(1)
		{
			if (mFetchQueue.empty())
			{
				break;
			}

			if(gDisconnected)
			{
				// just bail if we are disconnected.
				break;
			}

			const FetchQueueInfo info = mFetchQueue.front();
			LLViewerInventoryCategory* cat = gInventory.getCategory(info.mCatUUID);

			// category has been deleted, remove from queue.
			if (!cat)
			{
				mFetchQueue.pop_front();
				continue;
			}
			
			if (mFetchTimer.getElapsedTimeF32() > mMinTimeBetweenFetches && 
				LLViewerInventoryCategory::VERSION_UNKNOWN == cat->getVersion())
			{
				// category exists but has no children yet, fetch the descendants
				// for now, just request every time and rely on retry timer to throttle
				if (cat->fetch())
				{
					mFetchTimer.reset();
					mTimelyFetchPending = TRUE;
				}
				else
				{
					//  The catagory also tracks if it has expired and here it says it hasn't
					//  yet.  Get out of here because nothing is going to happen until we
					//  update the timers.
					break;
				}
			}
			// do I have all my children?
			else if (gInventory.isCategoryComplete(info.mCatUUID))
			{
				// finished with this category, remove from queue
				mFetchQueue.pop_front();

				// add all children to queue
				LLInventoryModel::cat_array_t* categories;
				LLInventoryModel::item_array_t* items;
				gInventory.getDirectDescendentsOf(cat->getUUID(), categories, items);
				for (LLInventoryModel::cat_array_t::const_iterator it = categories->begin();
					 it != categories->end();
					 ++it)
				{
					mFetchQueue.push_back(FetchQueueInfo((*it)->getUUID(),info.mRecursive));
				}

				// we received a response in less than the fast time
				if (mTimelyFetchPending && mFetchTimer.getElapsedTimeF32() < fast_fetch_time)
				{
					// shrink timeouts based on success
					mMinTimeBetweenFetches = llmax(mMinTimeBetweenFetches * 0.8f, 0.3f);
					mMaxTimeBetweenFetches = llmax(mMaxTimeBetweenFetches * 0.8f, 10.f);
					//llinfos << "Inventory fetch times shrunk to (" << mMinTimeBetweenFetches << ", " << mMaxTimeBetweenFetches << ")" << llendl;
				}

				mTimelyFetchPending = FALSE;
				continue;
			}
			else if (mFetchTimer.getElapsedTimeF32() > mMaxTimeBetweenFetches)
			{
				// received first packet, but our num descendants does not match db's num descendants
				// so try again later
				mFetchQueue.pop_front();

				if (mNumFetchRetries++ < MAX_FETCH_RETRIES)
				{
					// push on back of queue
					mFetchQueue.push_back(info);
				}
				mTimelyFetchPending = FALSE;
				mFetchTimer.reset();
				break;
			}

			// not enough time has elapsed to do a new fetch
			break;
		}
	}
}

void LLInventoryModelBackgroundFetch::incrBulkFetch(S16 fetching) 
{  
	mBulkFetchCount += fetching; 
	if (mBulkFetchCount < 0)
	{
		mBulkFetchCount = 0; 
	}
}


class LLInventoryModelFetchDescendentsResponder: public LLHTTPClient::Responder
{
public:
	LLInventoryModelFetchDescendentsResponder(const LLSD& request_sd, uuid_vec_t recursive_cats) : 
		mRequestSD(request_sd),
		mRecursiveCatUUIDs(recursive_cats)
	{};
	//LLInventoryModelFetchDescendentsResponder() {};
	void result(const LLSD& content);
	void error(U32 status, const std::string& reason);
protected:
	BOOL getIsRecursive(const LLUUID& cat_id) const;
private:
	LLSD mRequestSD;
	uuid_vec_t mRecursiveCatUUIDs; // Hack for storing away which cat fetches are recursive.
};

//If we get back a normal response, handle it here
void LLInventoryModelFetchDescendentsResponder::result(const LLSD& content)
{
	LLInventoryModelBackgroundFetch *fetcher = LLInventoryModelBackgroundFetch::getInstance();
	if (content.has("folders"))	
	{

		for(LLSD::array_const_iterator folder_it = content["folders"].beginArray();
			folder_it != content["folders"].endArray();
			++folder_it)
		{	
			LLSD folder_sd = *folder_it;
			

			//LLUUID agent_id = folder_sd["agent_id"];

			//if(agent_id != gAgent.getID())	//This should never happen.
			//{
			//	llwarns << "Got a UpdateInventoryItem for the wrong agent."
			//			<< llendl;
			//	break;
			//}

			LLUUID parent_id = folder_sd["folder_id"];
			LLUUID owner_id = folder_sd["owner_id"];
			S32    version  = (S32)folder_sd["version"].asInteger();
			S32    descendents = (S32)folder_sd["descendents"].asInteger();
			LLPointer<LLViewerInventoryCategory> tcategory = new LLViewerInventoryCategory(owner_id);

            if (parent_id.isNull())
            {
			    LLPointer<LLViewerInventoryItem> titem = new LLViewerInventoryItem;
			    for(LLSD::array_const_iterator item_it = folder_sd["items"].beginArray();
				    item_it != folder_sd["items"].endArray();
				    ++item_it)
			    {	
                    const LLUUID lost_uuid = gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);
                    if (lost_uuid.notNull())
                    {
				        LLSD item = *item_it;
				        titem->unpackMessage(item);
				
                        LLInventoryModel::update_list_t update;
                        LLInventoryModel::LLCategoryUpdate new_folder(lost_uuid, 1);
                        update.push_back(new_folder);
                        gInventory.accountForUpdate(update);

                        titem->setParent(lost_uuid);
                        titem->updateParentOnServer(FALSE);
                        gInventory.updateItem(titem);
                        gInventory.notifyObservers("fetchDescendents");
                        
                    }
                }
            }

	        LLViewerInventoryCategory* pcat = gInventory.getCategory(parent_id);
			if (!pcat)
			{
				continue;
			}

			for(LLSD::array_const_iterator category_it = folder_sd["categories"].beginArray();
				category_it != folder_sd["categories"].endArray();
				++category_it)
			{	
				LLSD category = *category_it;
				tcategory->fromLLSD(category); 
				
				const BOOL recursive = getIsRecursive(tcategory->getUUID());
				
				if (recursive)
				{
					fetcher->mFetchQueue.push_back(LLInventoryModelBackgroundFetch::FetchQueueInfo(tcategory->getUUID(), recursive));
				}
				else if ( !gInventory.isCategoryComplete(tcategory->getUUID()) )
				{
					gInventory.updateCategory(tcategory);
				}

			}
			LLPointer<LLViewerInventoryItem> titem = new LLViewerInventoryItem;
			for(LLSD::array_const_iterator item_it = folder_sd["items"].beginArray();
				item_it != folder_sd["items"].endArray();
				++item_it)
			{	
				LLSD item = *item_it;
				titem->unpackMessage(item);
				
				gInventory.updateItem(titem);
			}

			// set version and descendentcount according to message.
			LLViewerInventoryCategory* cat = gInventory.getCategory(parent_id);
			if(cat)
			{
				cat->setVersion(version);
				cat->setDescendentCount(descendents);
				cat->determineFolderType();
			}

		}
	}
		
	if (content.has("bad_folders"))
	{
		for(LLSD::array_const_iterator folder_it = content["bad_folders"].beginArray();
			folder_it != content["bad_folders"].endArray();
			++folder_it)
		{	
			LLSD folder_sd = *folder_it;
			
			//These folders failed on the dataserver.  We probably don't want to retry them.
			llinfos << "Folder " << folder_sd["folder_id"].asString() 
					<< "Error: " << folder_sd["error"].asString() << llendl;
		}
	}

	fetcher->incrBulkFetch(-1);
	
	if (fetcher->isBulkFetchProcessingComplete())
	{
		llinfos << "Inventory fetch completed" << llendl;
		fetcher->setAllFoldersFetched();
	}
	
	gInventory.notifyObservers("fetchDescendents");
}

//If we get back an error (not found, etc...), handle it here
void LLInventoryModelFetchDescendentsResponder::error(U32 status, const std::string& reason)
{
	LLInventoryModelBackgroundFetch *fetcher = LLInventoryModelBackgroundFetch::getInstance();

	llinfos << "LLInventoryModelFetchDescendentsResponder::error "
		<< status << ": " << reason << llendl;
						
	fetcher->incrBulkFetch(-1);

	if (status==499) // Timed out.
	{
		for(LLSD::array_const_iterator folder_it = mRequestSD["folders"].beginArray();
			folder_it != mRequestSD["folders"].endArray();
			++folder_it)
		{	
			LLSD folder_sd = *folder_it;
			LLUUID folder_id = folder_sd["folder_id"];
			const BOOL recursive = getIsRecursive(folder_id);
			fetcher->mFetchQueue.push_front(LLInventoryModelBackgroundFetch::FetchQueueInfo(folder_id, recursive));
		}
	}
	else
	{
		if (fetcher->isBulkFetchProcessingComplete())
		{
			fetcher->setAllFoldersFetched();
		}
	}
	gInventory.notifyObservers("fetchDescendents");
}

BOOL LLInventoryModelFetchDescendentsResponder::getIsRecursive(const LLUUID& cat_id) const
{
	return (std::find(mRecursiveCatUUIDs.begin(),mRecursiveCatUUIDs.end(), cat_id) != mRecursiveCatUUIDs.end());
}

//static   Bundle up a bunch of requests to send all at once.
void LLInventoryModelBackgroundFetch::bulkFetch(std::string url)
{
	//Background fetch is called from gIdleCallbacks in a loop until background fetch is stopped.
	//If there are items in mFetchQueue, we want to check the time since the last bulkFetch was 
	//sent.  If it exceeds our retry time, go ahead and fire off another batch.  
	//Stopbackgroundfetch will be run from the Responder instead of here.  

	S16 max_concurrent_fetches=8;
	F32 new_min_time = 0.5f;			//HACK!  Clean this up when old code goes away entirely.
	if (mMinTimeBetweenFetches < new_min_time) 
	{
		mMinTimeBetweenFetches=new_min_time;  //HACK!  See above.
	}
	
	if (gDisconnected ||
		(mBulkFetchCount > max_concurrent_fetches) ||
		(mFetchTimer.getElapsedTimeF32() < mMinTimeBetweenFetches))
	{
		return; // just bail if we are disconnected.
	}	

	U32 folder_count=0;
	U32 max_batch_size=5;

	U32 sort_order = gSavedSettings.getU32(LLInventoryPanel::DEFAULT_SORT_ORDER) & 0x1;

	uuid_vec_t recursive_cats;

	LLSD body;
	LLSD body_lib;

	while (!(mFetchQueue.empty()) && (folder_count < max_batch_size))
	{
		const FetchQueueInfo& fetch_info = mFetchQueue.front();
		const LLUUID &cat_id = fetch_info.mCatUUID;
        if (cat_id.isNull()) //DEV-17797
        {
			LLSD folder_sd;
			folder_sd["folder_id"]		= LLUUID::null.asString();
			folder_sd["owner_id"]		= gAgent.getID();
			folder_sd["sort_order"]		= (LLSD::Integer)sort_order;
			folder_sd["fetch_folders"]	= (LLSD::Boolean)FALSE;
			folder_sd["fetch_items"]	= (LLSD::Boolean)TRUE;
			body["folders"].append(folder_sd);
            folder_count++;
        }
        else
        {
		    const LLViewerInventoryCategory* cat = gInventory.getCategory(cat_id);
		
		    if (cat)
		    {
			    if (LLViewerInventoryCategory::VERSION_UNKNOWN == cat->getVersion())
			    {
				    LLSD folder_sd;
				    folder_sd["folder_id"]		= cat->getUUID();
				    folder_sd["owner_id"]		= cat->getOwnerID();
				    folder_sd["sort_order"]		= (LLSD::Integer)sort_order;
				    folder_sd["fetch_folders"]	= TRUE; //(LLSD::Boolean)sFullFetchStarted;
				    folder_sd["fetch_items"]	= (LLSD::Boolean)TRUE;
				    
				    if (ALEXANDRIA_LINDEN_ID == cat->getOwnerID())
					    body_lib["folders"].append(folder_sd);
				    else
					    body["folders"].append(folder_sd);
				    folder_count++;
			    }
				// May already have this folder, but append child folders to list.
			    if (fetch_info.mRecursive)
			    {	
					LLInventoryModel::cat_array_t* categories;
					LLInventoryModel::item_array_t* items;
					gInventory.getDirectDescendentsOf(cat->getUUID(), categories, items);
					for (LLInventoryModel::cat_array_t::const_iterator it = categories->begin();
						 it != categories->end();
						 ++it)
					{
						mFetchQueue.push_back(FetchQueueInfo((*it)->getUUID(), fetch_info.mRecursive));
				    }
			    }
		    }
        }
		if (fetch_info.mRecursive)
			recursive_cats.push_back(cat_id);

		mFetchQueue.pop_front();
	}
		
	if (folder_count > 0)
	{
		mBulkFetchCount++;
		if (body["folders"].size())
		{
			LLInventoryModelFetchDescendentsResponder *fetcher = new LLInventoryModelFetchDescendentsResponder(body, recursive_cats);
			LLHTTPClient::post(url, body, fetcher, 300.0);
		}
		if (body_lib["folders"].size())
		{
			std::string url_lib = gAgent.getRegion()->getCapability("FetchLibDescendents");
			
			LLInventoryModelFetchDescendentsResponder *fetcher = new LLInventoryModelFetchDescendentsResponder(body_lib, recursive_cats);
			LLHTTPClient::post(url_lib, body_lib, fetcher, 300.0);
		}
		mFetchTimer.reset();
	}
	else if (isBulkFetchProcessingComplete())
	{
		setAllFoldersFetched();
	}
}

bool LLInventoryModelBackgroundFetch::fetchQueueContainsNoDescendentsOf(const LLUUID& cat_id) const
{
	for (fetch_queue_t::const_iterator it = mFetchQueue.begin();
		 it != mFetchQueue.end(); ++it)
	{
		const LLUUID& fetch_id = (*it).mCatUUID;
		if (gInventory.isObjectDescendentOf(fetch_id, cat_id))
			return false;
	}
	return true;
}


