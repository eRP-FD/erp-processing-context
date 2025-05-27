/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MEDICATION_DISPENSE_FIXTURE_HXX
#define ERP_PROCESSING_CONTEXT_MEDICATION_DISPENSE_FIXTURE_HXX

#include "shared/crypto/Jwt.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/TestUtils.hxx"

#include <date/date.h>
#include <gtest/gtest.h>
#include <regex>


class MedicationDispenseFixture : public EndpointHandlerTest
{
public:
    struct ProfileValidityTestParams;
    struct MaxWhenHandedOverTestParams;

    static constexpr char notSupported[] = "not supported";

    enum class ExpectedResult
    {
        success,
        failure,
        noCatch,
    };

    enum class EndpointType {
        close,
        dispense,
    };

    enum class BodyType {
        single,
        bundle,
        bundleNoProfile,
        parameters,
    };

    static std::list<ProfileValidityTestParams> profileValidityTestParameters();
    static std::list<MaxWhenHandedOverTestParams> maxWhenHandedOverTestParameters();

    //NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init) - force initialization at construction site.
    struct BodyOptions {
        EndpointType endpointType;
        BodyType bodyType;
        std::optional<ResourceTemplates::Versions::GEM_ERP> overrideVersion = std::nullopt;
        std::list<std::string> overrideWhenHandedOver = {};
    };

    std::string medicationDispenseBody(const BodyOptions& opt) const;



protected:
    [[nodiscard]] std::list<ResourceTemplates::MedicationDispenseOptions>
    getMultiMedicationDispenseOptions(const BodyOptions& opt) const;

    JWT jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    const std::string telematikId = jwtPharmacy.stringForClaim(JWT::idNumberClaim).value();
    const std::string lastValidDayOf_1_3 = "2025-04-15";
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                 .telematikId = telematikId};
private:
    std::string medicationDispenseXml(BodyOptions opt) const;
    std::string medicationDispenseBundleXml(BodyOptions opt) const;
    std::string medicationDispenseBundleXmlNoProfile(const BodyOptions& opt) const;
    std::string dispenseOperationInputParamers(const BodyOptions& opt) const;
    std::string closeOperationInputParamers(const BodyOptions& opt) const;



};

struct MedicationDispenseFixture::ProfileValidityTestParams
{
    ExpectedResult result;
    std::string date;
    MedicationDispenseFixture::BodyType bodyType;
    ResourceTemplates::Versions::GEM_ERP version;
};

std::ostream& operator << (std::ostream& out, const MedicationDispenseFixture::ProfileValidityTestParams& params);

struct MedicationDispenseFixture::MaxWhenHandedOverTestParams
{
    ExpectedResult result;
    std::list<std::string> whenHandedOver;
    MedicationDispenseFixture::BodyType bodyType;
    ResourceTemplates::Versions::GEM_ERP version;
};

std::ostream& operator << (std::ostream& out, const MedicationDispenseFixture::MaxWhenHandedOverTestParams& params);

#endif// ERP_PROCESSING_CONTEXT_MEDICATION_DISPENSE_FIXTURE_HXX
