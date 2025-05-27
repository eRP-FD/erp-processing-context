/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"


using Erp8545TestCreate = ErpWorkflowTestTemplate<::testing::TestWithParam<std::string>>;


TEST_P(Erp8545TestCreate, run)
{
    const auto& [outerResponse, innerResponse] =
        send(RequestArguments{HttpMethod::POST, "/Task/$create", std::string{GetParam()}, ContentMimeType::fhirXmlUtf8}
                 .withJwt(jwtArzt()));
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
}


INSTANTIATE_TEST_SUITE_P(
    invalid, Erp8545TestCreate,
    ::testing::Values(
        R"--(<?xml version="1.0" encoding="utf-8"?><Parameters xmlns="http://hl7.org/fhir"/>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?><Parameters xmlns="http://hl7.org/fhir"><parameter/></Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="workflowType"/>
            </parameter>
        </Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="workflowType"/>
                <valueCoding/>
            </parameter>
        </Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="workflowType"/>
                <valueCoding>
                    <system value="https://gematik.de/fhir/CodeSystem/Flowtype"/>
                </valueCoding>
            </parameter>
        </Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="WRONG-FLOH-TYP"/>
                <valueCoding>
                    <system value="https://gematik.de/fhir/CodeSystem/Flowtype"/>
                    <code value="160"/>
                </valueCoding>
            </parameter>
        </Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="workflowType"/>
                <valueCoding>
                    <system value="https://gehmatik.de/fire/CodeSystem/Flowtype-WRONG"/>
                    <code value="160"/>
                </valueCoding>
            </parameter>
        </Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="workflowType"/>
                <valueCoding>
                    <system value="https://gematik.de/fhir/CodeSystem/Flowtype"/>
                    <code value="0815"/>
                </valueCoding>
            </parameter>
        </Parameters>)--"));


using Erp8545TestActivate = ErpWorkflowTestTemplate<::testing::TestWithParam<std::string>>;

TEST_P(Erp8545TestActivate, run)//NOLINT(readability-function-cognitive-complexity)
{
    static const std::regex prescriptionPlaceholder{"##PRESCRIPTION##"};
    std::string kvnr;
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    auto prescriptionId = task->prescriptionId();
    std::string accessCode{task->accessCode()};
    const auto& path = "/Task/" + prescriptionId.toString() + "/$activate?ac=" + accessCode;
    const auto& [bundle, _] = makeQESBundle(kvnr, prescriptionId, model::Timestamp::now());
    const auto body = std::regex_replace(GetParam(), prescriptionPlaceholder, bundle);
    const auto& [outerResponse, innerResponse] =
        send(RequestArguments{HttpMethod::POST, path, body, ContentMimeType::fhirXmlUtf8}
                 .withJwt(jwtArzt()).withOverrideExpectedKbvVersion("XXX"));
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
}

INSTANTIATE_TEST_SUITE_P(
    invalid, Erp8545TestActivate,
    ::testing::Values(
        R"--(<?xml version="1.0" encoding="utf-8"?><Parameters xmlns="http://hl7.org/fhir"/>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?><Parameters xmlns="http://hl7.org/fhir"><parameter/></Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="ePrescription"/>
            </parameter>
        </Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="ePrescription"/>
                <valueCoding/>
            </parameter>
        </Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="ePrescription"/>
                <resource>
                    <Binary/>
                </resource>
            </parameter>
        </Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="ePrescription"/>
                <resource>
                    <Binary>
                        <contentType value="application/pkcs7-mime"/>
                    </Binary>
                </resource>
            </parameter>
        </Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="ePrescription"/>
                <resource>
                    <Binary>
                        <data value="##PRESCRIPTION##"/>
                    </Binary>
                </resource>
            </parameter>
        </Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="ePrescription"/>
                <resource>
                    <Binary>
                        <contentType value="application/pkcs7-mime"/>
                        <data value="This is not Base 64!"/>
                    </Binary>
                </resource>
            </parameter>
        </Parameters>)--",
        R"--(<?xml version="1.0" encoding="utf-8"?>
         <Parameters xmlns="http://hl7.org/fhir">
            <parameter>
                <name value="iPrescription-WRONG"/>
                <resource>
                    <Binary>
                        <contentType value="application/pkcs7-mime"/>
                        <data value="##PRESCRIPTION##"/>
                    </Binary>
                </resource>
            </parameter>
        </Parameters>)--"));
