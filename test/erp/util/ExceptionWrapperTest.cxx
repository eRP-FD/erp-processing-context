/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/Expect.hxx"

#include <gtest/gtest.h>

TEST(ExceptionWrapperTest, wrapping)//NOLINT(readability-function-cognitive-complexity)
{
    EXPECT_THROW(throw ExceptionWrapper<std::runtime_error>::create(fileAndLine, "test"), std::runtime_error);
    EXPECT_THROW(throw ExceptionWrapper<std::logic_error>::create(fileAndLine, "test"), std::logic_error);
    EXPECT_THROW(throw ExceptionWrapper<ErpException>::create(fileAndLine, "test", HttpStatus::OK), ErpException);
}

TEST(ExceptionWrapperTest, rootLocation)
{
    auto rootLocation = fileAndLine;
    try
    {
        throw ExceptionWrapper<std::runtime_error>::create(rootLocation, "test");
    }
    catch (...)
    {
        auto location = fileAndLine;
        ASSERT_NE(location, rootLocation);
        auto nested = ExceptionWrapper<std::runtime_error>::create(location, "test");
        EXPECT_EQ(nested.location.location, location);
        EXPECT_EQ(nested.location.rootLocation, rootLocation);
    }
}

TEST(ExceptionWrapperTest, Expect)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<FileNameAndLineNumber> rootLocation;
    std::optional<FileNameAndLineNumber> location;
    try
    {
        rootLocation.emplace(fileAndLine); Expect(false, "root cause");
    }
    catch (...)
    {
        try
        {
            location.emplace(fileAndLine); Fail("forwarded cause");
        }
        catch (const std::runtime_error& e)
        {
            const auto* wrapper = dynamic_cast<const ExceptionWrapperBase*>(&e);
            ASSERT_TRUE(wrapper);
            EXPECT_EQ(wrapper->location.location, location);
            EXPECT_EQ(wrapper->location.rootLocation, rootLocation);
        }
    }
}
