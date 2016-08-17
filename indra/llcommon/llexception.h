/**
 * @file   llexception.h
 * @author Nat Goodspeed
 * @date   2016-06-29
 * @brief  Types needed for generic exception handling
 * 
 * $LicenseInfo:firstyear=2016&license=viewerlgpl$
 * Copyright (c) 2016, Linden Research, Inc.
 * $/LicenseInfo$
 */

#if ! defined(LL_LLEXCEPTION_H)
#define LL_LLEXCEPTION_H

#include <stdexcept>
#include <boost/exception/exception.hpp>

// "Found someone who can comfort me
//  But there are always exceptions..."
//  - Empty Pages, Traffic, from John Barleycorn (1970)
//    https://www.youtube.com/watch?v=dRH0CGVK7ic

/**
 * LLException is intended as the common base class from which all
 * viewer-specific exceptions are derived. Rationale for why it's derived from
 * both std::exception and boost::exception is explained in
 * tests/llexception_test.cpp.
 *
 * boost::current_exception_diagnostic_information() is quite wonderful: if
 * all we need to do with an exception is log it, in most places we should
 * catch (...) and log boost::current_exception_diagnostic_information().
 *
 * Please use BOOST_THROW_EXCEPTION()
 * http://www.boost.org/doc/libs/release/libs/exception/doc/BOOST_THROW_EXCEPTION.html
 * to throw viewer exceptions whenever possible. This enriches the exception's
 * diagnostic_information() with the source file, line and containing function
 * of the BOOST_THROW_EXCEPTION() macro.
 *
 * There may be circumstances in which it would be valuable to distinguish an
 * exception explicitly thrown by viewer code from an exception thrown by
 * (say) a third-party library. Catching (const LLException&) supports such
 * usage. However, most of the value of this base class is in the
 * diagnostic_information() available via Boost.Exception.
 */
struct LLException:
    public std::runtime_error,
    public boost::exception
{
    LLException(const std::string& what):
        std::runtime_error(what)
    {}
};

/**
 * The point of LLContinueError is to distinguish exceptions that need not
 * terminate the whole viewer session. In general, an uncaught exception will
 * be logged and will crash the viewer. However, though an uncaught exception
 * derived from LLContinueError will still be logged, the viewer will attempt
 * to continue processing.
 */
struct LLContinueError: public LLException
{
    LLContinueError(const std::string& what):
        LLException(what)
    {}
};

/// Call this macro from a catch (...) clause
#define CRASH_ON_UNHANDLED_EXCEPTION() \
     crash_on_unhandled_exception_(__FILE__, __LINE__, __PRETTY_FUNCTION__)
void crash_on_unhandled_exception_(const char*, int, const char*);

/// Call this from a catch (const LLContinueError&) clause
#define LOG_UNHANDLED_EXCEPTION(EXC) \
     log_unhandled_exception_(__FILE__, __LINE__, __PRETTY_FUNCTION__, EXC)
void log_unhandled_exception_(const char*, int, const char*, const LLContinueError&);

#endif /* ! defined(LL_LLEXCEPTION_H) */
