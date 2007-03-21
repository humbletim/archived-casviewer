/** 
 * @file lldrawpoolsimple.h
 * @brief LLDrawPoolSimple class definition
 *
 * Copyright (c) 2002-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

#ifndef LL_LLDRAWPOOLSIMPLE_H
#define LL_LLDRAWPOOLSIMPLE_H

#include "lldrawpool.h"

class LLDrawPoolSimple : public LLRenderPass
{
public:
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD |
							LLVertexBuffer::MAP_COLOR
	};
	virtual U32 getVertexDataMask() { return VERTEX_DATA_MASK; }

	LLDrawPoolSimple();
	
	/*virtual*/ void beginRenderPass(S32 pass);
	/*virtual*/ void render(S32 pass = 0);
	/*virtual*/ void prerender();

};

class LLDrawPoolGlow : public LLRenderPass
{
public:
	LLDrawPoolGlow(): LLRenderPass(LLDrawPool::POOL_GLOW) { }
	
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_TEXCOORD
	};

	virtual U32 getVertexDataMask() { return VERTEX_DATA_MASK; }

	virtual void prerender() { }

	void render(S32 pass = 0);
	void pushBatch(LLDrawInfo& params, U32 mask, BOOL texture = TRUE);

};

#endif // LL_LLDRAWPOOLSIMPLE_H
