#include "erp/hsm/BlobDatabase.hxx"

#include "erp/model/Timestamp.hxx"


bool BlobDatabase::Entry::isBlobValid (std::chrono::system_clock::time_point now) const
{
    if (expiryDateTime.has_value())
    {
        if (expiryDateTime.value() <= now)
        {
            TLOG(WARNING) << "expiration date lies in the past: " << model::Timestamp(expiryDateTime.value()).toXsDateTime();
            return false;
        }
    }
    else
    {
        if (startDateTime.has_value())
            if (startDateTime.value() > now)
            {
                TLOG(WARNING) << "start time lies in the future: " << model::Timestamp(startDateTime.value()).toXsDateTime();
                return false;
            }
        if (endDateTime.has_value())
            if (endDateTime.value() <= now)
            {
                TLOG(WARNING) << "end time lies in the past: " << model::Timestamp(endDateTime.value()).toXsDateTime();
                return false;
            }
    }

    return true;
}


const ErpArray<TpmObjectNameLength>& BlobDatabase::Entry::getAkName (void) const
{
    Expect(metaAkName.has_value(), "blob entry does not have a metaAkName value");
    return metaAkName.value();
}


const PcrSet& BlobDatabase::Entry::getPcrSet (void) const
{
    Expect(metaPcrSet.has_value(), "blob entry does not have a metaPcrSet value");
    return metaPcrSet.value();
}
