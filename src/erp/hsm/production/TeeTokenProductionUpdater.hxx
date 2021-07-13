#ifndef ERP_PROCESSING_CONTEXT_TEETOKENPRODUCTIONUPDATER_HXX
#define ERP_PROCESSING_CONTEXT_TEETOKENPRODUCTIONUPDATER_HXX

#include "erp/hsm/TeeTokenUpdater.hxx"


class TeeTokenProductionUpdater
{
public:
    static ErpBlob provideTeeToken (HsmFactory& hsmFactory);
};


#endif
