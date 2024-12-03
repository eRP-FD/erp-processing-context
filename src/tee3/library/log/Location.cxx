/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/log/Location.hxx"

#include <cstring>

#include "shared/util/Hash.hxx"

namespace epa
{

Location::Location(const char* file, const int line)
  : mFile(file),
    mLine(line)
{
}


std::string Location::toString() const
{
    return std::string(mFile) + ":" + std::to_string(mLine);
}


std::string Location::toShortString() const
{
    const char* name = mFile;
    if (const char* p = std::strrchr(name, '/'))
        name = p + 1;
#ifdef _WIN32
    if (const char* p = std::strrchr(name, '\\'))
        name = p + 1;
#endif
    return std::string(name) + ":" + std::to_string(mLine);
}


const char* Location::file() const
{
    return mFile;
}


int Location::line() const
{
    return mLine;
}


int32_t Location::calculateProgramLocationCode() const
{
    std::string hash = Hash::sha256(toShortString());
    int32_t code = 0;
    for (size_t i = 0; i < 4 && i < hash.size(); i++)
        code = (code << 8) | static_cast<unsigned char>(hash[i]);
    code &= 0x7fffffff;
    return code;
}
}
