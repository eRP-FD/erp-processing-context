/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_WRAPPERS_RAPIDJSON_HXX
#define EPA_LIBRARY_WRAPPERS_RAPIDJSON_HXX

#include "library/util/Assert.hxx"
#include "library/wrappers/detail/DisableWarnings.hxx"

#ifndef RAPIDJSON_HAS_STDSTRING
#define RAPIDJSON_HAS_STDSTRING 1
#endif

#ifndef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x) Assert(x) << "RapidJson assert failed"
#endif

DISABLE_WARNINGS_BEGIN

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

DISABLE_WARNINGS_END

#endif
