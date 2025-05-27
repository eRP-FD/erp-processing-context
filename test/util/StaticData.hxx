/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_STATICDATA_HXX
#define ERP_PROCESSING_CONTEXT_STATICDATA_HXX

#include "erp/pc/PcServiceContext.hxx"
#include "test/util/CommonStaticData.hxx"

#include <memory>


class StaticData : public CommonStaticData
{
public:
    static const Certificate idpCertificate;

    static Factories makeMockFactories();
    static Factories makeMockFactoriesWithServers();
    static PcServiceContext
    makePcServiceContext(std::optional<decltype(Factories::databaseFactory)> cutomDatabaseFactory = {});
};

#endif
