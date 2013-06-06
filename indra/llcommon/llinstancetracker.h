/** 
 * @file llinstancetracker.h
 * @brief LLInstanceTracker is a mixin class that automatically tracks object
 *        instances with or without an associated key
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
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

#ifndef LL_LLINSTANCETRACKER_H
#define LL_LLINSTANCETRACKER_H

#include <map>
#include <typeinfo>

#include "string_table.h"
#include <boost/utility.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/indirect_iterator.hpp>

/**
 * Base class manages "class-static" data that must actually have singleton
 * semantics: one instance per process, rather than one instance per module as
 * sometimes happens with data simply declared static.
 */
class LL_COMMON_API LLInstanceTrackerBase
{
protected:
    /// It's not essential to derive your STATICDATA (for use with
    /// getStatic()) from StaticBase; it's just that both known
    /// implementations do.
    struct StaticBase
    {
        StaticBase():
            sIterationNestDepth(0)
        {}

		void incrementDepth();
		void decrementDepth();
		U32 getDepth();
	private:
		U32 sIterationNestDepth;
    };
};

/// This mix-in class adds support for tracking all instances of the specified class parameter T
/// The (optional) key associates a value of type KEY with a given instance of T, for quick lookup
/// If KEY is not provided, then instances are stored in a simple set
/// @NOTE: see explicit specialization below for default KEY==void case
/// @NOTE: this class is not thread-safe unless used as read-only
template<typename T, typename KEY = void>
class LLInstanceTracker : public LLInstanceTrackerBase
{
	typedef LLInstanceTracker<T, KEY> MyT;
	typedef typename std::map<KEY, T*> InstanceMap;
	struct StaticData: public StaticBase
	{
		InstanceMap sMap;
	};
	static StaticData& getStatic() { static StaticData sData; return sData;}
	static InstanceMap& getMap_() { return getStatic().sMap; }

public:
	class instance_iter : public boost::iterator_facade<instance_iter, T, boost::forward_traversal_tag>
	{
	public:
		typedef boost::iterator_facade<instance_iter, T, boost::forward_traversal_tag> super_t;
		
		instance_iter(const typename InstanceMap::iterator& it)
		:	mIterator(it)
		{
			getStatic().incrementDepth();
		}

		~instance_iter()
		{
			getStatic().decrementDepth();
		}


	private:
		friend class boost::iterator_core_access;

		void increment() { mIterator++; }
		bool equal(instance_iter const& other) const
		{
			return mIterator == other.mIterator;
		}

		T& dereference() const
		{
			return *(mIterator->second);
		}

		typename InstanceMap::iterator mIterator;
	};

	class key_iter : public boost::iterator_facade<key_iter, KEY, boost::forward_traversal_tag>
	{
	public:
		typedef boost::iterator_facade<key_iter, KEY, boost::forward_traversal_tag> super_t;

		key_iter(typename InstanceMap::iterator it)
		:	mIterator(it)
		{
			getStatic().incrementDepth();
		}

		key_iter(const key_iter& other)
		:	mIterator(other.mIterator)
		{
			getStatic().incrementDepth();
		}

		~key_iter()
		{
			getStatic().decrementDepth();
		}


	private:
		friend class boost::iterator_core_access;

		void increment() { mIterator++; }
		bool equal(key_iter const& other) const
		{
			return mIterator == other.mIterator;
		}

		KEY& dereference() const
		{
			return const_cast<KEY&>(mIterator->first);
		}

		typename InstanceMap::iterator mIterator;
	};

	static T* getInstance(const KEY& k)
	{
		const InstanceMap& map(getMap_());
		typename InstanceMap::const_iterator found = map.find(k);
		return (found == map.end()) ? NULL : found->second;
	}

	static instance_iter beginInstances() 
	{	
		return instance_iter(getMap_().begin()); 
	}

	static instance_iter endInstances() 
	{
		return instance_iter(getMap_().end());
	}

	static S32 instanceCount() 
	{ 
		return getMap_().size(); 
	}

	static key_iter beginKeys()
	{
		return key_iter(getMap_().begin());
	}
	static key_iter endKeys()
	{
		return key_iter(getMap_().end());
	}

protected:
	LLInstanceTracker(KEY key) 
	{ 
		// make sure static data outlives all instances
		getStatic();
		add_(key); 
	}
	virtual ~LLInstanceTracker() 
	{ 
		// it's unsafe to delete instances of this type while all instances are being iterated over.
		llassert_always(getStatic().getDepth() == 0);
		remove_();		
	}
	virtual void setKey(KEY key) { remove_(); add_(key); }
	virtual const KEY& getKey() const { return mInstanceKey; }

private:
	LLInstanceTracker( const LLInstanceTracker& );
	const LLInstanceTracker& operator=( const LLInstanceTracker& );

	void add_(KEY key) 
	{ 
		mInstanceKey = key; 
		getMap_()[key] = static_cast<T*>(this); 
	}
	void remove_()
	{
		getMap_().erase(mInstanceKey);
	}

private:
	KEY mInstanceKey;
};

/// explicit specialization for default case where KEY is void
/// use a simple std::set<T*>
template<typename T>
class LLInstanceTracker<T, void> : public LLInstanceTrackerBase
{
	typedef LLInstanceTracker<T, void> MyT;
	typedef typename std::set<T*> InstanceSet;
	struct StaticData: public StaticBase
	{
		InstanceSet sSet;
	};
	static StaticData& getStatic() { static StaticData sData; return sData; }
	static InstanceSet& getSet_() { return getStatic().sSet; }

public:

	/**
	 * Does a particular instance still exist? Of course, if you already have
	 * a T* in hand, you need not call getInstance() to @em locate the
	 * instance -- unlike the case where getInstance() accepts some kind of
	 * key. Nonetheless this method is still useful to @em validate a
	 * particular T*, since each instance's destructor removes itself from the
	 * underlying set.
	 */
	static T* getInstance(T* k)
	{
		const InstanceSet& set(getSet_());
		typename InstanceSet::const_iterator found = set.find(k);
		return (found == set.end())? NULL : *found;
	}
	static S32 instanceCount() { return getSet_().size(); }

	class instance_iter : public boost::iterator_facade<instance_iter, T, boost::forward_traversal_tag>
	{
	public:
		instance_iter(const typename InstanceSet::iterator& it)
		:	mIterator(it)
		{
			getStatic().incrementDepth();
		}

		instance_iter(const instance_iter& other)
		:	mIterator(other.mIterator)
		{
			getStatic().incrementDepth();
		}

		~instance_iter()
		{
			getStatic().decrementDepth();
		}

	private:
		friend class boost::iterator_core_access;

		void increment() { mIterator++; }
		bool equal(instance_iter const& other) const
		{
			return mIterator == other.mIterator;
		}

		T& dereference() const
		{
			return **mIterator;
		}

		typename InstanceSet::iterator mIterator;
	};

	static instance_iter beginInstances() {	return instance_iter(getSet_().begin()); }
	static instance_iter endInstances() { return instance_iter(getSet_().end()); }

protected:
	LLInstanceTracker()
	{
		// make sure static data outlives all instances
		getStatic();
		getSet_().insert(static_cast<T*>(this));
	}
	virtual ~LLInstanceTracker()
	{
		// it's unsafe to delete instances of this type while all instances are being iterated over.
		llassert_always(getStatic().getDepth() == 0);
		getSet_().erase(static_cast<T*>(this));
	}

	LLInstanceTracker(const LLInstanceTracker& other)
	{
		getSet_().insert(static_cast<T*>(this));
	}
};

#endif
