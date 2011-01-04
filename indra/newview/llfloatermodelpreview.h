/**
 * @file llfloatermodelpreview.h
 * @brief LLFloaterModelPreview class definition
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

#ifndef LL_LLFLOATERMODELPREVIEW_H
#define LL_LLFLOATERMODELPREVIEW_H

#include "llfloaternamedesc.h"

#include "lldynamictexture.h"
#include "llfloatermodelwizard.h"
#include "llquaternion.h"
#include "llmeshrepository.h"
#include "llmodel.h"
#include "llthread.h"
#include "llviewermenufile.h"

class LLComboBox;
class LLJoint;
class LLViewerJointMesh;
class LLVOAvatar;
class LLTextBox;
class LLVertexBuffer;
class LLModelPreview;
class LLFloaterModelPreview;
class daeElement;
class domProfile_COMMON;
class domInstance_geometry;
class domNode;
class domTranslate;
class LLMenuButton;
class LLToggleableMenu;

const S32 NUM_LOD = 4;

class LLModelLoader : public LLThread
{
public:
	typedef enum
	{
		STARTING = 0,
		READING_FILE,
		CREATING_FACES,
		GENERATING_VERTEX_BUFFERS,
		GENERATING_LOD,
		DONE,
		ERROR_PARSING, //basically loading failed
	} eLoadState;

	U32 mState;
	std::string mFilename;
	S32 mLod;
	LLModelPreview* mPreview;
	LLMatrix4 mTransform;
	BOOL mFirstTransform;
	LLVector3 mExtents[2];

	std::map<daeElement*, LLPointer<LLModel> > mModel;
	
	typedef std::vector<LLPointer<LLModel> > model_list;
	model_list mModelList;

	typedef std::vector<LLModelInstance> model_instance_list;
	
	typedef std::map<LLMatrix4, model_instance_list > scene;

	scene mScene;

	typedef std::queue<LLPointer<LLModel> > model_queue;

	//queue of models that need a physics rep
	model_queue mPhysicsQ;

	LLModelLoader(std::string filename, S32 lod, LLModelPreview* preview);

	virtual void run();
	
	void processElement(daeElement* element);
	std::vector<LLImportMaterial> getMaterials(LLModel* model, domInstance_geometry* instance_geo);
	LLImportMaterial profileToMaterial(domProfile_COMMON* material);
	std::string getElementLabel(daeElement *element);
	LLColor4 getDaeColor(daeElement* element);
	
	daeElement* getChildFromElement( daeElement* pElement, std::string const & name );
	
	bool isNodeAJoint( domNode* pNode );
	void processJointNode( domNode* pNode, std::map<std::string,LLMatrix4>& jointTransforms );
	void extractTranslation( domTranslate* pTranslate, LLMatrix4& transform );
	void extractTranslationViaElement( daeElement* pTranslateElement, LLMatrix4& transform );
	
	void setLoadState( U32 state ) { mState = state; }
	U32 getLoadState( void ) { return mState; }
	
	//map of avatar joints as named in COLLADA assets to internal joint names
	std::map<std::string, std::string> mJointMap;
};

class LLFloaterModelPreview : public LLFloater
{
public:
	
	class DecompRequest : public LLPhysicsDecomp::Request
	{
	public:
		S32 mContinue;
		LLPointer<LLModel> mModel;
		
		DecompRequest(const std::string& stage, LLModel* mdl);
		virtual S32 statusCallback(const char* status, S32 p1, S32 p2);
		virtual void completed();
		
	};
	static LLFloaterModelPreview* sInstance;
	
	LLFloaterModelPreview(const LLSD& key);
	virtual ~LLFloaterModelPreview();
	
	virtual BOOL postBuild();
	
	BOOL handleMouseDown(S32 x, S32 y, MASK mask);
	BOOL handleMouseUp(S32 x, S32 y, MASK mask);
	BOOL handleHover(S32 x, S32 y, MASK mask);
	BOOL handleScrollWheel(S32 x, S32 y, S32 clicks); 
	
	static void onMouseCaptureLostModelPreview(LLMouseHandler*);
	static void setUploadAmount(S32 amount) { sUploadAmount = amount; }
	
	static void onBrowseLOD(void* data);
	
	static void onUpload(void* data);
	
	static void onClearMaterials(void* data);
	
	static void refresh(LLUICtrl* ctrl, void* data);
	
	void updateResourceCost();
	
	void			loadModel(S32 lod);
	
	void onViewOptionChecked(const LLSD& userdata);
	bool isViewOptionChecked(const LLSD& userdata);
	bool isViewOptionEnabled(const LLSD& userdata);
	void setViewOptionEnabled(const std::string& option, bool enabled);
	void enableViewOption(const std::string& option);
	void disableViewOption(const std::string& option);

protected:
	friend class LLModelPreview;
	friend class LLMeshFilePicker;
	friend class LLPhysicsDecomp;
	
	static void		onImportScaleCommit(LLUICtrl*, void*);
	static void		onUploadJointsCommit(LLUICtrl*,void*);
	static void		onUploadSkinCommit(LLUICtrl*,void*);
	
	static void		onPreviewLODCommit(LLUICtrl*,void*);
	
	static void		onGenerateNormalsCommit(LLUICtrl*,void*);
	
	static void		onAutoFillCommit(LLUICtrl*,void*);
	static void		onLODParamCommit(LLUICtrl*,void*);
	
	static void		onExplodeCommit(LLUICtrl*, void*);
	
	static void onPhysicsParamCommit(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsStageExecute(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsStageCancel(LLUICtrl* ctrl, void* userdata);
	
	static void onPhysicsBrowse(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsUseLOD(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsOptimize(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsDecomposeBack(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsSimplifyBack(LLUICtrl* ctrl, void* userdata);
		
	void			draw();
	
	void initDecompControls();
	
	void setStatusMessage(const std::string& msg);

	LLModelPreview*	mModelPreview;
	
	LLPhysicsDecomp::decomp_params mDecompParams;
	
	S32				mLastMouseX;
	S32				mLastMouseY;
	LLRect			mPreviewRect;
	U32				mGLName;
	static S32		sUploadAmount;
	
	std::set<LLPointer<DecompRequest> > mCurRequest;
	std::string mStatusMessage;

	//use "disabled" as false by default
	std::map<std::string, bool> mViewOptionDisabled;
	
	//store which lod mode each LOD is using
	// 0 - load from file
	// 1 - auto generate
	// 2 - None
	S32 mLODMode[4];

	LLMenuButton* mViewOptionMenuButton;
	LLToggleableMenu* mViewOptionMenu;
	LLMutex* mStatusLock;
};

class LLMeshFilePicker : public LLFilePickerThread
{
public:
	LLMeshFilePicker(LLModelPreview* mp, S32 lod);
	virtual void notify(const std::string& filename);

private:
	LLModelPreview* mMP;
	S32 mLOD;
};


class LLModelPreview : public LLViewerDynamicTexture, public LLMutex
{
 public:
	
	 LLModelPreview(S32 width, S32 height, LLFloater* fmp);
	virtual ~LLModelPreview();

	void resetPreviewTarget();
	void setPreviewTarget(F32 distance);
	void setTexture(U32 name) { mTextureName = name; }

	void setPhysicsFromLOD(S32 lod);
	BOOL render();
	void update();
	void genBuffers(S32 lod, bool skinned);
	void clearBuffers();
	void refresh();
	void rotate(F32 yaw_radians, F32 pitch_radians);
	void zoom(F32 zoom_amt);
	void pan(F32 right, F32 up);
	virtual BOOL needsRender() { return mNeedsUpdate; }
	void setPreviewLOD(S32 lod);
	void clearModel(S32 lod);
	void loadModel(std::string filename, S32 lod);
	void loadModelCallback(S32 lod);
	void genLODs(S32 which_lod = -1, U32 decimation = 3);
	void generateNormals();
	void consolidate();
	void clearMaterials();
	U32 calcResourceCost();
	void rebuildUploadData();
	void clearIncompatible(S32 lod);
	void updateStatusMessages();
	bool containsRiggedAsset( void );
	void clearGLODGroup();


	static void	textureLoadedCallback( BOOL success, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* src_aux, S32 discard_level, BOOL final, void* userdata );

 protected:
	friend class LLFloaterModelPreview;
	friend class LLFloaterModelWizard;
	friend class LLFloaterModelPreview::DecompRequest;
	friend class LLFloaterModelWizard::DecompRequest;
	friend class LLPhysicsDecomp;

	LLFloater*  mFMP;

	BOOL        mNeedsUpdate;
	bool		mDirty;
	bool		mGenLOD;
	U32         mTextureName;
	F32			mCameraDistance;
	F32			mCameraYaw;
	F32			mCameraPitch;
	F32			mCameraZoom;
	LLVector3	mCameraOffset;
	LLVector3	mPreviewTarget;
	LLVector3	mPreviewScale;
	S32			mPreviewLOD;
	U32			mResourceCost;
	std::string mLODFile[LLModel::NUM_LODS];
	bool		mLoading;

	std::map<std::string, bool> mViewOption;

	//GLOD object parameters (must rebuild object if these change)
	F32 mBuildShareTolerance;
	U32 mBuildQueueMode;
	U32 mBuildOperator;
	U32 mBuildBorderMode;
	
	LLModelLoader* mModelLoader;

	LLModelLoader::scene mScene[LLModel::NUM_LODS];
	LLModelLoader::scene mBaseScene;

	LLModelLoader::model_list mModel[LLModel::NUM_LODS];
	LLModelLoader::model_list mBaseModel;

	U32 mGroup;
	std::map<LLPointer<LLModel>, U32> mObject;
	U32 mMaxTriangleLimit;
	std::map<LLPointer<LLModel>, std::vector<LLPointer<LLVertexBuffer> > > mPhysicsMesh;

	LLMeshUploadThread::instance_list mUploadData;
	std::set<LLPointer<LLViewerFetchedTexture> > mTextureSet;

	//map of vertex buffers to models (one vertex buffer in vector per face in model
	std::map<LLModel*, std::vector<LLPointer<LLVertexBuffer> > > mVertexBuffer[LLModel::NUM_LODS+1];
};


#endif  // LL_LLFLOATERMODELPREVIEW_H
