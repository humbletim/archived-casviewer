/** 
 * @file qtoolalign.h
 * @brief A tool to align objects
 */

#ifndef Q_QTOOLALIGN_H
#define Q_QTOOLALIGN_H

#include "lltool.h"
#include "llbbox.h"

class LLViewerObject;
class LLPickInfo;
class LLToolSelectRect;

class AlignThread : public LLThread
{
public:
	AlignThread(void);
	~AlignThread();	
	/*virtual*/ void run(void);
	/*virtual*/ void shutdown(void);
	static AlignThread* sInstance;
};

class QToolAlign
:	public LLTool, public LLSingleton<QToolAlign>
{
public:
	QToolAlign();
	virtual ~QToolAlign();

	virtual void	handleSelect();
	virtual void	handleDeselect();
	virtual BOOL	handleMouseDown(S32 x, S32 y, MASK mask);
	virtual BOOL    handleHover(S32 x, S32 y, MASK mask);
	virtual void	render();
	virtual BOOL	canAffectSelection();

	static void pickCallback(const LLPickInfo& pick_info);
	static void aligndone();
	static QToolAlign* getInstance1(){ return sInstance; }

	LLBBox          mBBox;
	F32             mManipulatorSize;
	S32             mHighlightedAxis;
	F32             mHighlightedDirection;
	BOOL            mForce;

private:
	static QToolAlign* sInstance;
	void            align();
	void            computeManipulatorSize();
	void            renderManipulators();
	BOOL            findSelectedManipulator(S32 x, S32 y);
};

#endif // Q_QTOOLALIGN_H
