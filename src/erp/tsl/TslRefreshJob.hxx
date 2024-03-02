/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_TSL_TSLREFRESHJOB_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_TSL_TSLREFRESHJOB_HXX

#include "erp/tsl/TslManager.hxx"
#include "erp/util/TimerJobBase.hxx"

/**
 * This implementation is responsible to trigger TSL-check/-refresh in standalone thread with specified interval.
 */
class TslRefreshJob : public TimerJobBase
{
public:
    TslRefreshJob(TslManager& tslManager,
                  const std::chrono::steady_clock::duration interval);
    ~TslRefreshJob() noexcept override = default;

protected:
    void onStart(void) override;
    void executeJob(void) override;
    void onFinish(void) override;

private:
    TslManager& mTslManager;
};


#endif
