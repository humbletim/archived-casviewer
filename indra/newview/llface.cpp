/** 
 * @file llface.cpp
 * @brief LLFace class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

#include "lldrawable.h" // lldrawable needs to be included before llface
#include "llface.h"
#include "llviewertextureanim.h"

#include "llviewercontrol.h"
#include "llvolume.h"
#include "m3math.h"
#include "llmatrix4a.h"
#include "v3color.h"

#include "lldrawpoolavatar.h"
#include "lldrawpoolbump.h"
#include "llgl.h"
#include "llrender.h"
#include "lllightconstants.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"
#include "llvosky.h"
#include "llvovolume.h"
#include "pipeline.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"

#define LL_MAX_INDICES_COUNT 1000000

BOOL LLFace::sSafeRenderSelect = TRUE; // FALSE

#define DOTVEC(a,b) (a.mV[0]*b.mV[0] + a.mV[1]*b.mV[1] + a.mV[2]*b.mV[2])


/*
For each vertex, given:
	B - binormal
	T - tangent
	N - normal
	P - position

The resulting texture coordinate <u,v> is:

	u = 2(B dot P)
	v = 2(T dot P)
*/
void planarProjection(LLVector2 &tc, const LLVector4a& normal,
					  const LLVector4a &center, const LLVector4a& vec)
{	
	LLVector4a binormal;
	F32 d = normal[0];

	if (d >= 0.5f || d <= -0.5f)
	{
		if (d < 0)
		{
			binormal.set(0,-1,0);
		}
		else
		{
			binormal.set(0, 1, 0);
		}
	}
	else
	{
        if (normal[1] > 0)
		{
			binormal.set(-1,0,0);
		}
		else
		{
			binormal.set(1,0,0);
		}
	}
	LLVector4a tangent;
	tangent.setCross3(binormal,normal);

	tc.mV[1] = -((tangent.dot3(vec).getF32())*2 - 0.5f);
	tc.mV[0] = 1.0f+((binormal.dot3(vec).getF32())*2 - 0.5f);
}

void sphericalProjection(LLVector2 &tc, const LLVector4a& normal,
						 const LLVector4a &mCenter, const LLVector4a& vec)
{	//BROKEN
	/*tc.mV[0] = acosf(vd.mNormal * LLVector3(1,0,0))/3.14159f;
	
	tc.mV[1] = acosf(vd.mNormal * LLVector3(0,0,1))/6.284f;
	if (vd.mNormal.mV[1] > 0)
	{
		tc.mV[1] = 1.0f-tc.mV[1];
	}*/
}

void cylindricalProjection(LLVector2 &tc, const LLVector4a& normal, const LLVector4a &mCenter, const LLVector4a& vec)
{	//BROKEN
	/*LLVector3 binormal;
	float d = vd.mNormal * LLVector3(1,0,0);
	if (d >= 0.5f || d <= -0.5f)
	{
		binormal = LLVector3(0,1,0);
	}
	else{
		binormal = LLVector3(1,0,0);
	}
	LLVector3 tangent = binormal % vd.mNormal;

	tc.mV[1] = -((tangent*vec)*2 - 0.5f);

	tc.mV[0] = acosf(vd.mNormal * LLVector3(1,0,0))/6.284f;

	if (vd.mNormal.mV[1] < 0)
	{
		tc.mV[0] = 1.0f-tc.mV[0];
	}*/
}

////////////////////
//
// LLFace implementation
//

void LLFace::init(LLDrawable* drawablep, LLViewerObject* objp)
{
	mLastUpdateTime = gFrameTimeSeconds;
	mLastMoveTime = 0.f;
	mLastSkinTime = gFrameTimeSeconds;
	mVSize = 0.f;
	mPixelArea = 16.f;
	mState      = GLOBAL;
	mDrawPoolp  = NULL;
	mPoolType = 0;
	mCenterLocal = objp->getPosition();
	mCenterAgent = drawablep->getPositionAgent();
	mDistance	= 0.f;

	mGeomCount		= 0;
	mGeomIndex		= 0;
	mIndicesCount	= 0;
	mIndicesIndex	= 0;
	mIndexInTex = 0;
	mTexture		= NULL;
	mTEOffset		= -1;
	mTextureIndex = 255;

	setDrawable(drawablep);
	mVObjp = objp;

	mReferenceIndex = -1;

	mTextureMatrix = NULL;
	mDrawInfo = NULL;

	mFaceColor = LLColor4(1,0,0,1);

	mLastVertexBuffer = mVertexBuffer;
	mLastGeomCount = mGeomCount;
	mLastGeomIndex = mGeomIndex;
	mLastIndicesCount = mIndicesCount;
	mLastIndicesIndex = mIndicesIndex;

	mImportanceToCamera = 0.f ;
	mBoundingSphereRadius = 0.0f ;

	mAtlasInfop = NULL ;
	mUsingAtlas  = FALSE ;
	mHasMedia = FALSE ;
}

void LLFace::destroy()
{
	if (gDebugGL)
	{
		gPipeline.checkReferences(this);
	}

	if(mTexture.notNull())
	{
		mTexture->removeFace(this) ;
	}
	
	if (mDrawPoolp)
	{
		if (this->isState(LLFace::RIGGED) && mDrawPoolp->getType() == LLDrawPool::POOL_AVATAR)
		{
			((LLDrawPoolAvatar*) mDrawPoolp)->removeRiggedFace(this);
		}
		else
		{
			mDrawPoolp->removeFace(this);
		}
	
		mDrawPoolp = NULL;
	}

	if (mTextureMatrix)
	{
		delete mTextureMatrix;
		mTextureMatrix = NULL;

		if (mDrawablep.notNull())
		{
			LLSpatialGroup* group = mDrawablep->getSpatialGroup();
			if (group)
			{
				group->dirtyGeom();
				gPipeline.markRebuild(group, TRUE);
			}
		}
	}
	
	setDrawInfo(NULL);
	removeAtlas();
		
	mDrawablep = NULL;
	mVObjp = NULL;
}


// static
void LLFace::initClass()
{
}

void LLFace::setWorldMatrix(const LLMatrix4 &mat)
{
	llerrs << "Faces on this drawable are not independently modifiable\n" << llendl;
}

void LLFace::setPool(LLFacePool* pool)
{
	mDrawPoolp = pool;
}

void LLFace::setPool(LLFacePool* new_pool, LLViewerTexture *texturep)
{
	LLMemType mt1(LLMemType::MTYPE_DRAWABLE);
	
	if (!new_pool)
	{
		llerrs << "Setting pool to null!" << llendl;
	}

	if (new_pool != mDrawPoolp)
	{
		// Remove from old pool
		if (mDrawPoolp)
		{
			mDrawPoolp->removeFace(this);

			if (mDrawablep)
			{
				gPipeline.markRebuild(mDrawablep, LLDrawable::REBUILD_ALL, TRUE);
			}
		}
		mGeomIndex = 0;

		// Add to new pool
		if (new_pool)
		{
			new_pool->addFace(this);
		}
		mDrawPoolp = new_pool;
	}
	
	setTexture(texturep) ;
}

void LLFace::setTexture(LLViewerTexture* tex) 
{
	if(mTexture == tex)
	{
		return ;
	}

	if(mTexture.notNull())
	{
		mTexture->removeFace(this) ;
		removeAtlas() ;
	}	
	
	if(tex)
	{
		tex->addFace(this) ;
	}

	mTexture = tex ;
}

void LLFace::dirtyTexture()
{
	gPipeline.markTextured(getDrawable());
}

void LLFace::switchTexture(LLViewerTexture* new_texture)
{
	if(mTexture == new_texture)
	{
		return ;
	}

	if(!new_texture)
	{
		llerrs << "Can not switch to a null texture." << llendl;
		return;
	}
	new_texture->addTextureStats(mTexture->getMaxVirtualSize()) ;

	getViewerObject()->changeTEImage(mTEOffset, new_texture) ;
	setTexture(new_texture) ;	
	dirtyTexture();
}

void LLFace::setTEOffset(const S32 te_offset)
{
	mTEOffset = te_offset;
}


void LLFace::setFaceColor(const LLColor4& color)
{
	mFaceColor = color;
	setState(USE_FACE_COLOR);
}

void LLFace::unsetFaceColor()
{
	clearState(USE_FACE_COLOR);
}

void LLFace::setDrawable(LLDrawable *drawable)
{
	mDrawablep  = drawable;
	mXform      = &drawable->mXform;
}

void LLFace::setSize(S32 num_vertices, S32 num_indices, bool align)
{
	if (align)
	{
		//allocate vertices in blocks of 4 for alignment
		num_vertices = (num_vertices + 0x3) & ~0x3;
	}
	
	if (mGeomCount != num_vertices ||
		mIndicesCount != num_indices)
	{
		mGeomCount    = num_vertices;
		mIndicesCount = num_indices;
		mVertexBuffer = NULL;
		mLastVertexBuffer = NULL;
	}

	llassert(verify());
}

void LLFace::setGeomIndex(U16 idx) 
{ 
	if (mGeomIndex != idx)
	{
		mGeomIndex = idx; 
		mVertexBuffer = NULL;
	}
}

void LLFace::setTextureIndex(U8 index)
{
	if (index != mTextureIndex)
	{
		mTextureIndex = index;

		if (mTextureIndex != 255)
		{
			mDrawablep->setState(LLDrawable::REBUILD_POSITION);
		}
		else
		{
			if (mDrawInfo && !mDrawInfo->mTextureList.empty())
			{
				llerrs << "Face with no texture index references indexed texture draw info." << llendl;
			}
		}
	}
}

void LLFace::setIndicesIndex(S32 idx) 
{ 
	if (mIndicesIndex != idx)
	{
		mIndicesIndex = idx; 
		mVertexBuffer = NULL;
	}
}
	
//============================================================================

U16 LLFace::getGeometryAvatar(
						LLStrider<LLVector3> &vertices,
						LLStrider<LLVector3> &normals,
						LLStrider<LLVector2> &tex_coords,
						LLStrider<F32>		 &vertex_weights,
						LLStrider<LLVector4> &clothing_weights)
{
	LLMemType mt1(LLMemType::MTYPE_DRAWABLE);

	if (mVertexBuffer.notNull())
	{
		mVertexBuffer->getVertexStrider      (vertices, mGeomIndex);
		mVertexBuffer->getNormalStrider      (normals, mGeomIndex);
		mVertexBuffer->getTexCoord0Strider    (tex_coords, mGeomIndex);
		mVertexBuffer->getWeightStrider(vertex_weights, mGeomIndex);
		mVertexBuffer->getClothWeightStrider(clothing_weights, mGeomIndex);
	}

	return mGeomIndex;
}

U16 LLFace::getGeometry(LLStrider<LLVector3> &vertices, LLStrider<LLVector3> &normals,
					    LLStrider<LLVector2> &tex_coords, LLStrider<U16> &indicesp)
{
	LLMemType mt1(LLMemType::MTYPE_DRAWABLE);
	
	if (mVertexBuffer.notNull())
	{
		mVertexBuffer->getVertexStrider(vertices,   mGeomIndex);
		if (mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_NORMAL))
		{
			mVertexBuffer->getNormalStrider(normals,    mGeomIndex);
		}
		if (mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0))
		{
			mVertexBuffer->getTexCoord0Strider(tex_coords, mGeomIndex);
		}

		mVertexBuffer->getIndexStrider(indicesp, mIndicesIndex);
	}
	
	return mGeomIndex;
}

void LLFace::updateCenterAgent()
{
	if (mDrawablep->isActive())
	{
		mCenterAgent = mCenterLocal * getRenderMatrix();
	}
	else
	{
		mCenterAgent = mCenterLocal;
	}
}

void LLFace::renderSelected(LLViewerTexture *imagep, const LLColor4& color)
{
	if (mDrawablep->getSpatialGroup() == NULL)
	{
		return;
	}

	mDrawablep->getSpatialGroup()->rebuildGeom();
	mDrawablep->getSpatialGroup()->rebuildMesh();
		
	if(mDrawablep.isNull() || mVertexBuffer.isNull())
	{
		return;
	}

	if (mGeomCount > 0 && mIndicesCount > 0)
	{
		gGL.getTexUnit(0)->bind(imagep);
	
		gGL.pushMatrix();
		if (mDrawablep->isActive())
		{
			glMultMatrixf((GLfloat*)mDrawablep->getRenderMatrix().mMatrix);
		}
		else
		{
			glMultMatrixf((GLfloat*)mDrawablep->getRegion()->mRenderMatrix.mMatrix);
		}

		glColor4fv(color.mV);
	
		if (mDrawablep->isState(LLDrawable::RIGGED))
		{
			LLVOVolume* volume = mDrawablep->getVOVolume();
			if (volume)
			{
				LLRiggedVolume* rigged = volume->getRiggedVolume();
				if (rigged)
				{
					LLGLEnable offset(GL_POLYGON_OFFSET_FILL);
					glPolygonOffset(-1.f, -1.f);
					glMultMatrixf((F32*) volume->getRelativeXform().mMatrix);
					const LLVolumeFace& vol_face = rigged->getVolumeFace(getTEOffset());
					LLVertexBuffer::unbind();
					glVertexPointer(3, GL_FLOAT, 16, vol_face.mPositions);
					if (vol_face.mTexCoords)
					{
						glEnableClientState(GL_TEXTURE_COORD_ARRAY);
						glTexCoordPointer(2, GL_FLOAT, 8, vol_face.mTexCoords);
					}
					glDrawElements(GL_TRIANGLES, vol_face.mNumIndices, GL_UNSIGNED_SHORT, vol_face.mIndices);
					glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				}
			}
		}
		else
		{
			mVertexBuffer->setBuffer(LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0);
			mVertexBuffer->draw(LLRender::TRIANGLES, mIndicesCount, mIndicesIndex);
		}

		gGL.popMatrix();
	}
}


/* removed in lieu of raycast uv detection
void LLFace::renderSelectedUV()
{
	LLViewerTexture* red_blue_imagep = LLViewerTextureManager::getFetchedTextureFromFile("uv_test1.j2c", TRUE, LLViewerTexture::BOOST_UI);
	LLViewerTexture* green_imagep = LLViewerTextureManager::getFetchedTextureFromFile("uv_test2.tga", TRUE, LLViewerTexture::BOOST_UI);

	LLGLSUVSelect object_select;

	// use red/blue gradient to get coarse UV coordinates
	renderSelected(red_blue_imagep, LLColor4::white);
	
	static F32 bias = 0.f;
	static F32 factor = -10.f;
	glPolygonOffset(factor, bias);

	// add green dither pattern on top of red/blue gradient
	gGL.blendFunc(LLRender::BF_ONE, LLRender::BF_ONE);
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	// make green pattern repeat once per texel in red/blue texture
	glScalef(256.f, 256.f, 1.f);
	glMatrixMode(GL_MODELVIEW);

	renderSelected(green_imagep, LLColor4::white);

	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	gGL.blendFunc(LLRender::BF_SOURCE_ALPHA, LLRender::BF_ONE_MINUS_SOURCE_ALPHA);
}
*/

void LLFace::setDrawInfo(LLDrawInfo* draw_info)
{
	if (draw_info)
	{
		if (draw_info->mFace)
		{
			draw_info->mFace->setDrawInfo(NULL);
		}
		draw_info->mFace = this;
	}
	
	if (mDrawInfo)
	{
		mDrawInfo->mFace = NULL;
	}

	mDrawInfo = draw_info;
}

void LLFace::printDebugInfo() const
{
	LLFacePool *poolp = getPool();
	llinfos << "Object: " << getViewerObject()->mID << llendl;
	if (getDrawable())
	{
		llinfos << "Type: " << LLPrimitive::pCodeToString(getDrawable()->getVObj()->getPCode()) << llendl;
	}
	if (getTexture())
	{
		llinfos << "Texture: " << getTexture() << " Comps: " << (U32)getTexture()->getComponents() << llendl;
	}
	else
	{
		llinfos << "No texture: " << llendl;
	}

	llinfos << "Face: " << this << llendl;
	llinfos << "State: " << getState() << llendl;
	llinfos << "Geom Index Data:" << llendl;
	llinfos << "--------------------" << llendl;
	llinfos << "GI: " << mGeomIndex << " Count:" << mGeomCount << llendl;
	llinfos << "Face Index Data:" << llendl;
	llinfos << "--------------------" << llendl;
	llinfos << "II: " << mIndicesIndex << " Count:" << mIndicesCount << llendl;
	llinfos << llendl;

	if (poolp)
	{
		poolp->printDebugInfo();

		S32 pool_references = 0;
		for (std::vector<LLFace*>::iterator iter = poolp->mReferences.begin();
			 iter != poolp->mReferences.end(); iter++)
		{
			LLFace *facep = *iter;
			if (facep == this)
			{
				llinfos << "Pool reference: " << pool_references << llendl;
				pool_references++;
			}
		}

		if (pool_references != 1)
		{
			llinfos << "Incorrect number of pool references!" << llendl;
		}
	}

#if 0
	llinfos << "Indices:" << llendl;
	llinfos << "--------------------" << llendl;

	const U32 *indicesp = getRawIndices();
	S32 indices_count = getIndicesCount();
	S32 geom_start = getGeomStart();

	for (S32 i = 0; i < indices_count; i++)
	{
		llinfos << i << ":" << indicesp[i] << ":" << (S32)(indicesp[i] - geom_start) << llendl;
	}
	llinfos << llendl;

	llinfos << "Vertices:" << llendl;
	llinfos << "--------------------" << llendl;
	for (S32 i = 0; i < mGeomCount; i++)
	{
		llinfos << mGeomIndex + i << ":" << poolp->getVertex(mGeomIndex + i) << llendl;
	}
	llinfos << llendl;
#endif
}

// Transform the texture coordinates for this face.
static void xform(LLVector2 &tex_coord, F32 cosAng, F32 sinAng, F32 offS, F32 offT, F32 magS, F32 magT)
{
	// New, good way
	F32 s = tex_coord.mV[0];
	F32 t = tex_coord.mV[1];

	// Texture transforms are done about the center of the face.
	s -= 0.5; 
	t -= 0.5;

	// Handle rotation
	F32 temp = s;
	s  = s     * cosAng + t * sinAng;
	t  = -temp * sinAng + t * cosAng;

	// Then scale
	s *= magS;
	t *= magT;

	// Then offset
	s += offS + 0.5f; 
	t += offT + 0.5f;

	tex_coord.mV[0] = s;
	tex_coord.mV[1] = t;
}


BOOL LLFace::genVolumeBBoxes(const LLVolume &volume, S32 f,
								const LLMatrix4& mat_vert_in, const LLMatrix3& mat_normal_in, BOOL global_volume)
{
	LLMemType mt1(LLMemType::MTYPE_DRAWABLE);

	//get bounding box
	if (mDrawablep->isState(LLDrawable::REBUILD_VOLUME | LLDrawable::REBUILD_POSITION | LLDrawable::REBUILD_RIGGED))
	{
		//VECTORIZE THIS
		LLMatrix4a mat_vert;
		mat_vert.loadu(mat_vert_in);

		LLMatrix4a mat_normal;
		mat_normal.loadu(mat_normal_in);

		//if (mDrawablep->isState(LLDrawable::REBUILD_VOLUME))
		//{ //vertex buffer no longer valid
		//	mVertexBuffer = NULL;
		//	mLastVertexBuffer = NULL;
		//}

		//VECTORIZE THIS
		LLVector4a min,max;
	
		if (f >= volume.getNumVolumeFaces())
		{
			llwarns << "Generating bounding box for invalid face index!" << llendl;
			f = 0;
		}

		const LLVolumeFace &face = volume.getVolumeFace(f);
		min = face.mExtents[0];
		max = face.mExtents[1];
		

		//min, max are in volume space, convert to drawable render space
		LLVector4a center;
		LLVector4a t;
		t.setAdd(min, max);
		t.mul(0.5f);
		mat_vert.affineTransform(t, center);
		LLVector4a size;
		size.setSub(max, min);
		size.mul(0.5f);

		if (!global_volume)
		{
			//VECTORIZE THIS
			LLVector4a scale;
			scale.load3(mDrawablep->getVObj()->getScale().mV);
			size.mul(scale);
		}

		mat_normal.mMatrix[0].normalize3fast();
		mat_normal.mMatrix[1].normalize3fast();
		mat_normal.mMatrix[2].normalize3fast();
		
		LLVector4a v[4];

		//get 4 corners of bounding box
		mat_normal.rotate(size,v[0]);

		//VECTORIZE THIS
		LLVector4a scale;
		
		scale.set(-1.f, -1.f, 1.f);
		scale.mul(size);
		mat_normal.rotate(scale, v[1]);
		
		scale.set(1.f, -1.f, -1.f);
		scale.mul(size);
		mat_normal.rotate(scale, v[2]);
		
		scale.set(-1.f, 1.f, -1.f);
		scale.mul(size);
		mat_normal.rotate(scale, v[3]);

		LLVector4a& newMin = mExtents[0];
		LLVector4a& newMax = mExtents[1];
		
		newMin = newMax = center;
		
		for (U32 i = 0; i < 4; i++)
		{
			LLVector4a delta;
			delta.setAbs(v[i]);
			LLVector4a min;
			min.setSub(center, delta);
			LLVector4a max;
			max.setAdd(center, delta);

			newMin.setMin(newMin,min);
			newMax.setMax(newMax,max);
		}

		if (!mDrawablep->isActive())
		{
			LLVector4a offset;
			offset.load3(mDrawablep->getRegion()->getOriginAgent().mV);
			newMin.add(offset);
			newMax.add(offset);
		}

		t.setAdd(newMin, newMax);
		t.mul(0.5f);

		//VECTORIZE THIS
		mCenterLocal.set(t.getF32ptr());
		
		t.setSub(newMax,newMin);
		mBoundingSphereRadius = t.getLength3().getF32()*0.5f;

		updateCenterAgent();
	}

	return TRUE;
}



// convert surface coordinates to texture coordinates, based on
// the values in the texture entry.  probably should be
// integrated with getGeometryVolume() for its texture coordinate
// generation - but i'll leave that to someone more familiar
// with the implications.
LLVector2 LLFace::surfaceToTexture(LLVector2 surface_coord, LLVector3 position, LLVector3 normal)
{
	LLVector2 tc = surface_coord;
	
	const LLTextureEntry *tep = getTextureEntry();

	if (tep == NULL)
	{
		// can't do much without the texture entry
		return surface_coord;
	}

	//VECTORIZE THIS
	// see if we have a non-default mapping
    U8 texgen = getTextureEntry()->getTexGen();
	if (texgen != LLTextureEntry::TEX_GEN_DEFAULT)
	{
		LLVector4a& center = *(mDrawablep->getVOVolume()->getVolume()->getVolumeFace(mTEOffset).mCenter);
		
		LLVector4a volume_position;
		volume_position.load3(mDrawablep->getVOVolume()->agentPositionToVolume(position).mV);
		
		if (!mDrawablep->getVOVolume()->isVolumeGlobal())
		{
			LLVector4a scale;
			scale.load3(mVObjp->getScale().mV);
			volume_position.mul(scale);
		}
		
		LLVector4a volume_normal;
		volume_normal.load3(mDrawablep->getVOVolume()->agentDirectionToVolume(normal).mV);
		volume_normal.normalize3fast();
		
		switch (texgen)
		{
		case LLTextureEntry::TEX_GEN_PLANAR:
			planarProjection(tc, volume_normal, center, volume_position);
			break;
		case LLTextureEntry::TEX_GEN_SPHERICAL:
			sphericalProjection(tc, volume_normal, center, volume_position);
			break;
		case LLTextureEntry::TEX_GEN_CYLINDRICAL:
			cylindricalProjection(tc, volume_normal, center, volume_position);
			break;
		default:
			break;
		}		
	}

	if (mTextureMatrix)	// if we have a texture matrix, use it
	{
		LLVector3 tc3(tc);
		tc3 = tc3 * *mTextureMatrix;
		tc = LLVector2(tc3);
	}
	
	else // otherwise use the texture entry parameters
	{
		xform(tc, cos(tep->getRotation()), sin(tep->getRotation()),
			  tep->mOffsetS, tep->mOffsetT, tep->mScaleS, tep->mScaleT);
	}

	
	return tc;
}

// Returns scale compared to default texgen, and face orientation as calculated
// by planarProjection(). This is needed to match planar texgen parameters.
void LLFace::getPlanarProjectedParams(LLQuaternion* face_rot, LLVector3* face_pos, F32* scale) const
{
	const LLMatrix4& vol_mat = getWorldMatrix();
	const LLVolumeFace& vf = getViewerObject()->getVolume()->getVolumeFace(mTEOffset);
	const LLVector4a& normal4a = vf.mNormals[0];
	const LLVector4a& binormal4a = vf.mBinormals[0];
	LLVector2 projected_binormal;
	planarProjection(projected_binormal, normal4a, *vf.mCenter, binormal4a);
	projected_binormal -= LLVector2(0.5f, 0.5f); // this normally happens in xform()
	*scale = projected_binormal.length();
	// rotate binormal to match what planarProjection() thinks it is,
	// then find rotation from that:
	projected_binormal.normalize();
	F32 ang = acos(projected_binormal.mV[VY]);
	ang = (projected_binormal.mV[VX] < 0.f) ? -ang : ang;

	//VECTORIZE THIS
	LLVector3 binormal(binormal4a.getF32ptr());
	LLVector3 normal(normal4a.getF32ptr());
	binormal.rotVec(ang, normal);
	LLQuaternion local_rot( binormal % normal, binormal, normal );
	*face_rot = local_rot * vol_mat.quaternion();
	*face_pos = vol_mat.getTranslation();
}

// Returns the necessary texture transform to align this face's TE to align_to's TE
bool LLFace::calcAlignedPlanarTE(const LLFace* align_to,  LLVector2* res_st_offset, 
								 LLVector2* res_st_scale, F32* res_st_rot) const
{
	if (!align_to)
	{
		return false;
	}
	const LLTextureEntry *orig_tep = align_to->getTextureEntry();
	if ((orig_tep->getTexGen() != LLTextureEntry::TEX_GEN_PLANAR) ||
		(getTextureEntry()->getTexGen() != LLTextureEntry::TEX_GEN_PLANAR))
	{
		return false;
	}

	LLVector3 orig_pos, this_pos;
	LLQuaternion orig_face_rot, this_face_rot;
	F32 orig_proj_scale, this_proj_scale;
	align_to->getPlanarProjectedParams(&orig_face_rot, &orig_pos, &orig_proj_scale);
	getPlanarProjectedParams(&this_face_rot, &this_pos, &this_proj_scale);

	// The rotation of "this face's" texture:
	LLQuaternion orig_st_rot = LLQuaternion(orig_tep->getRotation(), LLVector3::z_axis) * orig_face_rot;
	LLQuaternion this_st_rot = orig_st_rot * ~this_face_rot;
	F32 x_ang, y_ang, z_ang;
	this_st_rot.getEulerAngles(&x_ang, &y_ang, &z_ang);
	*res_st_rot = z_ang;

	// Offset and scale of "this face's" texture:
	LLVector3 centers_dist = (this_pos - orig_pos) * ~orig_st_rot;
	LLVector3 st_scale(orig_tep->mScaleS, orig_tep->mScaleT, 1.f);
	st_scale *= orig_proj_scale;
	centers_dist.scaleVec(st_scale);
	LLVector2 orig_st_offset(orig_tep->mOffsetS, orig_tep->mOffsetT);

	*res_st_offset = orig_st_offset + (LLVector2)centers_dist;
	res_st_offset->mV[VX] -= (S32)res_st_offset->mV[VX];
	res_st_offset->mV[VY] -= (S32)res_st_offset->mV[VY];

	st_scale /= this_proj_scale;
	*res_st_scale = (LLVector2)st_scale;
	return true;
}

void LLFace::updateRebuildFlags()
{
	if (!mDrawablep->isState(LLDrawable::REBUILD_VOLUME))
	{
		BOOL moved = TRUE;
		if (mLastVertexBuffer == mVertexBuffer && 
			!mVertexBuffer->isEmpty())
		{	//this face really doesn't need to be regenerated, try real hard not to do so
			if (mLastGeomCount == mGeomCount &&
				mLastGeomIndex == mGeomIndex &&
				mLastIndicesCount == mIndicesCount &&
				mLastIndicesIndex == mIndicesIndex)
			{ //data is in same location in vertex buffer
				moved = FALSE;
			}
		}
		mLastMoveTime = gFrameTimeSeconds;
		
		if (moved)
		{
			mDrawablep->setState(LLDrawable::REBUILD_VOLUME);
		}
	}
	else
	{
		mLastUpdateTime = gFrameTimeSeconds;
	}
}


bool LLFace::canRenderAsMask()
{
	if (LLPipeline::sNoAlpha)
	{
		return true;
	}

	const LLTextureEntry* te = getTextureEntry();

	if ((te->getColor().mV[3] == 1.0f) && // can't treat as mask if we have face alpha
		(te->getGlow() == 0.f) && // glowing masks are hard to implement - don't mask
		getTexture()->getIsAlphaMask()) // texture actually qualifies for masking (lazily recalculated but expensive)
	{
		if (LLPipeline::sRenderDeferred)
		{
			if (getViewerObject()->isHUDAttachment() || te->getFullbright())
			{ //hud attachments and fullbright objects are NOT subject to the deferred rendering pipe
				return LLPipeline::sAutoMaskAlphaNonDeferred;
			}
			else
			{
				return LLPipeline::sAutoMaskAlphaDeferred;
			}
		}
		else
		{
			return LLPipeline::sAutoMaskAlphaNonDeferred;
		}
	}

	return false;
}


static LLFastTimer::DeclareTimer FTM_FACE_GET_GEOM("Face Geom");

BOOL LLFace::getGeometryVolume(const LLVolume& volume,
							   const S32 &f,
								const LLMatrix4& mat_vert_in, const LLMatrix3& mat_norm_in,
								const U16 &index_offset,
								bool force_rebuild)
{
	LLFastTimer t(FTM_FACE_GET_GEOM);
	llassert(verify());
	const LLVolumeFace &vf = volume.getVolumeFace(f);
	S32 num_vertices = (S32)vf.mNumVertices;
	S32 num_indices = (S32) vf.mNumIndices;
	
	if (mVertexBuffer.notNull())
	{
		if (num_indices + (S32) mIndicesIndex > mVertexBuffer->getNumIndices())
		{
			llwarns	<< "Index buffer overflow!" << llendl;
			llwarns << "Indices Count: " << mIndicesCount
					<< " VF Num Indices: " << num_indices
					<< " Indices Index: " << mIndicesIndex
					<< " VB Num Indices: " << mVertexBuffer->getNumIndices() << llendl;
			llwarns	<< "Last Indices Count: " << mLastIndicesCount
					<< " Last Indices Index: " << mLastIndicesIndex
					<< " Face Index: " << f
					<< " Pool Type: " << mPoolType << llendl;
			return FALSE;
		}

		if (num_vertices + mGeomIndex > mVertexBuffer->getNumVerts())
		{
			llwarns << "Vertex buffer overflow!" << llendl;
			return FALSE;
		}
	}

	LLStrider<LLVector3> vert;
	LLVector4a* vertices = NULL;
	LLStrider<LLVector2> tex_coords;
	LLStrider<LLVector2> tex_coords2;
	LLVector4a* normals = NULL;
	LLStrider<LLVector3> norm;
	LLStrider<LLColor4U> colors;
	LLVector4a* binormals = NULL;
	LLStrider<LLVector3> binorm;
	LLStrider<U16> indicesp;
	LLVector4a* weights = NULL;
	LLStrider<LLVector4> wght;

	BOOL full_rebuild = force_rebuild || mDrawablep->isState(LLDrawable::REBUILD_VOLUME);
	
	BOOL global_volume = mDrawablep->getVOVolume()->isVolumeGlobal();
	LLVector3 scale;
	if (global_volume)
	{
		scale.setVec(1,1,1);
	}
	else
	{
		scale = mVObjp->getScale();
	}
	
	bool rebuild_pos = full_rebuild || mDrawablep->isState(LLDrawable::REBUILD_POSITION);
	bool rebuild_color = full_rebuild || mDrawablep->isState(LLDrawable::REBUILD_COLOR);
	bool rebuild_tcoord = full_rebuild || mDrawablep->isState(LLDrawable::REBUILD_TCOORD);
	bool rebuild_normal = rebuild_pos && mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_NORMAL);
	bool rebuild_binormal = rebuild_pos && mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_BINORMAL);
	bool rebuild_weights = rebuild_pos && mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_WEIGHT4);

	const LLTextureEntry *tep = mVObjp->getTE(f);
	const U8 bump_code = tep ? tep->getBumpmap() : 0;

	if (rebuild_pos)
	{
		mVertexBuffer->getVertexStrider(vert, mGeomIndex);
		vertices = (LLVector4a*) vert.get();
	}
	if (rebuild_normal)
	{
		mVertexBuffer->getNormalStrider(norm, mGeomIndex);
		normals = (LLVector4a*) norm.get();
	}
	if (rebuild_binormal)
	{
		mVertexBuffer->getBinormalStrider(binorm, mGeomIndex);
		binormals = (LLVector4a*) binorm.get();
	}
	if (rebuild_weights)
	{
		mVertexBuffer->getWeight4Strider(wght, mGeomIndex);
		weights = (LLVector4a*) wght.get();
	}
	
	F32 tcoord_xoffset = 0.f ;
	F32 tcoord_yoffset = 0.f ;
	F32 tcoord_xscale = 1.f ;
	F32 tcoord_yscale = 1.f ;
	BOOL in_atlas = FALSE ;

	if (rebuild_tcoord)
	{
		mVertexBuffer->getTexCoord0Strider(tex_coords, mGeomIndex);
		if (bump_code && mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD1))
		{
			mVertexBuffer->getTexCoord1Strider(tex_coords2, mGeomIndex);
		}

		in_atlas = isAtlasInUse() ;
		if(in_atlas)
		{
			const LLVector2* tmp = getTexCoordOffset() ;
			tcoord_xoffset = tmp->mV[0] ; 
			tcoord_yoffset = tmp->mV[1] ;

			tmp = getTexCoordScale() ;
			tcoord_xscale = tmp->mV[0] ; 
			tcoord_yscale = tmp->mV[1] ;	
		}
	}
	if (rebuild_color)
	{	
		mVertexBuffer->getColorStrider(colors, mGeomIndex);
	}

	BOOL is_static = mDrawablep->isStatic();
	BOOL is_global = is_static;

	LLVector3 center_sum(0.f, 0.f, 0.f);
	
	if (is_global)
	{
		setState(GLOBAL);
	}
	else
	{
		clearState(GLOBAL);
	}

	LLColor4U color = tep->getColor();

	if (rebuild_color)
	{
		if (tep)
		{
			GLfloat alpha[4] =
			{
				0.00f,
				0.25f,
				0.5f,
				0.75f
			};
			
			if (getPoolType() != LLDrawPool::POOL_ALPHA && (LLPipeline::sRenderDeferred || (LLPipeline::sRenderBump && tep->getShiny())))
			{
				color.mV[3] = U8 (alpha[tep->getShiny()] * 255);
			}
		}
	}

	// INDICES
	if (full_rebuild)
	{
		mVertexBuffer->getIndexStrider(indicesp, mIndicesIndex);
		__m128i* dst = (__m128i*) indicesp.get();
		__m128i* src = (__m128i*) vf.mIndices;
		__m128i offset = _mm_set1_epi16(index_offset);

		S32 end = num_indices/8;
		
		for (S32 i = 0; i < end; i++)
		{
			__m128i res = _mm_add_epi16(src[i], offset);
			_mm_storeu_si128(dst+i, res);
		}

		for (S32 i = end*8; i < num_indices; ++i)
		{
			indicesp[i] = vf.mIndices[i]+index_offset;
		}
	}
	
	LLMatrix4a mat_normal;
	mat_normal.loadu(mat_norm_in);
	
	//if it's not fullbright and has no normals, bake sunlight based on face normal
	//bool bake_sunlight = !getTextureEntry()->getFullbright() &&
	//  !mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_NORMAL);

	F32 r = 0, os = 0, ot = 0, ms = 0, mt = 0, cos_ang = 0, sin_ang = 0;

	if (rebuild_tcoord)
	{
		bool do_xform;
			
		if (tep)
		{
			r  = tep->getRotation();
			os = tep->mOffsetS;
			ot = tep->mOffsetT;
			ms = tep->mScaleS;
			mt = tep->mScaleT;
			cos_ang = cos(r);
			sin_ang = sin(r);

			if (cos_ang != 1.f || 
				sin_ang != 0.f ||
				os != 0.f ||
				ot != 0.f ||
				ms != 1.f ||
				mt != 1.f)
			{
				do_xform = true;
			}
			else
			{
				do_xform = false;
			}	
		}
		else
		{
			do_xform = false;
		}
						
		//bump setup
		LLVector4a binormal_dir( -sin_ang, cos_ang, 0.f );
		LLVector4a bump_s_primary_light_ray(0.f, 0.f, 0.f);
		LLVector4a bump_t_primary_light_ray(0.f, 0.f, 0.f);

		LLQuaternion bump_quat;
		if (mDrawablep->isActive())
		{
			bump_quat = LLQuaternion(mDrawablep->getRenderMatrix());
		}
		
		if (bump_code)
		{
			mVObjp->getVolume()->genBinormals(f);
			F32 offset_multiple; 
			switch( bump_code )
			{
				case BE_NO_BUMP:
				offset_multiple = 0.f;
				break;
				case BE_BRIGHTNESS:
				case BE_DARKNESS:
				if( mTexture.notNull() && mTexture->hasGLTexture())
				{
					// Offset by approximately one texel
					S32 cur_discard = mTexture->getDiscardLevel();
					S32 max_size = llmax( mTexture->getWidth(), mTexture->getHeight() );
					max_size <<= cur_discard;
					const F32 ARTIFICIAL_OFFSET = 2.f;
					offset_multiple = ARTIFICIAL_OFFSET / (F32)max_size;
				}
				else
				{
					offset_multiple = 1.f/256;
				}
				break;

				default:  // Standard bumpmap textures.  Assumed to be 256x256
				offset_multiple = 1.f / 256;
				break;
			}

			F32 s_scale = 1.f;
			F32 t_scale = 1.f;
			if( tep )
			{
				tep->getScale( &s_scale, &t_scale );
			}
			// Use the nudged south when coming from above sun angle, such
			// that emboss mapping always shows up on the upward faces of cubes when 
			// it's noon (since a lot of builders build with the sun forced to noon).
			LLVector3   sun_ray  = gSky.mVOSkyp->mBumpSunDir;
			LLVector3   moon_ray = gSky.getMoonDirection();
			LLVector3& primary_light_ray = (sun_ray.mV[VZ] > 0) ? sun_ray : moon_ray;

			bump_s_primary_light_ray.load3((offset_multiple * s_scale * primary_light_ray).mV);
			bump_t_primary_light_ray.load3((offset_multiple * t_scale * primary_light_ray).mV);
		}

		U8 texgen = getTextureEntry()->getTexGen();
		if (rebuild_tcoord && texgen != LLTextureEntry::TEX_GEN_DEFAULT)
		{ //planar texgen needs binormals
			mVObjp->getVolume()->genBinormals(f);
		}

		U8 tex_mode = 0;
	
		if (isState(TEXTURE_ANIM))
		{
			LLVOVolume* vobj = (LLVOVolume*) (LLViewerObject*) mVObjp;	
			tex_mode = vobj->mTexAnimMode;

			if (!tex_mode)
			{
				clearState(TEXTURE_ANIM);
			}
			else
			{
				os = ot = 0.f;
				r = 0.f;
				cos_ang = 1.f;
				sin_ang = 0.f;
				ms = mt = 1.f;

				do_xform = false;
			}

			if (getVirtualSize() >= MIN_TEX_ANIM_SIZE)
			{ //don't override texture transform during tc bake
				tex_mode = 0;
			}
		}

		LLVector4a scalea;
		scalea.load3(scale.mV);

		bool do_bump = bump_code && mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD1);
		bool do_tex_mat = tex_mode && mTextureMatrix;

		if (!in_atlas && !do_bump)
		{ //not in atlas or not bump mapped, might be able to do a cheap update
			if (texgen != LLTextureEntry::TEX_GEN_PLANAR)
			{
				if (!do_tex_mat)
				{
					if (!do_xform)
					{
						LLVector4a::memcpyNonAliased16((F32*) tex_coords.get(), (F32*) vf.mTexCoords, num_vertices*2*sizeof(F32));
					}
					else
					{
						for (S32 i = 0; i < num_vertices; i++)
						{	
							LLVector2 tc(vf.mTexCoords[i]);
							xform(tc, cos_ang, sin_ang, os, ot, ms, mt);
							*tex_coords++ = tc;	
						}
					}
				}
				else
				{ //do tex mat, no texgen, no atlas, no bump
					for (S32 i = 0; i < num_vertices; i++)
					{	
						LLVector2 tc(vf.mTexCoords[i]);
						//LLVector4a& norm = vf.mNormals[i];
						//LLVector4a& center = *(vf.mCenter);

						LLVector3 tmp(tc.mV[0], tc.mV[1], 0.f);
						tmp = tmp * *mTextureMatrix;
						tc.mV[0] = tmp.mV[0];
						tc.mV[1] = tmp.mV[1];
						*tex_coords++ = tc;	
					}
				}
			}
			else
			{ //no bump, no atlas, tex gen planar
				if (do_tex_mat)
				{
					for (S32 i = 0; i < num_vertices; i++)
					{	
						LLVector2 tc(vf.mTexCoords[i]);
						LLVector4a& norm = vf.mNormals[i];
						LLVector4a& center = *(vf.mCenter);
						LLVector4a vec = vf.mPositions[i];	
						vec.mul(scalea);
						planarProjection(tc, norm, center, vec);
						
						LLVector3 tmp(tc.mV[0], tc.mV[1], 0.f);
						tmp = tmp * *mTextureMatrix;
						tc.mV[0] = tmp.mV[0];
						tc.mV[1] = tmp.mV[1];
				
						*tex_coords++ = tc;	
					}
				}
				else
				{
					for (S32 i = 0; i < num_vertices; i++)
					{	
						LLVector2 tc(vf.mTexCoords[i]);
						LLVector4a& norm = vf.mNormals[i];
						LLVector4a& center = *(vf.mCenter);
						LLVector4a vec = vf.mPositions[i];	
						vec.mul(scalea);
						planarProjection(tc, norm, center, vec);
						
						xform(tc, cos_ang, sin_ang, os, ot, ms, mt);

						*tex_coords++ = tc;	
					}
				}
			}
		}
		else
		{ //either bump mapped or in atlas, just do the whole expensive loop
			for (S32 i = 0; i < num_vertices; i++)
			{	
				LLVector2 tc(vf.mTexCoords[i]);
			
				LLVector4a& norm = vf.mNormals[i];
				
				LLVector4a& center = *(vf.mCenter);
		   
				if (texgen != LLTextureEntry::TEX_GEN_DEFAULT)
				{
					LLVector4a vec = vf.mPositions[i];
				
					vec.mul(scalea);

					switch (texgen)
					{
						case LLTextureEntry::TEX_GEN_PLANAR:
							planarProjection(tc, norm, center, vec);
							break;
						case LLTextureEntry::TEX_GEN_SPHERICAL:
							sphericalProjection(tc, norm, center, vec);
							break;
						case LLTextureEntry::TEX_GEN_CYLINDRICAL:
							cylindricalProjection(tc, norm, center, vec);
							break;
						default:
							break;
					}		
				}

				if (tex_mode && mTextureMatrix)
				{
					LLVector3 tmp(tc.mV[0], tc.mV[1], 0.f);
					tmp = tmp * *mTextureMatrix;
					tc.mV[0] = tmp.mV[0];
					tc.mV[1] = tmp.mV[1];
				}
				else
				{
					xform(tc, cos_ang, sin_ang, os, ot, ms, mt);
				}

				if(in_atlas)
				{
					//
					//manually calculate tex-coord per vertex for varying address modes.
					//should be removed if shader can handle this.
					//

					S32 int_part = 0 ;
					switch(mTexture->getAddressMode())
					{
					case LLTexUnit::TAM_CLAMP:
						if(tc.mV[0] < 0.f)
						{
							tc.mV[0] = 0.f ;
						}
						else if(tc.mV[0] > 1.f)
						{
							tc.mV[0] = 1.f;
						}

						if(tc.mV[1] < 0.f)
						{
							tc.mV[1] = 0.f ;
						}
						else if(tc.mV[1] > 1.f)
						{
							tc.mV[1] = 1.f;
						}
						break;
					case LLTexUnit::TAM_MIRROR:
						if(tc.mV[0] < 0.f)
						{
							tc.mV[0] = -tc.mV[0] ;
						}
						int_part = (S32)tc.mV[0] ;
						if(int_part & 1) //odd number
						{
							tc.mV[0] = int_part + 1 - tc.mV[0] ;
						}
						else //even number
						{
							tc.mV[0] -= int_part ;
						}

						if(tc.mV[1] < 0.f)
						{
							tc.mV[1] = -tc.mV[1] ;
						}
						int_part = (S32)tc.mV[1] ;
						if(int_part & 1) //odd number
						{
							tc.mV[1] = int_part + 1 - tc.mV[1] ;
						}
						else //even number
						{
							tc.mV[1] -= int_part ;
						}
						break;
					case LLTexUnit::TAM_WRAP:
						if(tc.mV[0] > 1.f)
							tc.mV[0] -= (S32)(tc.mV[0] - 0.00001f) ;
						else if(tc.mV[0] < -1.f)
							tc.mV[0] -= (S32)(tc.mV[0] + 0.00001f) ;

						if(tc.mV[1] > 1.f)
							tc.mV[1] -= (S32)(tc.mV[1] - 0.00001f) ;
						else if(tc.mV[1] < -1.f)
							tc.mV[1] -= (S32)(tc.mV[1] + 0.00001f) ;

						if(tc.mV[0] < 0.f)
						{
							tc.mV[0] = 1.0f + tc.mV[0] ;
						}
						if(tc.mV[1] < 0.f)
						{
							tc.mV[1] = 1.0f + tc.mV[1] ;
						}
						break;
					default:
						break;
					}
				
					tc.mV[0] = tcoord_xoffset + tcoord_xscale * tc.mV[0] ;
					tc.mV[1] = tcoord_yoffset + tcoord_yscale * tc.mV[1] ;
				}
				

				*tex_coords++ = tc;
				
				if (bump_code && mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD1))
				{
					LLVector4a tangent;
					tangent.setCross3(vf.mBinormals[i], vf.mNormals[i]);

					LLMatrix4a tangent_to_object;
					tangent_to_object.setRows(tangent, vf.mBinormals[i], vf.mNormals[i]);
					LLVector4a t;
					tangent_to_object.rotate(binormal_dir, t);
					LLVector4a binormal;
					mat_normal.rotate(t, binormal);
						
					//VECTORIZE THIS
					if (mDrawablep->isActive())
					{
						LLVector3 t;
						t.set(binormal.getF32ptr());
						t *= bump_quat;
						binormal.load3(t.mV);
					}

					binormal.normalize3fast();
					tc += LLVector2( bump_s_primary_light_ray.dot3(tangent).getF32(), bump_t_primary_light_ray.dot3(binormal).getF32() );
					
					*tex_coords2++ = tc;
				}	
			}
		}
	}

	if (rebuild_pos)
	{
		LLMatrix4a mat_vert;
		mat_vert.loadu(mat_vert_in);

		LLVector4a* src = vf.mPositions;
		LLVector4a* dst = vertices;

		LLVector4a* end = dst+num_vertices;
		do
		{	
			mat_vert.affineTransform(*src++, *dst++);
		}
		while(dst < end);

		F32 index = (F32) (mTextureIndex < 255 ? mTextureIndex : 0);
		F32 *index_dst = (F32*) vertices;
		F32 *index_end = (F32*) end;

		index_dst += 3;
		index_end += 3;
		do
		{
			*index_dst = index;
			index_dst += 4;
		}
		while (index_dst < index_end);
		
	}
		
	if (rebuild_normal)
	{
		for (S32 i = 0; i < num_vertices; i++)
		{	
			LLVector4a normal;
			mat_normal.rotate(vf.mNormals[i], normal);
			normal.normalize3fast();
			normals[i] = normal;
		}
	}
		
	if (rebuild_binormal)
	{
		for (S32 i = 0; i < num_vertices; i++)
		{	
			LLVector4a binormal;
			mat_normal.rotate(vf.mBinormals[i], binormal);
			binormal.normalize3fast();
			binormals[i] = binormal;
		}
	}
	
	if (rebuild_weights && vf.mWeights)
	{
		LLVector4a::memcpyNonAliased16((F32*) weights, (F32*) vf.mWeights, num_vertices*4*sizeof(F32));
	}

	if (rebuild_color)
	{
		LLVector4a src;

		U32 vec[4];
		vec[0] = vec[1] = vec[2] = vec[3] = color.mAll;
		
		src.loadua((F32*) vec);

		LLVector4a* dst = (LLVector4a*) colors.get();
		S32 num_vecs = num_vertices/4;
		if (num_vertices%4 > 0)
		{
			++num_vecs;
		}

		for (S32 i = 0; i < num_vecs; i++)
		{	
			dst[i] = src;
		}
	}

	if (rebuild_tcoord)
	{
		mTexExtents[0].setVec(0,0);
		mTexExtents[1].setVec(1,1);
		xform(mTexExtents[0], cos_ang, sin_ang, os, ot, ms, mt);
		xform(mTexExtents[1], cos_ang, sin_ang, os, ot, ms, mt);
		
		F32 es = vf.mTexCoordExtents[1].mV[0] - vf.mTexCoordExtents[0].mV[0] ;
		F32 et = vf.mTexCoordExtents[1].mV[1] - vf.mTexCoordExtents[0].mV[1] ;
		mTexExtents[0][0] *= es ;
		mTexExtents[1][0] *= es ;
		mTexExtents[0][1] *= et ;
		mTexExtents[1][1] *= et ;
	}

	mLastVertexBuffer = mVertexBuffer;
	mLastGeomCount = mGeomCount;
	mLastGeomIndex = mGeomIndex;
	mLastIndicesCount = mIndicesCount;
	mLastIndicesIndex = mIndicesIndex;

	return TRUE;
}

//check if the face has a media
BOOL LLFace::hasMedia() const 
{
	if(mHasMedia)
	{
		return TRUE ;
	}
	if(mTexture.notNull()) 
	{
		return mTexture->hasParcelMedia() ;  //if has a parcel media
	}

	return FALSE ; //no media.
}

const F32 LEAST_IMPORTANCE = 0.05f ;
const F32 LEAST_IMPORTANCE_FOR_LARGE_IMAGE = 0.3f ;

F32 LLFace::getTextureVirtualSize()
{
	F32 radius;
	F32 cos_angle_to_view_dir;	
	BOOL in_frustum = calcPixelArea(cos_angle_to_view_dir, radius);

	if (mPixelArea < F_ALMOST_ZERO || !in_frustum)
	{
		setVirtualSize(0.f) ;
		return 0.f;
	}

	//get area of circle in texture space
	LLVector2 tdim = mTexExtents[1] - mTexExtents[0];
	F32 texel_area = (tdim * 0.5f).lengthSquared()*3.14159f;
	if (texel_area <= 0)
	{
		// Probably animated, use default
		texel_area = 1.f;
	}

	F32 face_area;
	if (mVObjp->isSculpted() && texel_area > 1.f)
	{
		//sculpts can break assumptions about texel area
		face_area = mPixelArea;
	}
	else
	{
		//apply texel area to face area to get accurate ratio
		//face_area /= llclamp(texel_area, 1.f/64.f, 16.f);
		face_area =  mPixelArea / llclamp(texel_area, 0.015625f, 128.f);
	}

	face_area = LLFace::adjustPixelArea(mImportanceToCamera, face_area) ;
	if(face_area > LLViewerTexture::sMinLargeImageSize) //if is large image, shrink face_area by considering the partial overlapping.
	{
		if(mImportanceToCamera > LEAST_IMPORTANCE_FOR_LARGE_IMAGE && mTexture.notNull() && mTexture->isLargeImage())
		{		
			face_area *= adjustPartialOverlapPixelArea(cos_angle_to_view_dir, radius );
		}	
	}

	setVirtualSize(face_area) ;

	return face_area;
}

BOOL LLFace::calcPixelArea(F32& cos_angle_to_view_dir, F32& radius)
{
	//VECTORIZE THIS
	//get area of circle around face
	LLVector4a center;
	center.load3(getPositionAgent().mV);
	LLVector4a size;
	size.setSub(mExtents[1], mExtents[0]);
	size.mul(0.5f);

	LLViewerCamera* camera = LLViewerCamera::getInstance();

	F32 size_squared = size.dot3(size).getF32();
	LLVector4a lookAt;
	LLVector4a t;
	t.load3(camera->getOrigin().mV);
	lookAt.setSub(center, t);
	F32 dist = lookAt.getLength3().getF32();
	dist = llmax(dist-size.getLength3().getF32(), 0.f);
	lookAt.normalize3fast() ;	

	//get area of circle around node
	F32 app_angle = atanf((F32) sqrt(size_squared) / dist);
	radius = app_angle*LLDrawable::sCurPixelAngle;
	mPixelArea = radius*radius * 3.14159f;
	LLVector4a x_axis;
	x_axis.load3(camera->getXAxis().mV);
	cos_angle_to_view_dir = lookAt.dot3(x_axis).getF32();

	//if has media, check if the face is out of the view frustum.	
	if(hasMedia())
	{
		if(!camera->AABBInFrustum(center, size)) 
		{
			mImportanceToCamera = 0.f ;
			return false ;
		}
		if(cos_angle_to_view_dir > camera->getCosHalfFov()) //the center is within the view frustum
		{
			cos_angle_to_view_dir = 1.0f ;
		}
		else
		{		
			LLVector4a d;
			d.setSub(lookAt, x_axis);

			if(dist * dist * d.dot3(d) < size_squared)
			{
				cos_angle_to_view_dir = 1.0f ;
			}
		}
	}

	if(dist < mBoundingSphereRadius) //camera is very close
	{
		cos_angle_to_view_dir = 1.0f ;
		mImportanceToCamera = 1.0f ;
	}
	else
	{		
		mImportanceToCamera = LLFace::calcImportanceToCamera(cos_angle_to_view_dir, dist) ;
	}

	return true ;
}

//the projection of the face partially overlaps with the screen
F32 LLFace::adjustPartialOverlapPixelArea(F32 cos_angle_to_view_dir, F32 radius )
{
	F32 screen_radius = (F32)llmax(gViewerWindow->getWindowWidthRaw(), gViewerWindow->getWindowHeightRaw()) ;
	F32 center_angle = acosf(cos_angle_to_view_dir) ;
	F32 d = center_angle * LLDrawable::sCurPixelAngle ;

	if(d + radius > screen_radius + 5.f)
	{
		//----------------------------------------------
		//calculate the intersection area of two circles
		//F32 radius_square = radius * radius ;
		//F32 d_square = d * d ;
		//F32 screen_radius_square = screen_radius * screen_radius ;
		//face_area = 
		//	radius_square * acosf((d_square + radius_square - screen_radius_square)/(2 * d * radius)) +
		//	screen_radius_square * acosf((d_square + screen_radius_square - radius_square)/(2 * d * screen_radius)) -
		//	0.5f * sqrtf((-d + radius + screen_radius) * (d + radius - screen_radius) * (d - radius + screen_radius) * (d + radius + screen_radius)) ;			
		//----------------------------------------------

		//the above calculation is too expensive
		//the below is a good estimation: bounding box of the bounding sphere:
		F32 alpha = 0.5f * (radius + screen_radius - d) / radius ;
		alpha = llclamp(alpha, 0.f, 1.f) ;
		return alpha * alpha ;
	}
	return 1.0f ;
}

const S8 FACE_IMPORTANCE_LEVEL = 4 ;
const F32 FACE_IMPORTANCE_TO_CAMERA_OVER_DISTANCE[FACE_IMPORTANCE_LEVEL][2] = //{distance, importance_weight}
	{{16.1f, 1.0f}, {32.1f, 0.5f}, {48.1f, 0.2f}, {96.1f, 0.05f} } ;
const F32 FACE_IMPORTANCE_TO_CAMERA_OVER_ANGLE[FACE_IMPORTANCE_LEVEL][2] =    //{cos(angle), importance_weight}
	{{0.985f /*cos(10 degrees)*/, 1.0f}, {0.94f /*cos(20 degrees)*/, 0.8f}, {0.866f /*cos(30 degrees)*/, 0.64f}, {0.0f, 0.36f}} ;

//static 
F32 LLFace::calcImportanceToCamera(F32 cos_angle_to_view_dir, F32 dist)
{
	F32 importance = 0.f ;
	
	if(cos_angle_to_view_dir > LLViewerCamera::getInstance()->getCosHalfFov() && 
		dist < FACE_IMPORTANCE_TO_CAMERA_OVER_DISTANCE[FACE_IMPORTANCE_LEVEL - 1][0]) 
	{
		LLViewerCamera* camera = LLViewerCamera::getInstance();
		F32 camera_moving_speed = camera->getAverageSpeed() ;
		F32 camera_angular_speed = camera->getAverageAngularSpeed();

		if(camera_moving_speed > 10.0f || camera_angular_speed > 1.0f)
		{
			//if camera moves or rotates too fast, ignore the importance factor
			return 0.f ;
		}
		
		//F32 camera_relative_speed = camera_moving_speed * (lookAt * LLViewerCamera::getInstance()->getVelocityDir()) ;
		
		S32 i = 0 ;
		for(i = 0; i < FACE_IMPORTANCE_LEVEL && dist > FACE_IMPORTANCE_TO_CAMERA_OVER_DISTANCE[i][0]; ++i);
		i = llmin(i, FACE_IMPORTANCE_LEVEL - 1) ;
		F32 dist_factor = FACE_IMPORTANCE_TO_CAMERA_OVER_DISTANCE[i][1] ;
		
		for(i = 0; i < FACE_IMPORTANCE_LEVEL && cos_angle_to_view_dir < FACE_IMPORTANCE_TO_CAMERA_OVER_ANGLE[i][0] ; ++i) ;
		i = llmin(i, FACE_IMPORTANCE_LEVEL - 1) ;
		importance = dist_factor * FACE_IMPORTANCE_TO_CAMERA_OVER_ANGLE[i][1] ;
	}

	return importance ;
}

//static 
F32 LLFace::adjustPixelArea(F32 importance, F32 pixel_area)
{
	if(pixel_area > LLViewerTexture::sMaxSmallImageSize)
	{
		if(importance < LEAST_IMPORTANCE) //if the face is not important, do not load hi-res.
		{
			static const F32 MAX_LEAST_IMPORTANCE_IMAGE_SIZE = 128.0f * 128.0f ;
			pixel_area = llmin(pixel_area * 0.5f, MAX_LEAST_IMPORTANCE_IMAGE_SIZE) ;
		}
		else if(pixel_area > LLViewerTexture::sMinLargeImageSize) //if is large image, shrink face_area by considering the partial overlapping.
		{
			if(importance < LEAST_IMPORTANCE_FOR_LARGE_IMAGE)//if the face is not important, do not load hi-res.
			{
				pixel_area = LLViewerTexture::sMinLargeImageSize ;
			}				
		}
	}

	return pixel_area ;
}

BOOL LLFace::verify(const U32* indices_array) const
{
	BOOL ok = TRUE;

	if( mVertexBuffer.isNull() )
	{ //no vertex buffer, face is implicitly valid
		return TRUE;
	}
	
	// First, check whether the face data fits within the pool's range.
	if ((mGeomIndex + mGeomCount) > mVertexBuffer->getRequestedVerts())
	{
		ok = FALSE;
		llinfos << "Face references invalid vertices!" << llendl;
	}

	S32 indices_count = (S32)getIndicesCount();
	
	if (!indices_count)
	{
		return TRUE;
	}
	
	if (indices_count > LL_MAX_INDICES_COUNT)
	{
		ok = FALSE;
		llinfos << "Face has bogus indices count" << llendl;
	}
	
	if (mIndicesIndex + mIndicesCount > mVertexBuffer->getRequestedIndices())
	{
		ok = FALSE;
		llinfos << "Face references invalid indices!" << llendl;
	}

#if 0
	S32 geom_start = getGeomStart();
	S32 geom_count = mGeomCount;

	const U32 *indicesp = indices_array ? indices_array + mIndicesIndex : getRawIndices();

	for (S32 i = 0; i < indices_count; i++)
	{
		S32 delta = indicesp[i] - geom_start;
		if (0 > delta)
		{
			llwarns << "Face index too low!" << llendl;
			llinfos << "i:" << i << " Index:" << indicesp[i] << " GStart: " << geom_start << llendl;
			ok = FALSE;
		}
		else if (delta >= geom_count)
		{
			llwarns << "Face index too high!" << llendl;
			llinfos << "i:" << i << " Index:" << indicesp[i] << " GEnd: " << geom_start + geom_count << llendl;
			ok = FALSE;
		}
	}
#endif

	if (!ok)
	{
		printDebugInfo();
	}
	return ok;
}


void LLFace::setViewerObject(LLViewerObject* objp)
{
	mVObjp = objp;
}

const LLColor4& LLFace::getRenderColor() const
{
	if (isState(USE_FACE_COLOR))
	{
		  return mFaceColor; // Face Color
	}
	else
	{
		const LLTextureEntry* tep = getTextureEntry();
		return (tep ? tep->getColor() : LLColor4::white);
	}
}
	
void LLFace::renderSetColor() const
{
	if (!LLFacePool::LLOverrideFaceColor::sOverrideFaceColor)
	{
		const LLColor4* color = &(getRenderColor());
		
		glColor4fv(color->mV);
	}
}

S32 LLFace::pushVertices(const U16* index_array) const
{
	if (mIndicesCount)
	{
		U32 render_type = LLRender::TRIANGLES;
		if (mDrawInfo)
		{
			render_type = mDrawInfo->mDrawMode;
		}
		mVertexBuffer->drawRange(render_type, mGeomIndex, mGeomIndex+mGeomCount-1, mIndicesCount, mIndicesIndex);
		gPipeline.addTrianglesDrawn(mIndicesCount, render_type);
	}

	return mIndicesCount;
}

const LLMatrix4& LLFace::getRenderMatrix() const
{
	return mDrawablep->getRenderMatrix();
}

S32 LLFace::renderElements(const U16 *index_array) const
{
	S32 ret = 0;
	
	if (isState(GLOBAL))
	{	
		ret = pushVertices(index_array);
	}
	else
	{
		glPushMatrix();
		glMultMatrixf((float*)getRenderMatrix().mMatrix);
		ret = pushVertices(index_array);
		glPopMatrix();
	}
	
	return ret;
}

S32 LLFace::renderIndexed()
{
	if(mDrawablep.isNull() || mDrawPoolp == NULL)
	{
		return 0;
	}
	
	return renderIndexed(mDrawPoolp->getVertexDataMask());
}

S32 LLFace::renderIndexed(U32 mask)
{
	if (mVertexBuffer.isNull())
	{
		return 0;
	}

	mVertexBuffer->setBuffer(mask);
	U16* index_array = (U16*) mVertexBuffer->getIndicesPointer();
	return renderElements(index_array);
}

//============================================================================
// From llface.inl

S32 LLFace::getColors(LLStrider<LLColor4U> &colors)
{
	if (!mGeomCount)
	{
		return -1;
	}
	
	// llassert(mGeomIndex >= 0);
	mVertexBuffer->getColorStrider(colors, mGeomIndex);
	return mGeomIndex;
}

S32	LLFace::getIndices(LLStrider<U16> &indicesp)
{
	mVertexBuffer->getIndexStrider(indicesp, mIndicesIndex);
	llassert(indicesp[0] != indicesp[1]);
	return mIndicesIndex;
}

LLVector3 LLFace::getPositionAgent() const
{
	if (mDrawablep->isStatic())
	{
		return mCenterAgent;
	}
	else
	{
		return mCenterLocal * getRenderMatrix();
	}
}

//
//atlas
//
void LLFace::removeAtlas()
{
	setAtlasInUse(FALSE) ;
	mAtlasInfop = NULL ;	
}

const LLTextureAtlas* LLFace::getAtlas()const 
{
	if(mAtlasInfop)
	{
		return mAtlasInfop->getAtlas() ;
	}
	return NULL ;
}

const LLVector2* LLFace::getTexCoordOffset()const 
{
	if(isAtlasInUse())
	{
		return mAtlasInfop->getTexCoordOffset() ;
	}
	return NULL ;
}
const LLVector2* LLFace::getTexCoordScale() const 
{
	if(isAtlasInUse())
	{
		return mAtlasInfop->getTexCoordScale() ;
	}
	return NULL ;
}

BOOL LLFace::isAtlasInUse()const
{
	return mUsingAtlas ;
}

BOOL LLFace::canUseAtlas()const
{
	//no drawable or no spatial group, do not use atlas
	if(!mDrawablep || !mDrawablep->getSpatialGroup())
	{
		return FALSE ;
	}

	//if bump face, do not use atlas
	if(getTextureEntry() && getTextureEntry()->getBumpmap())
	{
		return FALSE ;
	}

	//if animated texture, do not use atlas
	if(isState(TEXTURE_ANIM))
	{
		return FALSE ;
	}

	return TRUE ;
}

void LLFace::setAtlasInUse(BOOL flag)
{
	//no valid atlas to use.
	if(flag && (!mAtlasInfop || !mAtlasInfop->isValid()))
	{
		flag = FALSE ;
	}

	if(!flag && !mUsingAtlas)
	{
		return ;
	}

	//
	//at this stage (flag || mUsingAtlas) is always true.
	//

	//rebuild the tex coords
	if(mDrawablep)
	{
		gPipeline.markRebuild(mDrawablep, LLDrawable::REBUILD_TCOORD);
		mUsingAtlas = flag ;
	}
	else
	{
		mUsingAtlas = FALSE ;
	}
}

LLTextureAtlasSlot* LLFace::getAtlasInfo()
{
	return mAtlasInfop ;
}

void LLFace::setAtlasInfo(LLTextureAtlasSlot* atlasp)
{	
	if(mAtlasInfop != atlasp)
	{
		if(mAtlasInfop)
		{
			//llerrs << "Atlas slot changed!" << llendl ;
		}
		mAtlasInfop = atlasp ;
	}
}

LLViewerTexture* LLFace::getTexture() const
{
	if(isAtlasInUse())
	{
		return (LLViewerTexture*)mAtlasInfop->getAtlas() ;
	}

	return mTexture ;
}

//switch to atlas or switch back to gl texture 
//return TRUE if using atlas.
BOOL LLFace::switchTexture()
{
	//no valid atlas or texture
	if(!mAtlasInfop || !mAtlasInfop->isValid() || !mTexture)
	{
		return FALSE ;
	}
	
	if(mTexture->getTexelsInAtlas() >= (U32)mVSize || 
		mTexture->getTexelsInAtlas() >= mTexture->getTexelsInGLTexture())
	{
		//switch to use atlas
		//atlas resolution is qualified, use it.		
		if(!mUsingAtlas)
		{
			setAtlasInUse(TRUE) ;
		}
	}
	else //if atlas not qualified.
	{
		//switch back to GL texture
		if(mUsingAtlas && mTexture->isGLTextureCreated() && 
			mTexture->getDiscardLevel() < mTexture->getDiscardLevelInAtlas())
		{
			setAtlasInUse(FALSE) ;
		}
	}

	return mUsingAtlas ;
}


void LLFace::setVertexBuffer(LLVertexBuffer* buffer)
{
	mVertexBuffer = buffer;
	llassert(verify());
}

void LLFace::clearVertexBuffer()
{
	mVertexBuffer = NULL;
	mLastVertexBuffer = NULL;
}

//static
U32 LLFace::getRiggedDataMask(U32 type)
{
	static const U32 rigged_data_mask[] = {
		LLDrawPoolAvatar::RIGGED_SIMPLE_MASK,
		LLDrawPoolAvatar::RIGGED_FULLBRIGHT_MASK,
		LLDrawPoolAvatar::RIGGED_SHINY_MASK,
		LLDrawPoolAvatar::RIGGED_FULLBRIGHT_SHINY_MASK,
		LLDrawPoolAvatar::RIGGED_GLOW_MASK,
		LLDrawPoolAvatar::RIGGED_ALPHA_MASK,
		LLDrawPoolAvatar::RIGGED_FULLBRIGHT_ALPHA_MASK,
		LLDrawPoolAvatar::RIGGED_DEFERRED_BUMP_MASK,						 
		LLDrawPoolAvatar::RIGGED_DEFERRED_SIMPLE_MASK,
	};

	llassert(type < sizeof(rigged_data_mask)/sizeof(U32));

	return rigged_data_mask[type];
}

U32 LLFace::getRiggedVertexBufferDataMask() const
{
	U32 data_mask = 0;
	for (U32 i = 0; i < mRiggedIndex.size(); ++i)
	{
		if (mRiggedIndex[i] > -1)
		{
			data_mask |= LLFace::getRiggedDataMask(i);
		}
	}

	return data_mask;
}

S32 LLFace::getRiggedIndex(U32 type) const
{
	if (mRiggedIndex.empty())
	{
		return -1;
	}

	llassert(type < mRiggedIndex.size());

	return mRiggedIndex[type];
}

void LLFace::setRiggedIndex(U32 type, S32 index)
{
	if (mRiggedIndex.empty())
	{
		mRiggedIndex.resize(LLDrawPoolAvatar::NUM_RIGGED_PASSES);
		for (U32 i = 0; i < mRiggedIndex.size(); ++i)
		{
			mRiggedIndex[i] = -1;
		}
	}

	llassert(type < mRiggedIndex.size());

	mRiggedIndex[type] = index;
}

