/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef ERP_PROCESSING_CONTEXT_TEST_MEDICATIONEXPORTERSTATICDATA_HXX_INCLUDED
#define ERP_PROCESSING_CONTEXT_TEST_MEDICATIONEXPORTERSTATICDATA_HXX_INCLUDED

#include "test/util/CommonStaticData.hxx"

class MedicationExporterFactories;

class MedicationExporterStaticData : public CommonStaticData {
public:
    static MedicationExporterFactories makeMockMedicationExporterFactories();

};

#endif// ERP_PROCESSING_CONTEXT_TEST_MEDICATIONEXPORTERSTATICDATA_HXX_INCLUDED
