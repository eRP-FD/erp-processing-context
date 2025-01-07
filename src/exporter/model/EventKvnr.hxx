/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EVENTKVNR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EVENTKVNR_HXX

#include "shared/model/Timestamp.hxx"

#include <string>
#include <optional>

namespace model
{

class EventKvnr
{
public:
    enum class State : uint8_t
    {
        pending,
        processing,
        processed
    };


    EventKvnr(const std::basic_string<std::byte>& kvnrHashed, std::optional<Timestamp> lastConsentCheck,
              const std::optional<std::string>& assignedEpa, State state, std::int32_t retryCount);
    ~EventKvnr() = default;

    const std::basic_string<std::byte>& kvnrHashed() const;
    std::string getLoggingId() const;
    std::optional<Timestamp> getLastConsentCheck() const;
    const std::optional<std::string>& getASsignedEpa() const;
    State getState() const;
    virtual std::int32_t getRetryCount() const;

private:
    std::basic_string<std::byte> mKvnrHashed;
    std::optional<Timestamp> mLastConsentCheck;
    std::optional<std::string> mASsignedEpa;
    State mState;
    std::int32_t mRetryCount;
};

}// model

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EVENTKVNR_HXX
