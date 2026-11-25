/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

// "Expect.hxx" defines some macros that cause conflicts in other headers
#include "shared/util/Expect.txx"
#include "shared/model/ModelException.hxx"

#ifdef RAPIDJSON_ASSERT
#undef RAPIDJSON_ASSERT
#endif

#define RAPIDJSON_ASSERT(expression)                                                                                   \
    local::throwIfNot<model::ModelException>(                                                                          \
        static_cast<bool>(expression), #expression,                                                                    \
        [&] {                                                                                                          \
            return ("rapidjson assertion");                                                                            \
        },                                                                                                             \
        FileNameAndLineNumber(__FILE__, __LINE__))
