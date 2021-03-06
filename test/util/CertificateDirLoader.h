/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_CERTIFICATEDIRLOADER_H
#define ERP_PROCESSING_CONTEXT_TEST_CERTIFICATEDIRLOADER_H

#include <filesystem>
#include <list>

class Certificate;

class CertificateDirLoader
{
public:
    static std::list<Certificate> loadDir( const std::filesystem::path& dir);
};

#endif // ERP_PROCESSING_CONTEXT_TEST_CERTIFICATEDIRLOADER_H
