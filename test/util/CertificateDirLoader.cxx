/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/CertificateDirLoader.h"

#include "erp/crypto/Certificate.hxx"
#include "erp/util/FileHelper.hxx"
#include "ResourceManager.hxx"

std::list<Certificate> CertificateDirLoader::loadDir(const std::filesystem::path& dir)
{
    std::list<Certificate> result;
    std::filesystem::directory_iterator dirIter{dir.is_absolute()?dir:ResourceManager::getAbsoluteFilename(dir)};
    ResourceManager& resMgr = ResourceManager::instance();
    for (const auto& dir: dirIter)
    {
        if (dir.is_regular_file())
        {
            result.emplace_back(Certificate::fromPem(resMgr.getStringResource(dir.path().string())));
        }
    }
    return result;
}
