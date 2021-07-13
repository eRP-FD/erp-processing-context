#ifndef ERP_PROCESSING_CONTEXT_DATABASE_DATA_HXX
#define ERP_PROCESSING_CONTEXT_DATABASE_DATA_HXX

#include <string>

/**
 * Temporary class that is a stand-in for the Fhire representation of prescriptions, mTasks and mCommunications.
 */
struct Data
{
    std::string id;
    std::string displayText;
};


#endif
