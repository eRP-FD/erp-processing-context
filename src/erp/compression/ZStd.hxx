#ifndef ERP_PROCESSING_CONTEXT_ZSTD_HXX
#define ERP_PROCESSING_CONTEXT_ZSTD_HXX

#include "erp/compression/Compression.hxx"
#include "erp/compression/ZStdDictionaryRepository.hxx"
#include "erp/ErpConstants.hxx"
#include "erp/util/Configuration.hxx"

#include <filesystem>
#include <set>

class ZStd final
    : public Compression
{
public:
    // Set an upper limit to ensure we don't allocate huge blocks when decompress is called on corrupt data
    static constexpr size_t maxPlainSize = ErpConstants::MaxBodySize * 2;

    explicit ZStd(const std::filesystem::path& dictionaryDir);

    ~ZStd() override;
    std::string compress(const std::string_view& plain, Compression::DictionaryUse dict) const override;
    std::string decompress(const std::string_view& compressed) const override;

private:
    static std::map<ZStdDictionary::IdType, std::string> requiredDictionarySHA256;

    ZStdDictionaryRepository mDictRepo;
    class Free;
};

#endif // ERP_PROCESSING_CONTEXT_ZSTD_HXX

