/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/BlobDatabase.hxx"

#include "fhirtools/model/Timestamp.hxx"

namespace
{
bool hasCorrectPcrHash(const ::BlobDatabase::Entry& entry, const ::ErpVector& currentPcrHash)
{
    if ((entry.type == ::BlobType::Quote) && ! currentPcrHash.empty() && entry.pcrHash)
    {
        if (entry.pcrHash.value() != currentPcrHash)
        {
            return false;
        }
    }

    return true;
}

}

bool BlobDatabase::Entry::isBlobValid(::std::chrono::system_clock::time_point now,
                                      const ::ErpVector& currentPcrHash) const
{
    if (expiryDateTime.has_value())
    {
        if (expiryDateTime.value() <= now)
        {
            TLOG(WARNING) << "expiration date lies in the past: " << fhirtools::Timestamp(expiryDateTime.value()).toXsDateTime();
            return false;
        }
    }
    else
    {
        if (startDateTime.has_value())
            if (startDateTime.value() > now)
            {
                TLOG(WARNING) << "start time lies in the future: " << fhirtools::Timestamp(startDateTime.value()).toXsDateTime();
                return false;
            }
        if (endDateTime.has_value())
            if (endDateTime.value() <= now)
            {
                TLOG(WARNING) << "end time lies in the past: " << fhirtools::Timestamp(endDateTime.value()).toXsDateTime();
                return false;
            }
    }

    return hasCorrectPcrHash(*this, currentPcrHash);
}


const ErpArray<TpmObjectNameLength>& BlobDatabase::Entry::getAkName (void) const
{
    Expect(metaAkName.has_value(), "blob entry does not have a metaAkName value");
    return metaAkName.value();
}


const PcrSet& BlobDatabase::Entry::getPcrSet (void) const
{
    Expect(pcrSet.has_value(), "blob entry does not have a pcrSet value");
    return pcrSet.value();
}
