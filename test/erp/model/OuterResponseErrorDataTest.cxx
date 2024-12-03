/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/OuterResponseErrorData.hxx"
#include "shared/model/Resource.hxx"

#include <gtest/gtest.h>


TEST(OuterResponseErrorDataTest, Construct)
{
    const std::string_view xRequestId = "5e2b25b5-866a-49b3-8ef1-4b0a010368cc";
    const auto status = HttpStatus::BadRequest;
    const std::string_view error = "ErpException";
    const std::string_view message = "Error message";
    model::OuterResponseErrorData errorData(xRequestId, status, error, message);

    EXPECT_EQ(errorData.xRequestId(), xRequestId);
    EXPECT_EQ(errorData.status(), status);
    EXPECT_EQ(errorData.error(), error);
    EXPECT_EQ(errorData.message(), message);
}
