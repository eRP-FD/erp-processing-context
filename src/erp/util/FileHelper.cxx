/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/FileHelper.hxx"
#include "erp/util/Expect.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <filesystem>
#include <fstream>


bool FileHelper::exists(const std::filesystem::path& filePath)
{
    return std::filesystem::is_regular_file(filePath);
}


std::string FileHelper::readFileAsString(const std::filesystem::path& filePath)
{
    std::ifstream stream(filePath, std::ios::binary);
    if (!stream.good())
    {
        Fail("can not find or open for reading the file " + filePath.string());
    }
    std::vector<char> buffer(std::istreambuf_iterator<char>(stream), {});
    return std::string(buffer.data(), buffer.size());
}


void FileHelper::writeFile (const std::filesystem::path& filePath, const std::string& content)
{
    std::ofstream out (filePath, std::ios_base::out | std::ios_base::trunc);
    if ( ! out.good())
        Fail("could not open file '" + filePath.string() + "' for writing");

    out.write(content.data(), gsl::narrow<std::streamsize>(content.size()));
    out.close();
}


void FileHelper::deleteFile (const std::filesystem::path& filePath)
{
    if (remove(filePath) != 0)
        Fail("could not delete file '" + filePath.string() + "'");
}
