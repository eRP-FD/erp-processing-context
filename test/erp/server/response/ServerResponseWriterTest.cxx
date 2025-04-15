/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/beast/BoostBeastHeader.hxx"

#include "shared/network/client/response/ClientResponseReader.hxx"
#include "shared/server/response/ValidatedServerResponse.hxx"
#include "shared/server/response/ServerResponseWriter.hxx"

#include <gtest/gtest.h>


class ServerResponseWriterTest : public testing::Test
{
};



TEST_F(ServerResponseWriterTest, toString)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string request =
        "HTTP/1.1 201 Created\r\n"
        "Content-Type: text\r\n"
        "Content-Length: 16\r\n"
        "\r\n"
        "this is the body";
    ClientResponseReader reader;
    auto clientResponse = reader.read(request);

    ServerResponse serverResponse{
        clientResponse.getHeader(),
        clientResponse.getBody()};
    const std::string serializedResponse = ServerResponseWriter().toString(ValidatedServerResponse(std::move(serverResponse)));

    // The header fields are an unordered map. The order in which the header fields are
    // serialized cannot be accurately predicted except that the field containing the HTTP version
    // and the status code has to be at the beginning and the body follows at the end.

    std::vector<std::string> reqFields = String::split(request, "\r\n");
    std::vector<std::string> rspFields = String::split(serializedResponse, "\r\n");

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
            auto req = reqFields[idxReqFld];
            int cnt = 0;
            for (size_t idxRspFld = 1; idxRspFld < rspFields.size()-2; ++idxRspFld)
            {
                auto rsp = rspFields[idxRspFld];
                if(req == rsp)
                {
                    ++cnt;
                }
            }
            EXPECT_EQ(cnt, 1);
        }
    }
}


TEST_F(ServerResponseWriterTest, nonStandardHttpCode)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string request =
        "HTTP/1.1 512 BackendCallFailed\r\n"
        "Content-Length: 16\r\n"
        "\r\n"
        "this is the body";
    ClientResponseReader reader;
    auto clientResponse = reader.read(request);
    ServerResponse serverResponse{
        clientResponse.getHeader(),
        clientResponse.getBody()};
    const std::string serializedResponse = ServerResponseWriter().toString(ValidatedServerResponse(std::move(serverResponse)));
    EXPECT_EQ(serializedResponse, "HTTP/1.1 512 <unknown-status>\r\n"
                                  "Content-Length: 16\r\n\r\n"
                                  "this is the body");
}
