/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIR_HXX
#define ERP_PROCESSING_CONTEXT_FHIR_HXX

#include "erp/fhir/FhirConverter.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "fhirtools/repository/FhirKbvKeyTablesView.hxx"
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
    std::shared_ptr<fhirtools::FhirStructureRepository>
    structureRepository(const model::Timestamp& referenceTimestamp,
                        model::ResourceVersion::FhirProfileBundleVersion version =
                            model::ResourceVersion::current<model::ResourceVersion::FhirProfileBundleVersion>()) const
    {
        Expect(mStructureRepository.contains(version),
               "version not configured: " + std::string{model::ResourceVersion::v_str(version)});
        return std::make_shared<fhirtools::FhirKbvKeyTablesView>(
            &mStructureRepository.at(version), Configuration::instance().fhirResourceViewConfiguration(),
            fhirtools::Date{referenceTimestamp.toGermanDate()});
    }

private:
    Fhir();
    void loadVersion(ConfigurationKey versionKey, ConfigurationKey structureKey);

    FhirConverter mConverter;
    std::unordered_map<model::ResourceVersion::FhirProfileBundleVersion, fhirtools::FhirStructureRepositoryBackend>
        mStructureRepository;
};


#endif// ERP_PROCESSING_CONTEXT_FHIR_HXX
