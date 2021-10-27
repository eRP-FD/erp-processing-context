/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_HSMPOOLSESSION_HXX
#define ERP_PROCESSING_CONTEXT_HSMPOOLSESSION_HXX

#include "erp/hsm/HsmSession.hxx"
#include "erp/hsm/HsmPoolSessionRemover.hxx"


/**
 * This is a thin wrapper around HsmSession with the goal
 * to move the HsmSession base object back into the HsmPool
 * when the destructor is executed.
 */
class HsmPoolSession
{
public:
    HsmPoolSession (void);
    HsmPoolSession (std::unique_ptr<HsmSession>&& session, const std::shared_ptr<HsmPoolSessionRemover>& sessionRemover);
    HsmPoolSession (HsmPoolSession&& other) noexcept;

    HsmPoolSession& operator= (HsmPoolSession&& other) noexcept;

    /**
     * Release the HsmSession back into the HSM pool.
     */
    ~HsmPoolSession (void);

    HsmSession& session();

private:
    std::unique_ptr<HsmSession> mSession;
    std::shared_ptr<HsmPoolSessionRemover> mSessionRemover;

    void releaseBackIntoPool (void);
};


#endif
