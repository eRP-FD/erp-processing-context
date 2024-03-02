/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ZSTDDICTIONARY_HXX
#define ERP_PROCESSING_CONTEXT_ZSTDDICTIONARY_HXX

#include "erp/compression/Compression.hxx"

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <zstd.h>

class ZStdDictionary
{
public:
    class IdType
    {
    public:
        // currently we use only 8 bit to reduce the frame header size
        using Numeric = uint8_t;
        explicit IdType(Numeric numericId);
        IdType(Compression::DictionaryUse dictUse, uint8_t dictVersion);
        static IdType fromFrame(const std::string_view& frame);

        uint8_t version() const;
        Compression::DictionaryUse use() const;
        bool isValid() const;
        bool operator < (IdType) const;
        bool operator != (IdType) const;
    private:
        Numeric mId;
    };
    const ZSTD_CDict& getCompressDictionary() const;
    const ZSTD_DDict& getDecompressDictionary() const;

    static ZStdDictionary fromBin(const std::string_view&);

    IdType id() const;

private:
    class Free {
    public:
        void operator()(ZSTD_CDict* dict) { ZSTD_freeCDict(dict);}
        void operator()(ZSTD_DDict* dict) { ZSTD_freeDDict(dict);}
    };

    using CDictPtr = std::unique_ptr<ZSTD_CDict, Free>;
    using DDictPtr = std::unique_ptr<ZSTD_DDict, Free>;

    ZStdDictionary(
            CDictPtr mCompressDictionary,
            DDictPtr mDecompressDictionary,
            IdType mId
    );

    CDictPtr mCompressDictionary;
    DDictPtr mDecompressDictionary;
    IdType mId{0};
};

std::ostream& operator << (std::ostream&, ZStdDictionary::IdType);
std::string to_string(ZStdDictionary::IdType);


#endif// ERP_PROCESSING_CONTEXT_ZSTDDICTIONARY_HXX
