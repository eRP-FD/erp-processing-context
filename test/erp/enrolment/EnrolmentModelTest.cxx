/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <erp/enrolment/EnrolmentModel.hxx>
#include <gtest/gtest.h>
#include <regex>

static constexpr ::std::string_view json = R"({
  "first": "1",
  "second": "two",
  "third": {
    "sub-one": 3,
    "sub-two": "four"
  },
  "fourth": 4,
  "base64": "dGVzdDEyMzQ=",
  "vector": [ 1, 2, 3 ],
  "invalid-vector": [ 1, "text", 3 ],
  "erp": {
    "vector": "test1234",
    "base64-vector": "dGVzdDEyMzQ=",
    "invalid-vector": 5
  }
})";

TEST(EnrolmentModelTest, ConstructionAndSerialization)
{
    {
        EnrolmentModel model;
        EXPECT_EQ(model.serializeToString(), "");
    }

    {
        EnrolmentModel model{json};
        EXPECT_EQ(model.serializeToString(), ::std::regex_replace(::std::string{json}, ::std::regex{"[ \n]"}, ""));
    }
}

TEST(EnrolmentModelTest, HasValue)
{
    EnrolmentModel model{json};

    EXPECT_TRUE(model.hasValue("/first"));
    EXPECT_TRUE(model.hasValue("/third/sub-one"));
    EXPECT_FALSE(model.hasValue("/invalid"));
}

TEST(EnrolmentModelTest, GetString)//NOLINT(readability-function-cognitive-complexity)
{
    EnrolmentModel model{json};

    EXPECT_EQ(model.getString("/first"), "1");
    EXPECT_EQ(model.getString("/second"), "two");
    EXPECT_EQ(model.getString("/third/sub-two"), "four");

    EXPECT_THROW(model.getString("/fourth"), ErpException);
    EXPECT_THROW(model.getString("/invalid"), ErpException);
}

TEST(EnrolmentModelTest, GetSafeString)//NOLINT(readability-function-cognitive-complexity)
{
    EnrolmentModel model{json};

    EXPECT_EQ(model.getSafeString("/first"), ::SafeString{"1"});
    EXPECT_EQ(model.getSafeString("/second"), ::SafeString{"two"});
    EXPECT_EQ(model.getSafeString("/third/sub-two"), ::SafeString{"four"});

    EXPECT_THROW(model.getString("/fourth"), ErpException);
    EXPECT_THROW(model.getString("/invalid"), ErpException);
}

TEST(EnrolmentModelTest, GetInt64)//NOLINT(readability-function-cognitive-complexity)
{
    EnrolmentModel model{json};

    ASSERT_TRUE(model.getOptionalInt64("/fourth").has_value());
    EXPECT_EQ(model.getOptionalInt64("/fourth").value(), 4);
    EXPECT_EQ(model.getInt64("/fourth"), 4);

    ASSERT_TRUE(model.getOptionalInt64("/third/sub-one").has_value());
    EXPECT_EQ(model.getOptionalInt64("/third/sub-one").value(), 3);
    EXPECT_EQ(model.getInt64("/third/sub-one"), 3);

    EXPECT_THROW(model.getOptionalInt64("/third/sub-two"), ErpException);

    EXPECT_THROW(model.getInt64("/first"), ErpException);
    EXPECT_THROW(model.getInt64("/second"), ErpException);
    EXPECT_THROW(model.getInt64("/invalid"), ErpException);
}

TEST(EnrolmentModelTest, GetDecodedString)//NOLINT(readability-function-cognitive-complexity)
{
    EnrolmentModel model{json};

    ASSERT_TRUE(model.getOptionalDecodedString("/base64").has_value());
    EXPECT_EQ(model.getOptionalDecodedString("/base64").value(), "test1234");
    EXPECT_EQ(model.getDecodedString("/base64"), "test1234");

    EXPECT_THROW(model.getDecodedString("/fourth"), ErpException);
    EXPECT_THROW(model.getDecodedString("/invalid"), ErpException);
}

TEST(EnrolmentModelTest, GetSizeTVector)//NOLINT(readability-function-cognitive-complexity)
{
    EnrolmentModel model{json};

    ASSERT_TRUE(model.getOptionalSizeTVector("/vector").has_value());
    const auto vector = ::std::vector<::std::size_t>{1, 2, 3};
    EXPECT_EQ(model.getOptionalSizeTVector("/vector").value(), vector);
    EXPECT_EQ(model.getSizeTVector("/vector"), vector);

    EXPECT_THROW(model.getSizeTVector("/first"), ErpException);
    EXPECT_THROW(model.getSizeTVector("/invalid"), ErpException);
    EXPECT_THROW(model.getSizeTVector("/invalid-vector"), ErpException);
    ASSERT_THROW(model.getOptionalSizeTVector("/first"), ErpException);
}

TEST(EnrolmentModelTest, GetErpVector)//NOLINT(readability-function-cognitive-complexity)
{
    EnrolmentModel model{json};

    const auto vector = ::ErpVector::create("test1234");
    EXPECT_EQ(model.getErpVector("/erp/vector"), vector);
    EXPECT_EQ(model.getDecodedErpVector("/erp/base64-vector"), vector);

    EXPECT_THROW(model.getErpVector("/fourth"), ErpException);
    EXPECT_THROW(model.getErpVector("/invalid"), ErpException);
    EXPECT_THROW(model.getErpVector("/invalid-vector"), ErpException);
}

TEST(EnrolmentModelTest, Setter)//NOLINT(readability-function-cognitive-complexity)
{
    {
        EnrolmentModel model;
        EXPECT_TRUE(model.serializeToString().empty());

        model.set("/set-one", "1");
        EXPECT_EQ(model.getString("/set-one"), "1");
        EXPECT_THROW(model.getInt64("/set-one"), ErpException);

        model.set("/set-one", "2");
        EXPECT_EQ(model.getString("/set-one"), "2");
        EXPECT_THROW(model.getInt64("/set-one"), ErpException);

        model.set("/set-two", "two");
        EXPECT_EQ(model.getString("/set-two"), "two");
    }

    {
        EnrolmentModel model{json};
        ASSERT_TRUE(model.serializeToString() == ::std::regex_replace(::std::string{json}, ::std::regex{"[ \n]"}, ""));

        model.set("/set-one", "1");
        EXPECT_EQ(model.getString("/set-one"), "1");
        EXPECT_THROW(model.getInt64("/set-one"), ErpException);

        model.set("/first", "2");
        EXPECT_EQ(model.getString("/first"), "2");
        EXPECT_THROW(model.getInt64("/first"), ErpException);
    }
}
