/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/PrescriptionType.hxx"

#include <gtest/gtest.h>
#include <magic_enum/magic_enum.hpp>

using namespace model;

class PrescriptionTypeTest : public ::testing::TestWithParam<PrescriptionType>
{
protected:
};

TEST_P(PrescriptionTypeTest, isPkv)
{
    PrescriptionType prescriptionType = GetParam();
    if (prescriptionType == PrescriptionType::apothekenpflichtigeArzneimittelPkv ||
        prescriptionType == PrescriptionType::direkteZuweisungPkv)
    {
        EXPECT_TRUE(isPkv(prescriptionType));
    }
    else
    {
        EXPECT_FALSE(isPkv(prescriptionType));
    }
}

TEST(PrescriptionTypeTest, isPkv_badValueThrows)
{
    PrescriptionType prescriptionType = static_cast<PrescriptionType>(0);
    EXPECT_THROW(isPkv(prescriptionType), std::exception);
}

TEST_P(PrescriptionTypeTest, isDiga)
{
    PrescriptionType prescriptionType = GetParam();
    if (prescriptionType == PrescriptionType::digitaleGesundheitsanwendungen)
    {
        EXPECT_TRUE(isDiga(prescriptionType));
    }
    else
    {
        EXPECT_FALSE(isDiga(prescriptionType));
    }
}

TEST(PrescriptionTypeTest, isDiga_badValueThrows)
{
    PrescriptionType prescriptionType = static_cast<PrescriptionType>(0);
    EXPECT_THROW(isDiga(prescriptionType), std::exception);
}

TEST_P(PrescriptionTypeTest, isDirectAssignment)
{
    PrescriptionType prescriptionType = GetParam();
    if (prescriptionType == PrescriptionType::direkteZuweisung ||
        prescriptionType == PrescriptionType::direkteZuweisungPkv)
    {
        EXPECT_TRUE(isDirectAssignment(prescriptionType));
    }
    else
    {
        EXPECT_FALSE(isDirectAssignment(prescriptionType));
    }
}

TEST(PrescriptionTypeTest, isDirectAssignment_badValueThrows)
{
    PrescriptionType prescriptionType = static_cast<PrescriptionType>(0);
    EXPECT_THROW(isDirectAssignment(prescriptionType), std::exception);
}

INSTANTIATE_TEST_CASE_P(PrescriptionType, PrescriptionTypeTest,
                        ::testing::ValuesIn(magic_enum::enum_values<model::PrescriptionType>()));
