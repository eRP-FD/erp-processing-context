/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "tools/ResourceManager.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/util/Uuid.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

namespace {

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

void checkCommon(model::ChargeItem& chargeItem)
{
    EXPECT_EQ(chargeItem.id().toString(), "160.123.456.789.123.58");
    EXPECT_EQ(chargeItem.subjectKvnr(), "X234567890");
    EXPECT_EQ(chargeItem.entererTelematikId(), "606358757");
    EXPECT_EQ(chargeItem.enteredDate().toXsDateTimeWithoutFractionalSeconds(), "2021-06-01T02:13:00+00:00");

    const auto receiptRef = chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt);
    EXPECT_TRUE(receiptRef.has_value());
    EXPECT_EQ(receiptRef.value(), "160.123.456.789.123.58");
    const auto dispenseRef = chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem);
    EXPECT_TRUE(dispenseRef.has_value());
    EXPECT_EQ(dispenseRef.value(), "72bd741c-7ad8-41d8-97c3-9aabbdd0f5b4");
    const auto prescriptionRef = chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem);
    EXPECT_TRUE(prescriptionRef.has_value());
    EXPECT_EQ(prescriptionRef.value(), "0428d416-149e-48a4-977c-394887b3d85c");

    const model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.000.004.715.74");
    chargeItem.setId(prescriptionId);
    EXPECT_EQ(chargeItem.id().toString(), prescriptionId.toString());

    const std::string_view kvnr = "X424242424";
    chargeItem.setSubjectKvnr(kvnr);
    EXPECT_EQ(chargeItem.subjectKvnr(), kvnr);

    const std::string_view telematicId = "1234567";
    chargeItem.setEntererTelematikId(telematicId);
    EXPECT_EQ(chargeItem.entererTelematikId(), telematicId);

    const model::Timestamp enteredDate = model::Timestamp::now();
    chargeItem.setEnteredDate(enteredDate);
    EXPECT_EQ(chargeItem.enteredDate(), enteredDate);

    EXPECT_NO_FATAL_FAILURE(checkSetSupportingInfoReferences(chargeItem, prescriptionId));

    EXPECT_NO_THROW(chargeItem.deleteSupportingInfoElement(model::ChargeItem::SupportingInfoType::receipt));
    EXPECT_FALSE(chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt).has_value());

    EXPECT_NO_THROW(chargeItem.deleteSupportingInfoElement(model::ChargeItem::SupportingInfoType::dispenseItem));
    EXPECT_FALSE(chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem).has_value());

    EXPECT_NO_THROW(chargeItem.deleteSupportingInfoElement(model::ChargeItem::SupportingInfoType::prescriptionItem));
    EXPECT_FALSE(chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem).has_value());

    EXPECT_NO_FATAL_FAILURE(checkSetSupportingInfoReferences(chargeItem, prescriptionId));
}


} // anonymous namespace

TEST(ChargeItemTest, Construct)
{
    std::optional<model::ChargeItem> optChargeItem;
    ASSERT_NO_THROW(optChargeItem = model::ChargeItem::fromXml(
        ResourceManager::instance().getStringResource("test/EndpointHandlerTest/charge_item_input.xml"),
        *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(), SchemaType::fhir)); // TODO change to correct schema as soon as available

    EXPECT_NO_FATAL_FAILURE(checkCommon(optChargeItem.value()));

    EXPECT_FALSE(optChargeItem.value().isMarked());
}

TEST(ChargeItemTest, ConstructMarked)
{
    std::optional<model::ChargeItem> optChargeItem;
    ASSERT_NO_THROW(optChargeItem = model::ChargeItem::fromJson(
        ResourceManager::instance().getStringResource("test/EndpointHandlerTest/charge_item_input_marked.json"),
        *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
        *StaticData::getInCodeValidator(), SchemaType::fhir)); // TODO change to correct schema as soon as available

    EXPECT_NO_FATAL_FAILURE(checkCommon(optChargeItem.value()));

    EXPECT_TRUE(optChargeItem.value().isMarked());
}

