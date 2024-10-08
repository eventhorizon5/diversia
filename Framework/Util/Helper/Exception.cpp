/*
-----------------------------------------------------------------------------
Copyright (c) 2008-2010 Diversia

Original file used from the OGRE project, license:
-----------------------------------------------------------------------------
Copyright (c) 2000-2009 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#include "Util/Platform/StableHeaders.h"

#include "Util/Helper/Exception.h"
#include "Util/Helper/StringUtil.h"
#include "Util/Log/Log.h"

#if DIVERSIA_COMPILER == DIVERSIA_COMPILER_BORL
    #include <stdio.h>
#endif

namespace Diversia 
{
namespace Util 
{
Exception::Exception(int num, const String& desc, const String& src) :
    line( 0 ),
    number( num ),
    description( desc ),
    source( src )
{
}

Exception::Exception(int num, const String& desc, const String& src, 
	const char* typ, const char* fil, long lin) :
    line( lin ),
    number( num ),
	typeName(typ),
    description( desc ),
    source( src ),
    file( fil )
{

}

Exception::Exception(const Exception& rhs)
    : line( rhs.line ), 
	number( rhs.number ), 
	typeName( rhs.typeName ), 
	description( rhs.description ), 
	source( rhs.source ), 
	file( rhs.file )
{
}

void Exception::operator = ( const Exception& rhs )
{
    description = rhs.description;
    number = rhs.number;
    source = rhs.source;
    file = rhs.file;
    line = rhs.line;
	typeName = rhs.typeName;
}

const String& Exception::getFullDescription( bool unhandled /*= false*/ ) const
{
	if( fullDesc.empty() )
	{
		StringUtil::StrStreamType desc;

		desc    << String( unhandled ? "DIVERSIA UNHANDLED EXCEPTION(" : "DIVERSIA EXCEPTION(" ) << 
                number << ":" << typeName << "): " << description  << " in " << source;

		if( line > 0 )
		{
			desc << " at " << file << " (line " << line << ")";
		}

		fullDesc = desc.str();
	}

	return fullDesc;
}

int Exception::getNumber() const throw()
{
    return number;
}

void Exception::log() const
{
    ULOGE << this->getFullDescription();
}

void Exception::log( 
    boost::log::sources::severity_channel_logger_mt< Diversia::Util::LogLevel >& rLogger, 
    bool unhandled /*= false*/ ) const
{
    BOOST_LOG_SEV( rLogger, unhandled ? Diversia::Util::LOG_CRITICAL : Diversia::Util::LOG_ERROR ) 
        << this->getFullDescription();
}

} // Namespace Util
} // Namespace Diversia
