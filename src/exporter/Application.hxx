// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_APPLICATION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_APPLICATION_HXX

#include "shared/Application.hxx"

namespace exporter
{

class Application : public ::Application
{
    void printConfiguration() override;
};

}

#endif
