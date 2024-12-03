/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/search/PagingArgument.hxx"

#include "shared/ErpRequirements.hxx"
#include "shared/network/message/HttpStatus.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/String.hxx"


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


PagingArgument::PagingArgument ()
    : mCount(getDefaultCount()),
      mOffset(0),
      mTotalSearchMatches(0)
{
}


void PagingArgument::setCount (const std::string& countString)
{
    mCount = toSizeT(countString, "_count", false, getDefaultCount());
}


bool PagingArgument::hasDefaultCount () const
{
    return mCount == getDefaultCount();
}


size_t PagingArgument::getCount () const
{
    return mCount;
}


size_t PagingArgument::getDefaultCount ()
{
    A_19534.start("limit page size to 50 entries");
    return 50;
    A_19534.finish();
}


void PagingArgument::setOffset (const std::string& offsetString)
{
    mOffset = toSizeT(offsetString, "__offset", true);
}


size_t PagingArgument::getOffset () const
{
    return mOffset;
}


bool PagingArgument::isSet () const
{
    return mCount != getDefaultCount() || mOffset>0;
}


bool PagingArgument::hasPreviousPage () const
{
    return mOffset > 0;
}


bool PagingArgument::hasNextPage (const std::size_t totalSearchMatches) const
{
    return totalSearchMatches > mOffset + mCount;
}


void PagingArgument::setEntryTimestampRange(const model::Timestamp& firstEntry, const model::Timestamp& lastEntry)
{
    mEntryTimestampRange.emplace(firstEntry, lastEntry);
}


std::optional<std::pair<model::Timestamp, model::Timestamp>> PagingArgument::getEntryTimestampRange() const
{
    return mEntryTimestampRange;
}

void PagingArgument::setTotalSearchMatches(const std::size_t totalSearchMatches)
{
    mTotalSearchMatches = totalSearchMatches;
}

size_t PagingArgument::getTotalSearchMatches() const
{
    return mTotalSearchMatches;
}

size_t PagingArgument::getOffsetLastPage() const
{
    if (mCount == 0)
    {
        throw std::invalid_argument("invalid count");
    }

    size_t prevPages = mTotalSearchMatches / mCount;
    size_t offsetLastPage = prevPages * mCount;

    if (offsetLastPage == mTotalSearchMatches)
    {
        offsetLastPage = offsetLastPage - mCount;
    }

    return offsetLastPage;
}
