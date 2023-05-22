/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIR_HXX
#define ERP_PROCESSING_CONTEXT_FHIR_HXX

#include "erp/fhir/FhirConverter.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/util/XmlHelper.hxx"

#include <string_view>

class FhirConverter;

class Fhir final
{
public:
    static const Fhir& instance();

    const FhirConverter& converter() const
    {
        return mConverter;
    }
    const fhirtools::FhirStructureRepository&
    structureRepository(model::ResourceVersion::FhirProfileBundleVersion version =
                            model::ResourceVersion::current<model::ResourceVersion::FhirProfileBundleVersion>()) const
    {
        Expect(mStructureRepository.contains(version),
               "version not configured: " + std::string{model::ResourceVersion::v_str(version)});
        return mStructureRepository.at(version);
    }

private:
    Fhir();
    void loadVersion(ConfigurationKey versionKey, ConfigurationKey structureKey);

    FhirConverter mConverter;
    std::unordered_map<model::ResourceVersion::FhirProfileBundleVersion, fhirtools::FhirStructureRepository>
        mStructureRepository;
};


#endif// ERP_PROCESSING_CONTEXT_FHIR_HXX
