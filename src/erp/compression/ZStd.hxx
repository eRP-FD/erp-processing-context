/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ZSTD_HXX
#define ERP_PROCESSING_CONTEXT_ZSTD_HXX

#include "erp/compression/Compression.hxx"
#include "erp/compression/ZStdDictionaryRepository.hxx"
#include "erp/util/Configuration.hxx"

#include <filesystem>
#include <set>

class ZStd final
    : public Compression
{
public:

    explicit ZStd(const std::filesystem::path& dictionaryDir);

    ~ZStd() override;
    std::string compress(std::string_view plain, Compression::DictionaryUse dict) const override;
    std::string decompress(std::string_view compressed) const override;

private:
    static std::map<ZStdDictionary::IdType, std::string> requiredDictionarySHA256;

    ZStdDictionaryRepository mDictRepo;
    class Free;
};

#endif // ERP_PROCESSING_CONTEXT_ZSTD_HXX
