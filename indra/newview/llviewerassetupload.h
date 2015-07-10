/**
* @file llviewerassetupload.h
* @author optional
* @brief brief description of the file
*
* $LicenseInfo:firstyear=2011&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2011, Linden Research, Inc.
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

#ifndef LL_VIEWER_ASSET_UPLOAD_H
#define LL_VIEWER_ASSET_UPLOAD_H

#include "llfoldertype.h"
#include "llassettype.h"
#include "llinventorytype.h"
#include "lleventcoro.h"
#include "llcoros.h"
#include "llcorehttputil.h"

class NewResourceUploadInfo
{
public:
    typedef boost::shared_ptr<NewResourceUploadInfo> ptr_t;

    NewResourceUploadInfo(
            LLTransactionID transactId,
            LLAssetType::EType assetType,
            std::string name,
            std::string description,
            S32 compressionInfo,
            LLFolderType::EType destinationType,
            LLInventoryType::EType inventoryType,
            U32 nextOWnerPerms,
            U32 groupPerms,
            U32 everyonePerms,
            S32 expectedCost) :
        mTransactionId(transactId),
        mAssetType(assetType),
        mName(name),
        mDescription(description),
        mCompressionInfo(compressionInfo),
        mDestinationFolderType(destinationType),
        mInventoryType(inventoryType),
        mNextOwnerPerms(nextOWnerPerms),
        mGroupPerms(groupPerms),
        mEveryonePerms(everyonePerms),
        mExpectedUploadCost(expectedCost),
        mFolderId(LLUUID::null),
        mItemId(LLUUID::null),
        mAssetId(LLAssetID::null)
    { }

    virtual ~NewResourceUploadInfo()
    { }

    virtual LLSD        prepareUpload();
    virtual LLSD        generatePostBody();
    virtual void        logPreparedUpload();
    virtual LLUUID      finishUpload(LLSD &result);

    LLTransactionID     getTransactionId() const { return mTransactionId; }
    LLAssetType::EType  getAssetType() const { return mAssetType; }
    std::string         getAssetTypeString() const;
    std::string         getName() const { return mName; };
    std::string         getDescription() const { return mDescription; };
    S32                 getCompressionInfo() const { return mCompressionInfo; };
    LLFolderType::EType getDestinationFolderType() const { return mDestinationFolderType; };
    LLInventoryType::EType  getInventoryType() const { return mInventoryType; };
    std::string         getInventoryTypeString() const;
    U32                 getNextOwnerPerms() const { return mNextOwnerPerms; };
    U32                 getGroupPerms() const { return mGroupPerms; };
    U32                 getEveryonePerms() const { return mEveryonePerms; };
    S32                 getExpectedUploadCost() const { return mExpectedUploadCost; };

    virtual std::string getDisplayName() const;

    LLUUID              getFolderId() const { return mFolderId; }
    LLUUID              getItemId() const { return mItemId; }
    LLAssetID           getAssetId() const { return mAssetId; }

protected:
    NewResourceUploadInfo(
            std::string name,
            std::string description,
            S32 compressionInfo,
            LLFolderType::EType destinationType,
            LLInventoryType::EType inventoryType,
            U32 nextOWnerPerms,
            U32 groupPerms,
            U32 everyonePerms,
            S32 expectedCost) :
        mName(name),
        mDescription(description),
        mCompressionInfo(compressionInfo),
        mDestinationFolderType(destinationType),
        mInventoryType(inventoryType),
        mNextOwnerPerms(nextOWnerPerms),
        mGroupPerms(groupPerms),
        mEveryonePerms(everyonePerms),
        mExpectedUploadCost(expectedCost),
        mTransactionId(),
        mAssetType(LLAssetType::AT_NONE),
        mFolderId(LLUUID::null),
        mItemId(LLUUID::null),
        mAssetId(LLAssetID::null)
    { }

    void                setTransactionId(LLTransactionID tid) { mTransactionId = tid; }
    void                setAssetType(LLAssetType::EType assetType) { mAssetType = assetType; }

    LLAssetID           generateNewAssetId();
    void                incrementUploadStats() const;
    virtual void        assignDefaults();

private:
    LLTransactionID     mTransactionId;
    LLAssetType::EType  mAssetType;
    std::string         mName;
    std::string         mDescription;
    S32                 mCompressionInfo;
    LLFolderType::EType mDestinationFolderType;
    LLInventoryType::EType mInventoryType;
    U32                 mNextOwnerPerms;
    U32                 mGroupPerms;
    U32                 mEveryonePerms;
    S32                 mExpectedUploadCost;

    LLUUID              mFolderId;
    LLUUID              mItemId;
    LLAssetID           mAssetId;
};

class NewFileResourceUploadInfo : public NewResourceUploadInfo
{
public:
    NewFileResourceUploadInfo(
        std::string fileName,
        std::string name,
        std::string description,
        S32 compressionInfo,
        LLFolderType::EType destinationType,
        LLInventoryType::EType inventoryType,
        U32 nextOWnerPerms,
        U32 groupPerms,
        U32 everyonePerms,
        S32 expectedCost);

    virtual LLSD        prepareUpload();

    std::string         getFileName() const { return mFileName; };

protected:

    virtual LLSD        exportTempFile();

private:
    std::string         mFileName;

};

#if 0
class NotecardResourceUploadInfo : public NewResourceUploadInfo
{
public:
    NotecardResourceUploadInfo(
        );


protected:

private:
};
#endif

class LLViewerAssetUpload
{
public:

    static void AssetInventoryUploadCoproc(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t &httpAdapter, const LLUUID &id,
        std::string url, NewResourceUploadInfo::ptr_t uploadInfo);

private:
    static void HandleUploadError(LLCore::HttpStatus status, LLSD &result, NewResourceUploadInfo::ptr_t &uploadInfo);
};

#endif // !VIEWER_ASSET_UPLOAD_H
