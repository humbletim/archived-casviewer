/** 
 * @file pipeline.h
 * @brief Rendering pipeline definitions
 *
 * Copyright (c) 2001-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

#ifndef LL_PIPELINE_H
#define LL_PIPELINE_H

#include "lldarrayptr.h"
#include "lldqueueptr.h"
#include "llstat.h"
#include "lllinkedqueue.h"
#include "llskiplist.h"
#include "lldrawpool.h"
#include "llspatialpartition.h"
#include "m4math.h"
#include "llmemory.h"
#include "lldrawpool.h"
#include "llgl.h"

class LLViewerImage;
class LLDrawable;
class LLEdge;
class LLFace;
class LLViewerObject;
class LLAgent;
class LLDisplayPrimitive;
class LLTextureEntry;
class LLRenderFunc;
class LLCubeMap;

typedef enum e_avatar_skinning_method
{
	SKIN_METHOD_SOFTWARE,
	SKIN_METHOD_VERTEX_PROGRAM
} EAvatarSkinningMethod;

BOOL compute_min_max(LLMatrix4& box, LLVector2& min, LLVector2& max); // Shouldn't be defined here!
bool LLRayAABB(const LLVector3 &center, const LLVector3 &size, const LLVector3& origin, const LLVector3& dir, LLVector3 &coord, F32 epsilon = 0);
BOOL LLLineSegmentAABB(const LLVector3& start, const LLVector3& end, const LLVector3& center, const LLVector3& size);
BOOL setup_hud_matrices(BOOL for_select);

class LLGLSLShader
{
public:
	LLGLSLShader();

	void unload();
	void attachObject(GLhandleARB object);
	void attachObjects(GLhandleARB* objects = NULL, S32 count = 0);
	BOOL mapAttributes(const char** attrib_names = NULL, S32 count = 0);
	BOOL mapUniforms(const char** uniform_names = NULL,  S32 count = 0);
	void mapUniform(GLint index, const char** uniform_names = NULL,  S32 count = 0);
	void vertexAttrib4f(U32 index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
	void vertexAttrib4fv(U32 index, GLfloat* v);
	
	GLint mapUniformTextureChannel(GLint location, GLenum type);
	

	//enable/disable texture channel for specified uniform
	//if given texture uniform is active in the shader, 
	//the corresponding channel will be active upon return
	//returns channel texture is enabled in from [0-MAX)
	S32 enableTexture(S32 uniform, S32 mode = GL_TEXTURE_2D);
	S32 disableTexture(S32 uniform, S32 mode = GL_TEXTURE_2D); 
	
    BOOL link(BOOL suppress_errors = FALSE);
	void bind();
	void unbind();

	GLhandleARB mProgramObject;
	std::vector<GLint> mAttribute;
	std::vector<GLint> mUniform;
	std::vector<GLint> mTexture;
	S32 mActiveTextureChannels;
};

class LLPipeline
{
public:
	LLPipeline();
	~LLPipeline();

	void destroyGL();
	void restoreGL();
	void resetVertexBuffers();
	void resetVertexBuffers(LLDrawable* drawable);
	void setUseVBO(BOOL use_vbo);
	void generateReflectionMap(LLCubeMap* cube_map, LLCamera& camera, GLsizei res);
	void blurReflectionMap(LLCubeMap* cube_in, LLCubeMap* cube_out, U32 res);

	void init();
	void cleanup();

	/// @brief Get a draw pool from pool type (POOL_SIMPLE, POOL_MEDIA) and texture.
	/// @return Draw pool, or NULL if not found.
	LLDrawPool *findPool(const U32 pool_type, LLViewerImage *tex0 = NULL);

	/// @brief Get a draw pool for faces of the appropriate type and texture.  Create if necessary.
	/// @return Always returns a draw pool.
	LLDrawPool *getPool(const U32 pool_type, LLViewerImage *tex0 = NULL);

	/// @brief Figures out draw pool type from texture entry. Creates pool if necessary.
	static LLDrawPool* getPoolFromTE(const LLTextureEntry* te, LLViewerImage* te_image);
	static U32 getPoolTypeFromTE(const LLTextureEntry* te, LLViewerImage* imagep);

	void		 addPool(LLDrawPool *poolp);	// Only to be used by LLDrawPool classes for splitting pools!
	void		 removePool( LLDrawPool* poolp );

	void		 allocDrawable(LLViewerObject *obj);

	void		 unlinkDrawable(LLDrawable*);

	// Object related methods
	void        markVisible(LLDrawable *drawablep, LLCamera& camera);
	void		doOcclusion(LLCamera& camera);
	void		markNotCulled(LLSpatialGroup* group, LLCamera &camera, BOOL active = FALSE);
	void        markMoved(LLDrawable *drawablep, BOOL damped_motion = FALSE);
	void        markShift(LLDrawable *drawablep);
	void        markTextured(LLDrawable *drawablep);
	void        markRebuild(LLDrawable *drawablep, LLDrawable::EDrawableFlags flag = LLDrawable::REBUILD_ALL, BOOL priority = FALSE);
	void		markRebuild(LLSpatialGroup* groupp);
	void        markRelight(LLDrawable *drawablep, const BOOL now = FALSE);
	
	//get the object between start and end that's closest to start.  Return the point of collision in collision.
	LLViewerObject* pickObject(const LLVector3 &start, const LLVector3 &end, LLVector3 &collision);

	// Something about these textures has changed.  Dirty them.
	void        dirtyPoolObjectTextures(const std::set<LLViewerImage*>& textures);

	void        resetDrawOrders();

	U32         addObject(LLViewerObject *obj);

	void		enableShadows(const BOOL enable_shadows);

// 	void		setLocalLighting(const BOOL local_lighting);
// 	BOOL		isLocalLightingEnabled() const;
	S32			setLightingDetail(S32 level);
	S32			getLightingDetail() const { return mLightingDetail; }
	S32			getMaxLightingDetail() const;
		
	void		setUseVertexShaders(BOOL use_shaders);
	BOOL		getUseVertexShaders() const { return mVertexShadersEnabled; }
	BOOL		canUseVertexShaders();
	BOOL		setVertexShaderLevel(S32 type, S32 level);
	S32			getVertexShaderLevel(S32 type) const { return mVertexShaderLevel[type]; }
	S32			getMaxVertexShaderLevel(S32 type) const { return mMaxVertexShaderLevel[type]; }

	void		setShaders();
	
	void		dumpObjectLog(GLhandleARB ret, BOOL warns = TRUE);
	BOOL		linkProgramObject(GLhandleARB obj, BOOL suppress_errors = FALSE);
	BOOL		validateProgramObject(GLhandleARB obj);
	GLhandleARB loadShader(const LLString& filename, S32 cls, GLenum type);

	// phases
	void resetFrameStats();

	void updateMoveDampedAsync(LLDrawable* drawablep);
	void updateMoveNormalAsync(LLDrawable* drawablep);
	void updateMovedList(LLDrawable::drawable_vector_t& move_list);
	void updateMove();
	void updateCull(LLCamera& camera);
	void updateGeom(F32 max_dtime);

	//calculate pixel area of given box from vantage point of given camera
	static F32 calcPixelArea(LLVector3 center, LLVector3 size, LLCamera& camera);

	void stateSort(LLCamera& camera);
	void stateSort(LLSpatialGroup* group, LLCamera& camera);
	void stateSort(LLSpatialBridge* bridge, LLCamera& camera);
	void stateSort(LLDrawable* drawablep, LLCamera& camera);
	void postSort(LLCamera& camera);
	void forAllDrawables(LLSpatialGroup::sg_vector_t& groups, void (*func)(LLDrawable*));
	void forAllVisibleDrawables(void (*func)(LLDrawable*));
	static void highlightPhysical(LLDrawable* drawablep);

	void renderObjects(U32 type, U32 mask, BOOL texture = TRUE);

	void renderGeom(LLCamera& camera);
	void renderHighlights();
	void renderDebug();
	void processGeometry(LLCamera& camera);
	void processOcclusion(LLCamera& camera);

	void renderForSelect(std::set<LLViewerObject*>& objects);
	void renderFaceForUVSelect(LLFace* facep);
	void rebuildPools(); // Rebuild pools

	void findReferences(LLDrawable *drawablep);	// Find the lists which have references to this object
	BOOL verify();						// Verify that all data in the pipeline is "correct"

	S32  getVisibleCount() const { return mVisibleList.size(); }
	S32  getLightCount() const { return mLights.size(); }

	void calcNearbyLights();
	void setupHWLights(LLDrawPool* pool);
	void setupAvatarLights(BOOL for_edit = FALSE);
	void enableLights(U32 mask, F32 shadow_factor);
	void enableLightsStatic(F32 shadow_factor);
	void enableLightsDynamic(F32 shadow_factor);
	void enableLightsAvatar(F32 shadow_factor);
	void enableLightsAvatarEdit(const LLColor4& color);
	void enableLightsFullbright(const LLColor4& color);
	void disableLights();
	void setAmbient(const LLColor4& ambient);

	void shiftObjects(const LLVector3 &offset);

	void setLight(LLDrawable *drawablep, BOOL is_light);
	void setActive(LLDrawable *drawablep, BOOL active);

	BOOL hasRenderType(const U32 type) const				{ return (type && (mRenderTypeMask & (1<<type))) ? TRUE : FALSE; }
	BOOL hasRenderDebugFeatureMask(const U32 mask) const	{ return (mRenderDebugFeatureMask & mask) ? TRUE : FALSE; }
	BOOL hasRenderFeatureMask(const U32 mask) const			{ return (mRenderFeatureMask & mask) ? TRUE : FALSE; }
	BOOL hasRenderDebugMask(const U32 mask) const			{ return (mRenderDebugMask & mask) ? TRUE : FALSE; }
	void setRenderTypeMask(const U32 mask)					{ mRenderTypeMask = mask; }
	U32  getRenderTypeMask() const							{ return mRenderTypeMask; }
	static void toggleRenderType(U32 type);

	// For UI control of render features
	static BOOL hasRenderTypeControl(void* data);
	static void toggleRenderDebug(void* data);
	static void toggleRenderDebugFeature(void* data);
	static void toggleRenderTypeControl(void* data);
	static BOOL toggleRenderTypeControlNegated(void* data);
	static BOOL toggleRenderDebugControl(void* data);
	static BOOL toggleRenderDebugFeatureControl(void* data);

	static void toggleRenderParticleBeacons(void* data);
	static BOOL getRenderParticleBeacons(void* data);

	static void toggleRenderSoundBeacons(void* data);
	static BOOL getRenderSoundBeacons(void* data);

	static void toggleRenderPhysicalBeacons(void* data);
	static BOOL getRenderPhysicalBeacons(void* data);

	static void toggleRenderScriptedBeacons(void* data);
	static BOOL getRenderScriptedBeacons(void* data);

private:
	void initShaders(BOOL force);
	void unloadShaders();
	BOOL loadShaders();
	BOOL loadShadersLighting();
	BOOL loadShadersObject();
	BOOL loadShadersAvatar();
	BOOL loadShadersEnvironment();
	BOOL loadShadersInterface();
	void saveVertexShaderLevel(S32 type, S32 level, S32 max);
	void addToQuickLookup( LLDrawPool* new_poolp );
	void removeFromQuickLookup( LLDrawPool* poolp );
	BOOL updateDrawableGeom(LLDrawable* drawable, BOOL priority);
	
public:
	enum {GPU_CLASS_MAX = 3 };
	enum EShaderClass
	{
		SHADER_LIGHTING,
		SHADER_OBJECT,
		SHADER_AVATAR,
		SHADER_ENVIRONMENT,
		SHADER_INTERFACE,
		SHADER_COUNT
	};
	enum LLRenderTypeMask
	{
		// Following are pool types (some are also object types)
		RENDER_TYPE_SKY			= LLDrawPool::POOL_SKY,
		RENDER_TYPE_STARS		= LLDrawPool::POOL_STARS,
		RENDER_TYPE_GROUND		= LLDrawPool::POOL_GROUND,	
		RENDER_TYPE_TERRAIN		= LLDrawPool::POOL_TERRAIN,
		RENDER_TYPE_SIMPLE		= LLDrawPool::POOL_SIMPLE,
		RENDER_TYPE_BUMP		= LLDrawPool::POOL_BUMP,
		RENDER_TYPE_AVATAR		= LLDrawPool::POOL_AVATAR,
		RENDER_TYPE_TREE		= LLDrawPool::POOL_TREE,
		RENDER_TYPE_WATER		= LLDrawPool::POOL_WATER,
 		RENDER_TYPE_ALPHA		= LLDrawPool::POOL_ALPHA,
		// Following are object types (only used in drawable mRenderType)
		RENDER_TYPE_HUD = LLDrawPool::NUM_POOL_TYPES,
		RENDER_TYPE_VOLUME,
		RENDER_TYPE_GRASS,
		RENDER_TYPE_PARTICLES,
		RENDER_TYPE_CLOUDS,
	};

	enum LLRenderDebugFeatureMask
	{
		RENDER_DEBUG_FEATURE_UI					= 0x0001,
		RENDER_DEBUG_FEATURE_SELECTED			= 0x0002,
		RENDER_DEBUG_FEATURE_HIGHLIGHTED		= 0x0004,
		RENDER_DEBUG_FEATURE_DYNAMIC_TEXTURES	= 0x0008,
// 		RENDER_DEBUG_FEATURE_HW_LIGHTING		= 0x0010,
		RENDER_DEBUG_FEATURE_FLEXIBLE			= 0x0010,
		RENDER_DEBUG_FEATURE_FOG				= 0x0020,
		RENDER_DEBUG_FEATURE_PALETTE			= 0x0040,
		RENDER_DEBUG_FEATURE_FR_INFO			= 0x0080,
		RENDER_DEBUG_FEATURE_FOOT_SHADOWS		= 0x0100,
	};

	enum LLRenderFeatureMask
	{
		RENDER_FEATURE_LOCAL_LIGHTING		= 0x02,
		RENDER_FEATURE_OBJECT_BUMP			= 0x04,
		RENDER_FEATURE_AVATAR_BUMP			= 0x08,
// 		RENDER_FEATURE_SHADOWS				= 0x10,
		RENDER_FEATURE_RIPPLE_WATER			= 0X20
	};

	enum LLRenderDebugMask
	{
		RENDER_DEBUG_LIGHT_TRACE		= 0x00001,
		RENDER_DEBUG_COMPOSITION		= 0x00020,
		RENDER_DEBUG_VERIFY				= 0x00080,
		RENDER_DEBUG_SHADOW_MAP			= 0x00100,
		RENDER_DEBUG_BBOXES				= 0x00200,
		RENDER_DEBUG_OCTREE				= 0x00400,
		RENDER_DEBUG_PICKING			= 0x00800,
		RENDER_DEBUG_OCCLUSION			= 0x01000,
		RENDER_DEBUG_POINTS				= 0x02000,
		RENDER_DEBUG_TEXTURE_PRIORITY	= 0x04000,
		RENDER_DEBUG_TEXTURE_AREA		= 0x08000,
		RENDER_DEBUG_PARTICLES			= 0x10000,
	};

	LLPointer<LLViewerImage>	mAlphaSizzleImagep;

	//MUST MATCH THE ORDER OF DECLARATION IN LLPipeline::init()
	typedef enum 
	{
		PARTITION_VOLUME = 0,
		PARTITION_BRIDGE,
		PARTITION_HUD,
		PARTITION_TERRAIN,
		PARTITION_WATER,
		PARTITION_TREE,
		PARTITION_PARTICLE,
		PARTITION_CLOUD,
		PARTITION_GRASS,
		PARTITION_NONE,
		NUM_PARTITIONS
	} eObjectPartitions;

private:
	std::vector<LLSpatialPartition*> mObjectPartition;
public:
	
	LLSpatialPartition* getSpatialPartition(LLViewerObject* vobj);
	LLSpatialPartition* getSpatialPartition(U32 index);

	BOOL					 mBackfaceCull;
	S32						 mTrianglesDrawn;
	LLStat                   mTrianglesDrawnStat;
	LLStat					 mCompilesStat;
	S32						 mVerticesRelit;
	LLStat					 mVerticesRelitStat;

	S32						 mLightingChanges;
	LLStat					 mLightingChangesStat;
	S32						 mGeometryChanges;
	LLStat					 mGeometryChangesStat;
	LLStat					 mMoveChangesStat;

	S32						 mNumVisibleFaces;
	LLStat					 mNumVisibleFacesStat;
	LLStat					 mNumVisibleDrawablesStat;

	static S32				sCompiles;

	static BOOL				sShowHUDAttachments;
	static BOOL				sUseOcclusion;
	static BOOL				sSkipUpdate; //skip lod updates
	static BOOL				sDynamicReflections;

	//cube map for anti-aliasing reflections
	LLCubeMap*				mCubeBuffer;
	GLuint					mCubeList;

	class LLScatterShader
	{
	public:
		static void init(GLhandleARB shader, int map_stage);
	};
	
	//utility shader objects (not shader programs)
	GLhandleARB				mLightVertex;
	GLhandleARB				mLightFragment;
	GLhandleARB				mScatterVertex;
	GLhandleARB				mScatterFragment;
	
	//global (reserved slot) shader parameters
	static const char* sReservedAttribs[];
	static U32 sReservedAttribCount;

	typedef enum 
	{
		GLSL_MATERIAL_COLOR = 0,
		GLSL_SPECULAR_COLOR,
		GLSL_BINORMAL,
		GLSL_END_RESERVED_ATTRIBS
	} eGLSLReservedAttribs;

	static const char* sReservedUniforms[];
	static U32 sReservedUniformCount;
	
	typedef enum
	{
		GLSL_DIFFUSE_MAP = 0,
		GLSL_SPECULAR_MAP,
		GLSL_BUMP_MAP,
		GLSL_ENVIRONMENT_MAP,
		GLSL_END_RESERVED_UNIFORMS
	} eGLSLReservedUniforms;

	static const char* sShinyUniforms[];
	static U32 sShinyUniformCount;

	typedef enum
	{
		GLSL_SHINY_ORIGIN = GLSL_END_RESERVED_UNIFORMS
	} eShinyUniforms;

	LLVector4				mShinyOrigin;

	//object shaders
	LLGLSLShader			mObjectSimpleProgram;
	LLGLSLShader			mObjectAlphaProgram;
	LLGLSLShader			mObjectBumpProgram;
	LLGLSLShader			mObjectShinyProgram;

	//water parameters
	static const char* sWaterUniforms[];
	static U32 sWaterUniformCount;

	typedef enum
	{
		GLSL_WATER_SCREENTEX = GLSL_END_RESERVED_UNIFORMS,
		GLSL_WATER_EYEVEC,
		GLSL_WATER_TIME,
		GLSL_WATER_WAVE_DIR1,
		GLSL_WATER_WAVE_DIR2,
		GLSL_WATER_LIGHT_DIR,
		GLSL_WATER_SPECULAR,
		GLSL_WATER_SPECULAR_EXP,
		GLSL_WATER_FBSCALE,
		GLSL_WATER_REFSCALE
	} eWaterUniforms;
		

	//terrain parameters
	static const char* sTerrainUniforms[];
	static U32 sTerrainUniformCount;

	typedef enum
	{
		GLSL_TERRAIN_DETAIL0 = GLSL_END_RESERVED_UNIFORMS,
		GLSL_TERRAIN_DETAIL1,
		GLSL_TERRAIN_ALPHARAMP
	} eTerrainUniforms;

	//environment shaders
	LLGLSLShader			mTerrainProgram;
	LLGLSLShader			mGroundProgram;
	LLGLSLShader			mWaterProgram;

	//interface shaders
	LLGLSLShader			mHighlightProgram;
	
	//avatar shader parameter tables
	static const char* sAvatarAttribs[];
	static U32 sAvatarAttribCount;

	typedef enum
	{
		GLSL_AVATAR_WEIGHT = GLSL_END_RESERVED_ATTRIBS,
		GLSL_AVATAR_CLOTHING,
		GLSL_AVATAR_WIND,
		GLSL_AVATAR_SINWAVE,
		GLSL_AVATAR_GRAVITY
	} eAvatarAttribs;

	static const char* sAvatarUniforms[];
	static U32 sAvatarUniformCount;

	typedef enum
	{
		GLSL_AVATAR_MATRIX = GLSL_END_RESERVED_UNIFORMS
	} eAvatarUniforms;

	//avatar skinning utility shader object
	GLhandleARB				mAvatarSkinVertex;

	//avatar shader handles
	LLGLSLShader			mAvatarProgram;
	LLGLSLShader			mAvatarEyeballProgram;
	LLGLSLShader			mAvatarPickProgram;
	
	//current avatar shader parameter pointer
	GLint					mAvatarMatrixParam;
	GLint					mMaterialIndex;
	GLint					mSpecularIndex;

	LLColor4				mSunDiffuse;
	LLVector3				mSunDir;

	LLSpatialGroup::sg_vector_t mActiveGroups;
	std::vector<LLDrawInfo*> mRenderMap[LLRenderPass::NUM_RENDER_TYPES];
	std::vector<LLSpatialGroup* > mAlphaGroups;
	std::vector<LLSpatialGroup* > mAlphaGroupsPostWater;
	LLSpatialGroup::sg_vector_t mVisibleGroups;
	LLSpatialGroup::sg_vector_t mDrawableGroups;

	void clearRenderMap();
	
protected:
	BOOL					mVertexShadersEnabled;
	S32						mVertexShadersLoaded; // 0 = no, 1 = yes, -1 = failed
	S32						mVertexShaderLevel[SHADER_COUNT];
	S32						mMaxVertexShaderLevel[SHADER_COUNT];
	
	U32						mRenderTypeMask;
	U32						mRenderFeatureMask;
	U32						mRenderDebugFeatureMask;
	U32						mRenderDebugMask;

	U32						mOldRenderDebugMask;
	
	/////////////////////////////////////////////
	//
	//
	LLDrawable::drawable_vector_t	mVisibleList;
	LLSpatialBridge::bridge_vector_t mVisibleBridge;
	LLSpatialBridge::bridge_vector_t mOccludedBridge;
	LLDrawable::drawable_vector_t	mMovedList;
	LLDrawable::drawable_vector_t mMovedBridge;
	LLDrawable::drawable_vector_t	mShiftList;

	/////////////////////////////////////////////
	//
	//
	struct Light
	{
		Light(LLDrawable* ptr, F32 d, F32 f = 0.0f)
			: drawable(ptr),
			  dist(d),
			  fade(f)
		{}
		LLPointer<LLDrawable> drawable;
		F32 dist;
		F32 fade;
		struct compare
		{
			bool operator()(const Light& a, const Light& b) const
			{
				if ( a.dist < b.dist )
					return true;
				else if ( a.dist > b.dist )
					return false;
				else
					return a.drawable < b.drawable;
			}
		};
	};
	typedef std::set< Light, Light::compare > light_set_t;
	
	LLDrawable::drawable_set_t		mLights;
	light_set_t						mNearbyLights; // lights near camera
	LLColor4						mHWLightColors[8];
	
	/////////////////////////////////////////////
	//
	// Different queues of drawables being processed.
	//
	LLDrawable::drawable_list_t 	mBuildQ1; // priority
	LLDrawable::drawable_list_t 	mBuildQ2; // non-priority
	LLSpatialGroup::sg_set_t		mGroupQ; //spatial groups
	
	LLDrawable::drawable_set_t		mActiveQ;
	
	LLDrawable::drawable_set_t		mRetexturedList;

	//////////////////////////////////////////////////
	//
	// Draw pools are responsible for storing all rendered data,
	// and performing the actual rendering of objects.
	//
	struct compare_pools
	{
		bool operator()(const LLDrawPool* a, const LLDrawPool* b) const
		{
			if (!a)
				return true;
			else if (!b)
				return false;
			else
			{
				S32 atype = a->getType();
				S32 btype = b->getType();
				if (atype < btype)
					return true;
				else if (atype > btype)
					return false;
				else
					return a->getId() < b->getId();
			}
		}
	};
 	typedef std::set<LLDrawPool*, compare_pools > pool_set_t;
	pool_set_t mPools;
	LLDrawPool*	mLastRebuildPool;
	
	// For quick-lookups into mPools (mapped by texture pointer)
	std::map<uintptr_t, LLDrawPool*>	mTerrainPools;
	std::map<uintptr_t, LLDrawPool*>	mTreePools;
	LLDrawPool*					mAlphaPool;
	LLDrawPool*					mAlphaPoolPostWater;
	LLDrawPool*					mSkyPool;
	LLDrawPool*					mStarsPool;
	LLDrawPool*					mTerrainPool;
	LLDrawPool*					mWaterPool;
	LLDrawPool*					mGroundPool;
	LLRenderPass*				mSimplePool;
	LLDrawPool*					mBumpPool;
	// Note: no need to keep an quick-lookup to avatar pools, since there's only one per avatar
	

	std::vector<LLFace*>		mHighlightFaces;	// highlight faces on physical objects
	std::vector<LLFace*>		mSelectedFaces;

	LLPointer<LLViewerImage>	mFaceSelectImagep;
	LLPointer<LLViewerImage>	mBloomImagep;
	LLPointer<LLViewerImage>	mBloomImage2p;
	
	U32						mLightMask;
	U32						mLightMovingMask;
	S32						mLightingDetail;
	F32						mSunShadowFactor;
	
	static BOOL				sRenderPhysicalBeacons;
	static BOOL				sRenderScriptedBeacons;
	static BOOL				sRenderParticleBeacons;
	static BOOL				sRenderSoundBeacons;
};

void render_bbox(const LLVector3 &min, const LLVector3 &max);

extern LLPipeline gPipeline;
extern BOOL gRenderForSelect;
extern F32 gPickAlphaThreshold;
extern BOOL gUsePickAlpha;

#endif
