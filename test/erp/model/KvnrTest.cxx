/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Kvnr.hxx"

#include <gtest/gtest.h>


TEST(KvnrTest, isKvnr)
{
    ASSERT_TRUE(model::Kvnr::isKvnr("X123456789"));
    ASSERT_TRUE(model::Kvnr::isKvnr("X234567890"));
    ASSERT_FALSE(model::Kvnr::isKvnr("XY34567890"));
    ASSERT_FALSE(model::Kvnr::isKvnr("X3456789"));
    ASSERT_FALSE(model::Kvnr::isKvnr("a123456789"));
}