/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERFACTORIES_HXX
#define ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERFACTORIES_HXX

#include "exporter/database/MainDatabaseFrontend.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontendInterface.hxx"
#include "shared/hsm/BlobCache.hxx"
#include "shared/hsm/HsmFactory.hxx"
#include "shared/hsm/TeeTokenUpdater.hxx"
#include "shared/server/BaseServiceContext.hxx"

class MedicationExporterFactories : public BaseFactories
{
public:
    MedicationExporterDatabaseFrontendInterface::Factory exporterDatabaseFactory;
    exporter::MainDatabaseFrontend::Factory erpDatabaseFactory;
};

#endif//ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERFACTORIES_HXX
