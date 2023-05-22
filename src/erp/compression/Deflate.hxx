/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DEFLATE_HXX
#define ERP_PROCESSING_CONTEXT_DEFLATE_HXX

#include "erp/ErpConstants.hxx"
#include "erp/compression/Compression.hxx"
#include "erp/util/Configuration.hxx"

#include <filesystem>
#include <string>
#include <string_view>

class Deflate final : public Compression
{
public:
    static constexpr std::size_t bufferSize = 2048;

    Deflate();
    ~Deflate() override;
    std::string compress(std::string_view plain, Compression::DictionaryUse dict) const override;
    std::string decompress(std::string_view compressed) const override;

private:
};

#endif// ERP_PROCESSING_CONTEXT_DEFLATE_HXX
