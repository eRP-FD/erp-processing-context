/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/beast/BoostBeastHeader.hxx"

#include "shared/beast/BoostBeastStringReader.hxx"
#include "shared/network/client/request/ValidatedClientRequest.hxx"
#include "shared/network/client/request/ClientRequestWriter.hxx"


#include <gtest/gtest.h>


class ClientRequestWriterTest : public testing::Test
{
public:
};



TEST_F(ClientRequestWriterTest, toString)//NOLINT(readability-function-cognitive-complexity)
{
    // Given.
    const std::string request =
        "POST /this/is/a/path HTTP/1.1\r\n"
        "Content-Length: 16\r\n"
        "Content-Type: text\r\n"
        "\r\n"
        "this is the body";
    auto [header, body] = BoostBeastStringReader::parseRequest(request);

    // When a given request is serialized.
    const std::string serializedRequest =
        ClientRequestWriter(ValidatedClientRequest(ClientRequest{header,std::string(body)})).toString();

    // Note that the order of the header fields is not guaranteed.
    // Algorithm copied from ServerResponseWriterTest.cxx:

    std::vector<std::string> reqFields = String::split(request, "\r\n");
    std::vector<std::string> rspFields = String::split(serializedRequest, "\r\n");

    EXPECT_EQ(rspFields.size(), 5);
    EXPECT_EQ(rspFields.size(), reqFields.size());

    if(rspFields.size() == 5 && rspFields.size() == reqFields.size())
    {
        // HTTP Version has to be the first field
        EXPECT_EQ(rspFields[0], reqFields[0]);
        // Body must be the last section.
        EXPECT_EQ(rspFields[3], "");
        EXPECT_EQ(reqFields[3], "");
        EXPECT_EQ(rspFields[4], reqFields[4]);

        // Only the two header fields (Content-Type, Content-Length) remain to be compared.
        // The order the fields are added to the unsorted map of the Header class
        // though is not predictable.
        for (size_t idxReqFld = 1; idxReqFld < rspFields.size()-2; ++idxReqFld)
        {
            const auto& req = reqFields[idxReqFld];
            int cnt = 0;
            for (size_t idxRspFld = 1; idxRspFld < rspFields.size()-2; ++idxRspFld)
            {
                const auto& rsp = rspFields[idxRspFld];
                if(req == rsp)
                {
                    ++cnt;
                }
            }
            EXPECT_EQ(cnt, 1);
        }
    }

}
