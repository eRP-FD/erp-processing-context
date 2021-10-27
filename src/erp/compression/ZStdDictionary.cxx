/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/compression/ZStdDictionary.hxx"

#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Gsl.hxx"
#include "erp/util/TLog.hxx"

#include <zstd.h>
#include <zdict.h>

ZStdDictionary::ZStdDictionary(ZStdDictionary::CDictPtr compressDictionary,
                               ZStdDictionary::DDictPtr decompressDictionary,
                               ZStdDictionary::IdType id)
    : mCompressDictionary(std::move(compressDictionary))
    , mDecompressDictionary(std::move(decompressDictionary))
    , mId(id)
{
    Expect3(mId.isValid(), "Invalid Dictionary.", std::logic_error);
    Expect3(mCompressDictionary, "Failed to create compression dictionary.", std::logic_error);
    Expect3(mDecompressDictionary, "Failed to create de-compression Dictionary.", std::logic_error);
}


ZStdDictionary ZStdDictionary::fromBin(const std::string_view& dictionaryData)
{
    auto dictionaryId = ZSTD_getDictID_fromDict(dictionaryData.data(), dictionaryData.size());
    Expect3(dictionaryId > 0, "Invalid Dictionary.", std::logic_error);
    auto id = IdType{gsl::narrow<IdType::Numeric>(dictionaryId)};
    CDictPtr compressDictionary{ZSTD_createCDict(dictionaryData.data(), dictionaryData.size(), 0)};
    DDictPtr decompressDictionary{ZSTD_createDDict(dictionaryData.data(), dictionaryData.size())};
    return {std::move(compressDictionary), std::move(decompressDictionary), id};
}


ZStdDictionary::IdType ZStdDictionary::id() const
{
    return mId;
}

const ZSTD_CDict& ZStdDictionary::getCompressDictionary() const
{
    return *mCompressDictionary;
}

const ZSTD_DDict& ZStdDictionary::getDecompressDictionary() const
{
    return *mDecompressDictionary;
}

ZStdDictionary::IdType::IdType(Numeric numericId)
    : mId(numericId)
{
}

ZStdDictionary::IdType::IdType(Compression::DictionaryUse dictUse, uint8_t dictVersion)
    : mId(uint8_t(dictUse) | uint8_t(dictVersion << 4))
{
    Expect3(uint8_t(dictUse) < 16, "Numeric value for dictionary-use out of range: " + std::to_string(uintmax_t(dictUse)), std::logic_error);
    Expect3(dictVersion < 16, "Numeric value for dictionary version out of range: " + std::to_string(dictVersion), std::logic_error);
}


ZStdDictionary::IdType ZStdDictionary::IdType::fromFrame(const std::string_view& frame)
{
    return IdType{gsl::narrow<Numeric>((ZSTD_getDictID_fromFrame(frame.data(), frame.size())))};
}

Compression::DictionaryUse ZStdDictionary::IdType::use() const
{
    return static_cast<Compression::DictionaryUse>(mId & 0x0F);
}

uint8_t ZStdDictionary::IdType::version() const
{
    return (mId >> 4) & 0x0F;
}

bool ZStdDictionary::IdType::operator < (ZStdDictionary::IdType other) const
{
    return mId < other.mId;
}

bool ZStdDictionary::IdType::operator!=(ZStdDictionary::IdType other) const
{
    return mId != other.mId;
}

bool ZStdDictionary::IdType::isValid()const
{
    return mId != 0;
}

std::ostream& operator << (std::ostream& out, ZStdDictionary::IdType id)
{
    out << R"({ "use": ")" << id.use() << R"(", "version": )" << uintmax_t(id.version()) <<  R"(})";
    return out;
}

std::string to_string(ZStdDictionary::IdType id)
{
    std::ostringstream out;
    out << id;
    return out.str();
}


