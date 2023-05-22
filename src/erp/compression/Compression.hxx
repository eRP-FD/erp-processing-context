/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_COMPRESSION_HXX
#define ERP_PROCESSING_CONTEXT_COMPRESSION_HXX

#include "erp/ErpConstants.hxx"

#include <iosfwd>
#include <string>

class Compression
{
public:
    enum class DictionaryUse : uint8_t
    {
        Undefined = 0x00,
        Default_xml = 0x01,
        Default_json = 0x02,
        // NOTE: the maximum value can be 15 (0x0F)
        //       otherwise ZStdDictionary::IdType must be adapted
    };
    // Set an upper limit to ensure we don't allocate huge blocks when decompress is called on corrupt data
    static constexpr size_t maxPlainSize = ErpConstants::MaxBodySize * 2;

    virtual ~Compression() = default;

    /// @brief compress data
    /// @param plain plain text to compress
    /// @param dict initial dictionary to be used if supported by Compressor
    virtual std::string compress(std::string_view plain,
                                 DictionaryUse dict = DictionaryUse::Default_xml) const = 0;
    virtual std::string decompress(std::string_view compressed) const = 0;
};

std::ostream& operator << (std::ostream& out, Compression::DictionaryUse);

#endif// ERP_PROCESSING_CONTEXT_COMPRESSION_HXX
