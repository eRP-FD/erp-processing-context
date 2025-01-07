/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_BDEMESSAGE_HXX
#define ERP_PROCESSING_CONTEXT_BDEMESSAGE_HXX

#include "shared/model/PrescriptionId.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/TLog.hxx"

#include <any>

class BDEMessage
{
public:
    static inline const std::string lastModifiedTimestampKey{"lastModifiedTimestamp"};
    static inline const std::string prescriptionIdKey{"prescriptionId"};

    BDEMessage();
    virtual ~BDEMessage();
    constexpr static std::string log_type = "bde";

    model::Timestamp mStartTime;
    std::string mRequestId;
    std::string mInnerOperation;
    std::string mPrescriptionId;
    std::string mHost;
    std::string mIp;
    std::optional<std::string> mCid;
    std::optional<unsigned int> mInnerResponseCode;
    unsigned int mResponseCode;
    model::Timestamp mEndTime;
    model::Timestamp mLastModified;
    std::string mError;

    template <typename T>
    static void assignIfContains(const std::unordered_map<std::string, std::any>& data,
                                 const std::string& key,
                                 T& target)
    {
        if (data.contains(key))
        {
            try
            {
                target = std::any_cast<T>(data.at(key));
            }
            catch (const std::bad_any_cast& exception)
            {
                TLOG(ERROR) << "Bad cast for BDEMessage property. " << exception.what();
            }
        }
    }

private:
    void publish();
};

#endif//ERP_PROCESSING_CONTEXT_BDEMESSAGE_HXX
