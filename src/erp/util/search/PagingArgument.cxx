/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/search/PagingArgument.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/common/HttpStatus.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/String.hxx"


namespace
{
size_t toSizeT(const std::string& numberString, const std::string& fieldName, bool zeroAllowed,
               const std::optional<size_t>& maxValue = std::nullopt)
{
    try
    {
        // According to https://www.hl7.org/fhir/search.html#errors, especially
        //     "Where the content of the parameter is syntactically incorrect, servers SHOULD return an error."
        size_t length = 0;
        const auto count = std::stoi(numberString, &length);
        ErpExpect(length == numberString.size(), HttpStatus::BadRequest,
                  "trailing characters are not permitted in a numerical argument: " + fieldName);
        ErpExpect(count>=0, HttpStatus::BadRequest, fieldName + " can not be negative");
        ErpExpect(count!=0 || zeroAllowed, HttpStatus::BadRequest, fieldName + " zero is not supported");
        return maxValue?std::min(static_cast<size_t>(count), *maxValue):static_cast<size_t>(count);
    }
    catch (const std::invalid_argument&)
    {
        TVLOG(1) << "invalid numeric format in " << fieldName << ": " << numberString;
        ErpFail(HttpStatus::BadRequest, "invalid numeric format in " +fieldName);
    }
    catch (const std::out_of_range&)
    {
        TVLOG(1) << fieldName << " value out of range: " << numberString;
        ErpFail(HttpStatus::BadRequest, fieldName + " value out of range");
    }
}
}


PagingArgument::PagingArgument (void)
    : mCount(getDefaultCount()),
      mOffset(0)
{
}


void PagingArgument::setCount (const std::string& countString)
{
    mCount = toSizeT(countString, "_count", false, getDefaultCount());
}


bool PagingArgument::hasDefaultCount (void) const
{
    return mCount == getDefaultCount();
}


size_t PagingArgument::getCount (void) const
{
    return mCount;
}


size_t PagingArgument::getDefaultCount (void)
{
    A_19534.start("limit page size to 50 entries");
    return 50;
    A_19534.finish();
}


void PagingArgument::setOffset (const std::string& offsetString)
{
    mOffset = toSizeT(offsetString, "__offset", true);
}


size_t PagingArgument::getOffset (void) const
{
    return mOffset;
}


bool PagingArgument::isSet (void) const
{
    return mCount != getDefaultCount() || mOffset>0;
}


bool PagingArgument::hasPreviousPage (void) const
{
    return mOffset > 0;
}


bool PagingArgument::hasNextPage (const std::size_t& totalSearchMatches) const
{
        return totalSearchMatches > mOffset + mCount;
}
