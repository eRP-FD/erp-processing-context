/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/util/ResourceManager.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/util/Uuid.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

using namespace ::std::literals;
using namespace ::std::string_view_literals;

namespace
{

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkSetSupportingInfoReferences(model::ChargeItem& chargeItem, const model::PrescriptionId& prescriptionId)
{
    EXPECT_NO_THROW(chargeItem.setSupportingInfoReference(
            model::ChargeItem::SupportingInfoType::receipt, prescriptionId.toString()));
    EXPECT_EQ(chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt).value(),
              prescriptionId.toString());

    const Uuid dispenseItemRef;
    EXPECT_NO_THROW(chargeItem.setSupportingInfoReference(
            model::ChargeItem::SupportingInfoType::dispenseItem, dispenseItemRef.toString()));
    EXPECT_EQ(chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem).value(),
              dispenseItemRef.toString());

    const Uuid prescriptionItemRef;
    EXPECT_NO_THROW(chargeItem.setSupportingInfoReference(
            model::ChargeItem::SupportingInfoType::prescriptionItem, prescriptionItemRef.toString()));
    EXPECT_EQ(chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem).value(),
              prescriptionItemRef.toString());
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkCommon(model::ChargeItem& chargeItem)
{
    ASSERT_TRUE(chargeItem.prescriptionId());
    EXPECT_EQ(chargeItem.prescriptionId()->toString(), "160.123.456.789.123.58");
    EXPECT_EQ(chargeItem.subjectKvnr(), "X234567890");
    EXPECT_EQ(chargeItem.entererTelematikId(), "606358757");
    ASSERT_TRUE(chargeItem.enteredDate());
    EXPECT_EQ(chargeItem.enteredDate()->toXsDateTimeWithoutFractionalSeconds(), "2021-06-01T02:13:00+00:00");

    const auto receiptRef = chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt);
    ASSERT_TRUE(receiptRef.has_value());
    EXPECT_EQ(receiptRef.value(), "160.123.456.789.123.58");
    const auto dispenseRef = chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem);
    ASSERT_TRUE(dispenseRef.has_value());
    EXPECT_EQ(dispenseRef.value(), "72bd741c-7ad8-41d8-97c3-9aabbdd0f5b4");
    const auto prescriptionRef = chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem);
    ASSERT_TRUE(prescriptionRef.has_value());
    EXPECT_EQ(prescriptionRef.value(), "0428d416-149e-48a4-977c-394887b3d85c");

    const model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.000.004.715.74");
    chargeItem.setId(prescriptionId);
    ASSERT_TRUE(chargeItem.id());
    EXPECT_EQ(chargeItem.id()->toString(), prescriptionId.toString());
    chargeItem.setPrescriptionId(prescriptionId);
    ASSERT_TRUE(chargeItem.prescriptionId());
    EXPECT_EQ(chargeItem.prescriptionId()->toString(), prescriptionId.toString());

    const std::string_view kvnr = "X424242424";
    chargeItem.setSubjectKvnr(kvnr);
    EXPECT_EQ(chargeItem.subjectKvnr(), kvnr);

    const std::string_view telematicId = "1234567";
    chargeItem.setEntererTelematikId(telematicId);
    EXPECT_EQ(chargeItem.entererTelematikId(), telematicId);

    const fhirtools::Timestamp enteredDate = fhirtools::Timestamp::now();
    chargeItem.setEnteredDate(enteredDate);
    EXPECT_EQ(chargeItem.enteredDate(), enteredDate);

    const auto containedBinary = chargeItem.containedBinary();
    ASSERT_TRUE(containedBinary.has_value());
    EXPECT_EQ(containedBinary->data(), "UEtWIGRpc3BlbnNlIGl0ZW0gYnVuZGxl");

    EXPECT_NO_FATAL_FAILURE(checkSetSupportingInfoReferences(chargeItem, prescriptionId));

    EXPECT_NO_THROW(chargeItem.deleteSupportingInfoReference(model::ChargeItem::SupportingInfoType::receipt));
    EXPECT_FALSE(chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt).has_value());

    EXPECT_NO_THROW(chargeItem.deleteSupportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem));
    EXPECT_FALSE(chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem).has_value());

    EXPECT_NO_THROW(chargeItem.deleteSupportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem));
    EXPECT_FALSE(
        chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem).has_value());

    EXPECT_NO_FATAL_FAILURE(checkSetSupportingInfoReferences(chargeItem, prescriptionId));
}


} // anonymous namespace

TEST(ChargeItemTest, ConstructFromIndividualData)//NOLINT(readability-function-cognitive-complexity)
{
    ::model::ChargeItem chargeItem;

    EXPECT_FALSE(chargeItem.isMarked());

    ::std::optional<::model::ChargeItem> sourceChargeItem;
    ASSERT_NO_THROW(
        sourceChargeItem = ::model::ChargeItem::fromXml(
            ::ResourceManager::instance().getStringResource("test/EndpointHandlerTest/charge_item_input.xml"),
            *::StaticData::getXmlValidator(), *::StaticData::getInCodeValidator(), ::SchemaType::Gem_erxChargeItem));
    ASSERT_TRUE(sourceChargeItem.has_value());

    EXPECT_FALSE(chargeItem.prescriptionId());
    ASSERT_TRUE(sourceChargeItem->prescriptionId());
    chargeItem.setPrescriptionId(sourceChargeItem->prescriptionId().value());
    ASSERT_TRUE(chargeItem.prescriptionId());
    EXPECT_EQ(chargeItem.prescriptionId(), sourceChargeItem->prescriptionId());

    EXPECT_FALSE(chargeItem.subjectKvnr());
    ASSERT_TRUE(sourceChargeItem->subjectKvnr());
    chargeItem.setSubjectKvnr(sourceChargeItem->subjectKvnr().value());
    ASSERT_TRUE(chargeItem.subjectKvnr());
    EXPECT_EQ(chargeItem.subjectKvnr(), sourceChargeItem->subjectKvnr());

    EXPECT_FALSE(chargeItem.entererTelematikId());
    ASSERT_TRUE(sourceChargeItem->entererTelematikId());
    chargeItem.setEntererTelematikId(sourceChargeItem->entererTelematikId().value());
    ASSERT_TRUE(chargeItem.entererTelematikId());
    EXPECT_EQ(chargeItem.entererTelematikId(), sourceChargeItem->entererTelematikId());

    EXPECT_FALSE(chargeItem.enteredDate());
    ASSERT_TRUE(sourceChargeItem->enteredDate());
    chargeItem.setEnteredDate(sourceChargeItem->enteredDate().value());
    ASSERT_TRUE(chargeItem.enteredDate());
    EXPECT_EQ(chargeItem.enteredDate(), sourceChargeItem->enteredDate());

    const auto sourceMarkingFlag = sourceChargeItem->markingFlag();
    if (sourceMarkingFlag)
    {
        chargeItem.setMarkingFlag(sourceMarkingFlag.value());
    }

    chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItem,
                                          "0428d416-149e-48a4-977c-394887b3d85c"sv);
    chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem,
                                          "72bd741c-7ad8-41d8-97c3-9aabbdd0f5b4"sv);
    chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt, "160.123.456.789.123.58"sv);

    // The enteredDate in the selected source may be converted to a different time zone when setting it above.
    sourceChargeItem->setEnteredDate(sourceChargeItem->enteredDate().value());

    sourceChargeItem->deleteContainedBinary();

    EXPECT_EQ(chargeItem.serializeToCanonicalJsonString(), sourceChargeItem->serializeToCanonicalJsonString());
}

TEST(ChargeItemTest, ConstructFromXml)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::ChargeItem> optChargeItem;
    ASSERT_NO_THROW(optChargeItem = model::ChargeItem::fromXml(
        ResourceManager::instance().getStringResource("test/EndpointHandlerTest/charge_item_input.xml"),
        *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(), SchemaType::Gem_erxChargeItem));

    EXPECT_NO_FATAL_FAILURE(checkCommon(optChargeItem.value()));

    EXPECT_FALSE(optChargeItem.value().isMarked());
}

TEST(ChargeItemTest, ConstructMarked)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::ChargeItem> optChargeItem;
    ASSERT_NO_THROW(optChargeItem = model::ChargeItem::fromJson(
        ResourceManager::instance().getStringResource("test/EndpointHandlerTest/charge_item_input_marked.json"),
        *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
        *StaticData::getInCodeValidator(), SchemaType::Gem_erxChargeItem));

    EXPECT_NO_FATAL_FAILURE(checkCommon(optChargeItem.value()));

    EXPECT_TRUE(optChargeItem->isMarked());
    auto allMarkings = optChargeItem->markingFlag()->getAllMarkings();
    EXPECT_TRUE(allMarkings.at("taxOffice"));
    EXPECT_FALSE(allMarkings.at("subsidy"));
    EXPECT_FALSE(allMarkings.at("insuranceProvider"));

    std::optional<model::ChargeItem> optChargeItemNoMarking;
    ASSERT_NO_THROW(optChargeItemNoMarking = model::ChargeItem::fromXmlNoValidation(
        ResourceManager::instance().getStringResource("test/EndpointHandlerTest/charge_item_input.xml")));

    ASSERT_TRUE(optChargeItemNoMarking->markingFlag().has_value());
    optChargeItem->setMarkingFlag(optChargeItemNoMarking->markingFlag().value());

    EXPECT_FALSE(optChargeItem->isMarked());
    allMarkings = optChargeItem->markingFlag()->getAllMarkings();
    EXPECT_FALSE(allMarkings.at("taxOffice"));
    EXPECT_FALSE(allMarkings.at("subsidy"));
    EXPECT_FALSE(allMarkings.at("insuranceProvider"));

    ASSERT_NO_THROW(optChargeItem->deleteMarkingFlag());
    EXPECT_FALSE(optChargeItem.value().isMarked());
    allMarkings = optChargeItem->markingFlag()->getAllMarkings();
    EXPECT_TRUE(allMarkings.empty());
}

TEST(ChargeItemTest, ContainedBinary)//NOLINT(readability-function-cognitive-complexity)
{
    ::std::optional<::model::ChargeItem> chargeItem;
    ASSERT_NO_THROW(
        chargeItem = ::model::ChargeItem::fromXml(
            ::ResourceManager::instance().getStringResource("test/EndpointHandlerTest/charge_item_input.xml"),
            *::StaticData::getXmlValidator(), *::StaticData::getInCodeValidator(), ::SchemaType::Gem_erxChargeItem));
    ASSERT_TRUE(chargeItem.has_value());

    ::std::optional<::model::Binary> containedBinary;
    ASSERT_NO_THROW(containedBinary = chargeItem->containedBinary());
    EXPECT_TRUE(containedBinary.has_value());
    EXPECT_EQ(containedBinary->data().value(), ::std::string{"UEtWIGRpc3BlbnNlIGl0ZW0gYnVuZGxl"});

    chargeItem->deleteContainedBinary();
    EXPECT_FALSE(chargeItem->containedBinary().has_value());
}

TEST(ChargeItemTest, MarkingFlags)//NOLINT(readability-function-cognitive-complexity)
{
    ::model::ChargeItem chargeItem;

    EXPECT_FALSE(chargeItem.isMarked());

    for (const auto& chargeItemJson : {"test/EndpointHandlerTest/charge_item_input_marked.json",
                                       "test/EndpointHandlerTest/charge_item_input_marked_all.json"})
    {

        ::std::optional<::model::ChargeItem> sourceChargeItem;
        ASSERT_NO_THROW(sourceChargeItem = ::model::ChargeItem::fromJson(
                            ::ResourceManager::instance().getStringResource(chargeItemJson),
                            *::StaticData::getJsonValidator(), *::StaticData::getXmlValidator(),
                            *::StaticData::getInCodeValidator(), ::SchemaType::Gem_erxChargeItem))
            << chargeItemJson;
        ASSERT_TRUE(sourceChargeItem.has_value());

        const auto sourceMarkingFlag = sourceChargeItem->markingFlag();
        ASSERT_TRUE(sourceMarkingFlag.has_value());
        chargeItem.setMarkingFlag(sourceMarkingFlag.value());

        EXPECT_TRUE(chargeItem.isMarked());
        auto targetMarkingFlag = chargeItem.markingFlag();
        EXPECT_EQ(targetMarkingFlag->getAllMarkings().at("taxOffice"),
                  targetMarkingFlag->getAllMarkings().at("taxOffice"));
        EXPECT_EQ(targetMarkingFlag->getAllMarkings().at("subsidy"), targetMarkingFlag->getAllMarkings().at("subsidy"));
        EXPECT_EQ(targetMarkingFlag->getAllMarkings().at("insuranceProvider"),
                  targetMarkingFlag->getAllMarkings().at("insuranceProvider"));
    }
}

TEST(ChargeItemTest, SupportingInfoReference)//NOLINT(readability-function-cognitive-complexity)
{
    ::model::ChargeItem chargeItem;

    const auto prescriptionInput = "0428d416-149e-48a4-977c-394887b3d85c"sv;
    chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItem, prescriptionInput);
    auto prescriptionReference =
        chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItem);
    ASSERT_TRUE(prescriptionReference.has_value());
    EXPECT_EQ(prescriptionReference.value(), prescriptionInput);

    auto dispenseInput = "72bd741c-7ad8-41d8-97c3-9aabbdd0f5b4"s;
    chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem, dispenseInput);
    auto dispenseReference = chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem);
    ASSERT_TRUE(dispenseReference.has_value());
    EXPECT_EQ(dispenseReference.value(), dispenseInput);

    const auto receiptInput = "160.123.456.789.123.58"sv;
    chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt, receiptInput);
    auto receiptReference = chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt);
    ASSERT_TRUE(receiptReference.has_value());
    EXPECT_EQ(receiptReference.value(), receiptInput);

    const auto& dispenseItemXML =
        ::ResourceManager::instance().getStringResource("test/EndpointHandlerTest/dispense_item.xml");
    auto dispenseBundle = ::model::Bundle::fromXmlNoValidation(dispenseItemXML);
    dispenseInput = "Bundle/"s + dispenseBundle.getId().toString();
    chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem, dispenseBundle);
    dispenseReference = chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem);
    ASSERT_TRUE(dispenseReference.has_value());
    EXPECT_EQ(dispenseReference.value(), dispenseInput);
    prescriptionReference =
        chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItem);
    EXPECT_TRUE(prescriptionReference.has_value());
    EXPECT_EQ(prescriptionReference.value(), prescriptionInput);
    receiptReference = chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt);
    EXPECT_TRUE(receiptReference.has_value());
    EXPECT_EQ(receiptReference.value(), receiptInput);

    chargeItem.deleteSupportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItem);
    EXPECT_FALSE(
        chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItem).has_value());
    dispenseReference = chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem);
    EXPECT_TRUE(dispenseReference.has_value());
    EXPECT_EQ(dispenseReference.value(), dispenseInput);
    receiptReference = chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt);
    EXPECT_TRUE(receiptReference.has_value());
    EXPECT_EQ(receiptReference.value(), receiptInput);

    chargeItem.deleteSupportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem);
    EXPECT_FALSE(chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem).has_value());
    receiptReference = chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt);
    EXPECT_TRUE(receiptReference.has_value());
    EXPECT_EQ(receiptReference.value(), receiptInput);

    chargeItem.deleteSupportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt);
    EXPECT_FALSE(chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt).has_value());

    EXPECT_FALSE(
        chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItem).has_value());
    EXPECT_FALSE(chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem).has_value());
}

TEST(ChargeItemTest, OverwriteIdentifier)//NOLINT(readability-function-cognitive-complexity)
{
    ::model::ChargeItem chargeItem;

    ASSERT_FALSE(chargeItem.prescriptionId());

    const auto prescriptionId1 =
        ::model::PrescriptionId::fromDatabaseId(::model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 1);

    EXPECT_NO_THROW(chargeItem.setPrescriptionId(prescriptionId1));
    ASSERT_TRUE(chargeItem.prescriptionId());
    EXPECT_EQ(chargeItem.prescriptionId(), prescriptionId1);

    const auto prescriptionId2 =
        ::model::PrescriptionId::fromDatabaseId(::model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 2);

    EXPECT_NO_THROW(chargeItem.setPrescriptionId(prescriptionId2));
    ASSERT_TRUE(chargeItem.prescriptionId());
    EXPECT_EQ(chargeItem.prescriptionId(), prescriptionId2);

    const auto accessCode1 = "111111"sv;

    EXPECT_NO_THROW(chargeItem.setAccessCode(accessCode1));
    ASSERT_TRUE(chargeItem.accessCode());
    EXPECT_EQ(chargeItem.accessCode(), accessCode1);

    const auto accessCode2 = "222222"sv;

    EXPECT_NO_THROW(chargeItem.setAccessCode(accessCode2));
    ASSERT_TRUE(chargeItem.accessCode());
    EXPECT_EQ(chargeItem.accessCode(), accessCode2);
}

TEST(ChargeItemTest, AccessCode)//NOLINT(readability-function-cognitive-complexity)
{
    ::model::ChargeItem chargeItem;

    ASSERT_FALSE(chargeItem.prescriptionId());

    const auto prescriptionId1 =
        ::model::PrescriptionId::fromDatabaseId(::model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 1);

    EXPECT_NO_THROW(chargeItem.setPrescriptionId(prescriptionId1));
    ASSERT_TRUE(chargeItem.prescriptionId());
    EXPECT_EQ(chargeItem.prescriptionId(), prescriptionId1);

    const auto accessCode = "111111"sv;

    EXPECT_NO_THROW(chargeItem.setAccessCode(accessCode));
    ASSERT_TRUE(chargeItem.accessCode());
    EXPECT_EQ(chargeItem.accessCode(), accessCode);

    EXPECT_NO_THROW(chargeItem.deleteAccessCode());
    EXPECT_FALSE(chargeItem.accessCode());

    ASSERT_TRUE(chargeItem.prescriptionId());
    EXPECT_EQ(chargeItem.prescriptionId(), prescriptionId1);
}
