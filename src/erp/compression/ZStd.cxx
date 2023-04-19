/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/compression/ZStd.hxx"

#include "erp/compression/ZStdDictionary.hxx"
#include "erp/crypto/Sha256.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "erp/util/TLog.hxx"

#include <filesystem>
#include <magic_enum.hpp>
#include <memory>
#include <zstd.h>


using namespace std::string_literals;

// it is vital that we always use the correct dictionaries, otherwise we will not be able to restore the original data
std::map<ZStdDictionary::IdType, std::string> ZStd::requiredDictionarySHA256 = {
    {{Compression::DictionaryUse::Default_json, 0}, "e19d51b38713b5e11cc26b90fe46fbee589837a22183de20277ce9339726a700"},
    {{Compression::DictionaryUse::Default_xml, 0} , "78dbfbeb190b02a5bb8306635bf66d8bf4b25f92730ad6c16288179b06e6b413"},
};


class ZStd::Free
{
public:
    void operator () (ZSTD_DCtx* context) const { ZSTD_freeDCtx(context); }
    void operator () (ZSTD_CCtx* context) const { ZSTD_freeCCtx(context); }
};

ZStd::ZStd(const std::filesystem::path& dictionaryDir)
{
    auto toLoad = requiredDictionarySHA256;
    std::filesystem::directory_iterator dictionaryDirIter{dictionaryDir};
    for (const auto& dirEntry: dictionaryDirIter)
    {
        if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".zdict")
        {
            std::string dictionaryData = FileHelper::readFileAsString(dirEntry);
            auto loadedSha256 = Sha256::fromBin(dictionaryData);
            try {
                auto dictionary = std::make_unique<ZStdDictionary>(ZStdDictionary::fromBin(dictionaryData));
                auto expectedSHA256 = toLoad.find(dictionary->id());
                Expect3(expectedSHA256 != toLoad.end(), "Got unexpected dictionary: " + to_string(dictionary->id()),
                        std::logic_error);
                Expect3(expectedSHA256->second == loadedSha256, "Sha256 mismatch for dictionary: " + to_string(dictionary->id()), std::logic_error);
                TVLOG(1) << "Loaded ZStd dictionary: " << dictionary->id();
                mDictRepo.addDictionary(std::move(dictionary));
                toLoad.erase(expectedSHA256);
            }
            catch (const std::logic_error& e)
            {
                std::throw_with_nested(std::logic_error(dirEntry.path().string() + ": " + e.what()));
            }
        }
    }
    if (! toLoad.empty())
    {
        for (const auto& missing: toLoad)
        {
            TLOG(ERROR) << "Dictionary not loaded: " << missing.first;
        }
        Fail("Not all necessary dictionaries found in: "s.append(dictionaryDir.string()));
    }
}

ZStd::~ZStd() = default;


std::string ZStd::compress(std::string_view plain, Compression::DictionaryUse dictUse) const
{
    const auto& dict = mDictRepo.getDictionaryForUse(dictUse);
    std::unique_ptr<ZSTD_CCtx, Free> context{ZSTD_createCCtx()};
    std::string compressed(gsl::narrow<size_t>(ZSTD_compressBound(plain.size())), '\0');
    auto compressedSize = ZSTD_compress_usingCDict(context.get(),compressed.data(),compressed.size(), plain.data(), plain.size(), &dict.getCompressDictionary());
    Expect3(compressedSize <= compressed.size(), "Compressed Size larger than output buffer.", std::logic_error);
    compressed.resize(compressedSize);
    return compressed;
}

std::string ZStd::decompress(std::string_view compressed) const
{
    using namespace std::string_literals;
    using std::to_string;
    auto dictId = ZStdDictionary::IdType::fromFrame(compressed);
    Expect3(dictId.isValid(), "Got invalid dictionary ID from compressed data.", std::logic_error);
    const auto& dict = mDictRepo.getDictionary(dictId);
    TVLOG(1) << "Extracting with dictionary: " << dictId;
    std::unique_ptr<ZSTD_DCtx, Free> context{ZSTD_createDCtx()};
    size_t plainSize = ZSTD_getFrameContentSize(compressed.data(), compressed.size());
    Expect3(plainSize !=  ZSTD_CONTENTSIZE_UNKNOWN, "Uncompressed size is unknown.", std::logic_error);
    Expect3(plainSize !=  ZSTD_CONTENTSIZE_ERROR, "Could not determine uncompressed size.", std::logic_error);
    Expect3(plainSize <= maxPlainSize, "Uncompressed size too large.", std::logic_error);
    std::string plainText(plainSize, '\0');
    auto result = ZSTD_decompress_usingDDict(context.get(),
                                             plainText.data(), plainText.size(),
                                             compressed.data(), compressed.size(),
                                             &dict.getDecompressDictionary());
    Expect3(result == plainSize,
            "Size from Frame-Header doesn't match decompressed size: " + to_string(plainSize) + " != " + to_string(result),
            std::logic_error);
    return plainText;
}
