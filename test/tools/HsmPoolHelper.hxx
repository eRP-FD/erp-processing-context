#ifndef ERP_PROCESSING_CONTEXT_HSMPOOLHELPER_HXX
#define ERP_PROCESSING_CONTEXT_HSMPOOLHELPER_HXX

#include <erp/util/Expect.hxx>
#include "erp/hsm/HsmPool.hxx"


/**
 * This helper allows us to e.g. clear the HsmPool instance, while keeping it alive.
 */
class HsmPoolHelper
{
public:
    static void resetHsmPool (HsmPool& pool);
};


#endif
