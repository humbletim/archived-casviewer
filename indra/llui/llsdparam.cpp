/** 
 * @file llsdparam.cpp
 * @brief parameter block abstraction for creating complex objects and 
 * parsing construction parameters from xml and LLSD
 *
 * $LicenseInfo:firstyear=2008&license=viewerlgpl$
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

#include "linden_common.h"

// Project includes
#include "llsdparam.h"

static 	LLInitParam::Parser::parser_read_func_map_t sReadFuncs;
static 	LLInitParam::Parser::parser_write_func_map_t sWriteFuncs;
static 	LLInitParam::Parser::parser_inspect_func_map_t sInspectFuncs;

//
// LLParamSDParser
//
LLParamSDParser::LLParamSDParser()
: Parser(sReadFuncs, sWriteFuncs, sInspectFuncs)
{
	using boost::bind;

	if (sReadFuncs.empty())
	{
		registerParserFuncs<S32>(readS32, &LLParamSDParser::writeTypedValue<S32>);
		registerParserFuncs<U32>(readU32, &LLParamSDParser::writeU32Param);
		registerParserFuncs<F32>(readF32, &LLParamSDParser::writeTypedValue<F32>);
		registerParserFuncs<F64>(readF64, &LLParamSDParser::writeTypedValue<F64>);
		registerParserFuncs<bool>(readBool, &LLParamSDParser::writeTypedValue<F32>);
		registerParserFuncs<std::string>(readString, &LLParamSDParser::writeTypedValue<std::string>);
		registerParserFuncs<LLUUID>(readUUID, &LLParamSDParser::writeTypedValue<LLUUID>);
		registerParserFuncs<LLDate>(readDate, &LLParamSDParser::writeTypedValue<LLDate>);
		registerParserFuncs<LLURI>(readURI, &LLParamSDParser::writeTypedValue<LLURI>);
		registerParserFuncs<LLSD>(readSD, &LLParamSDParser::writeTypedValue<LLSD>);
	}
}

// special case handling of U32 due to ambiguous LLSD::assign overload
bool LLParamSDParser::writeU32Param(LLParamSDParser::parser_t& parser, const void* val_ptr, const parser_t::name_stack_t& name_stack)
{
	LLParamSDParser& sdparser = static_cast<LLParamSDParser&>(parser);
	if (!sdparser.mWriteSD) return false;
	
	LLSD* sd_to_write = sdparser.getSDWriteNode(name_stack);
	if (!sd_to_write) return false;

	sd_to_write->assign((S32)*((const U32*)val_ptr));
	return true;
}

void LLParamSDParser::readSD(const LLSD& sd, LLInitParam::BaseBlock& block, bool silent)
{
	mCurReadSD = NULL;
	mNameStack.clear();
	setParseSilently(silent);

	readSDValues(sd, block);
}

void LLParamSDParser::writeSD(LLSD& sd, const LLInitParam::BaseBlock& block)
{
	mWriteSD = &sd;
	block.serializeBlock(*this);
}

void LLParamSDParser::readSDValues(const LLSD& sd, LLInitParam::BaseBlock& block)
{
	if (sd.isMap())
	{
		for (LLSD::map_const_iterator it = sd.beginMap();
			it != sd.endMap();
			++it)
		{
			mNameStack.push_back(make_pair(it->first, newParseGeneration()));
			readSDValues(it->second, block);
			mNameStack.pop_back();
		}
	}
	else if (sd.isArray())
	{
		for (LLSD::array_const_iterator it = sd.beginArray();
			it != sd.endArray();
			++it)
		{
			mNameStack.back().second = newParseGeneration();
			readSDValues(*it, block);
		}
	}
	else
	{
		mCurReadSD = &sd;
		block.submitValue(mNameStack, *this);
	}
}

/*virtual*/ std::string LLParamSDParser::getCurrentElementName()
{
	std::string full_name = "sd";
	for (name_stack_t::iterator it = mNameStack.begin();	
		it != mNameStack.end();
		++it)
	{
		full_name += llformat("[%s]", it->first.c_str());
	}

	return full_name;
}

LLSD* LLParamSDParser::getSDWriteNode(const parser_t::name_stack_t& name_stack)
{
	//TODO: implement nested LLSD writing
	return mWriteSD;
}

bool LLParamSDParser::readS32(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

    *((S32*)val_ptr) = self.mCurReadSD->asInteger();
    return true;
}

bool LLParamSDParser::readU32(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

    *((U32*)val_ptr) = self.mCurReadSD->asInteger();
    return true;
}

bool LLParamSDParser::readF32(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

    *((F32*)val_ptr) = self.mCurReadSD->asReal();
    return true;
}

bool LLParamSDParser::readF64(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

    *((F64*)val_ptr) = self.mCurReadSD->asReal();
    return true;
}

bool LLParamSDParser::readBool(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

    *((bool*)val_ptr) = self.mCurReadSD->asBoolean();
    return true;
}

bool LLParamSDParser::readString(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

	*((std::string*)val_ptr) = self.mCurReadSD->asString();
    return true;
}

bool LLParamSDParser::readUUID(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

	*((LLUUID*)val_ptr) = self.mCurReadSD->asUUID();
    return true;
}

bool LLParamSDParser::readDate(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

	*((LLDate*)val_ptr) = self.mCurReadSD->asDate();
    return true;
}

bool LLParamSDParser::readURI(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

	*((LLURI*)val_ptr) = self.mCurReadSD->asURI();
    return true;
}

bool LLParamSDParser::readSD(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

	*((LLSD*)val_ptr) = *self.mCurReadSD;
    return true;
}
