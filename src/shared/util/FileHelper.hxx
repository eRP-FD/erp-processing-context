/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_UTIL_FILEHELPER_HXX
#define E_LIBRARY_UTIL_FILEHELPER_HXX

#include <filesystem>

class FileHelper
{
public:
    /*
     * Check if the filePath is a real file and is readable
     */
    static bool exists(const std::filesystem::path& filePath);

    static std::string readFileAsString(const std::filesystem::path& filePath);

    static void writeFile (const std::filesystem::path& filePath, const std::string& content);

    static void deleteFile (const std::filesystem::path& filePath);
};


#endif
