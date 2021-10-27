/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/tee/InnerTeeRequest.hxx"

#include "erp/beast/BoostBeastHeader.hxx"
#include "erp/beast/BoostBeastStringReader.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Expect.hxx"


struct InnerTeeRequest::Data
{
    Data (void);

    std::string mVersion;
    JWT mAuthenticationToken;
    std::string mRequestId;
    SafeString mAesKey;
    Header mHeader;
    std::string mBody;
    std::string mHeaderAndBodyRaw;
};


namespace {
    std::string::size_type verifyLeadingSpace  (const std::string_view p, const std::string::size_type start)
    {
        ErpExpect(p.size() > start+1, HttpStatus::BadRequest, "item is empty");
        if (start > 0)
        {
            ErpExpect(p[start] == ' ', HttpStatus::BadRequest, "item is not separated by leading space");
            ErpExpect(p[start+1] != ' ', HttpStatus::BadRequest, "item is not separated by leading space");
            return start + 1;
        }
        else
        {
            ErpExpect(p[0] != ' ', HttpStatus::BadRequest, "first item has leading separator");
            return 0;
        }
    }


    /**
     * Verify that the item is delimited at both ends by spaces unless it starts at position 0.
     * @return the position of the space at its end.
     */
    std::string::size_type getItemEnd (const std::string_view p, std::string::size_type start)
    {
        start = verifyLeadingSpace(p, start);

        const auto end = p.find(' ', start);
        ErpExpect(end != std::string::npos, HttpStatus::BadRequest, "item has no end");
        ErpExpect(start<end, HttpStatus::BadRequest, "item is empty");
        return end;
    }



    /**
     * See gemSpec_Krypt_V2.18, A_20161-01, item 5 for the structure of p.
     */
    std::unique_ptr<InnerTeeRequest::Data> disassembleP (const SafeString& p)
    {
        const auto versionEnd = getItemEnd(p, 0);
        const auto authenticationTokenEnd(getItemEnd(p, versionEnd));
        const auto requestIdEnd(getItemEnd(p, authenticationTokenEnd));
        const auto aesKeyEnd(getItemEnd(p, requestIdEnd));

        auto data = std::make_unique<InnerTeeRequest::Data>();
        std::string_view pStr = p;
        data->mVersion = pStr.substr(0, versionEnd);
        data->mAuthenticationToken = JWT(std::string(pStr.substr(versionEnd+1, authenticationTokenEnd-versionEnd-1)));
        data->mRequestId =
            ByteHelper::fromHex(pStr.substr(authenticationTokenEnd + 1, requestIdEnd - authenticationTokenEnd - 1));
        data->mAesKey = SafeString(ByteHelper::fromHex(pStr.substr(requestIdEnd+1, aesKeyEnd-requestIdEnd-1)));

        data->mHeaderAndBodyRaw = pStr.substr(aesKeyEnd + 1);
        return data;
    }
}

void InnerTeeRequest::parseHeaderAndBody()
{
    std::tie(mData->mHeader, mData->mBody) = BoostBeastStringReader::parseRequest(mData->mHeaderAndBodyRaw);
}


InnerTeeRequest::InnerTeeRequest (const SafeString& p)
    : mData(disassembleP(p))
{
    ErpExpect(mData!=nullptr, HttpStatus::BadRequest, "disassembling p failed");
}


InnerTeeRequest::InnerTeeRequest (InnerTeeRequest&& other) noexcept
    : mData(std::move(other.mData))
{
}


InnerTeeRequest::~InnerTeeRequest (void) = default;


const std::string& InnerTeeRequest::version (void) const
{
    return mData->mVersion;
}


const JWT& InnerTeeRequest::authenticationToken (void) const
{
    return mData->mAuthenticationToken;
}


JWT&& InnerTeeRequest::releaseAuthenticationToken (void)
{
    return std::move(mData->mAuthenticationToken);
}


const std::string& InnerTeeRequest::requestId (void) const
{
    return mData->mRequestId;
}


const SafeString& InnerTeeRequest::aesKey (void) const
{
    return mData->mAesKey;
}


const Header& InnerTeeRequest::header (void) const
{
    return mData->mHeader;
}


Header InnerTeeRequest::releaseHeader (void)
{
    return std::move(mData->mHeader);
}


const std::string& InnerTeeRequest::body (void) const
{
    return mData->mBody;
}


std::string InnerTeeRequest::releaseBody (void)
{
    std::string body;
    body.swap(mData->mBody);
    return body;
}


InnerTeeRequest::Data::Data (void)
    : mVersion(),
      mAuthenticationToken(),
      mRequestId(),
      mAesKey(),
      mHeader(HttpStatus::OK),
      mBody()
{
}
