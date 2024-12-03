/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MAINSTATE_HXX
#define ERP_PROCESSING_CONTEXT_MAINSTATE_HXX

#include "shared/util/Condition.hxx"


/**
 * This set of states is primarily intended for tests so that a test can wait for a certain state to be
 * reached. See ErpMainTest.cxx for an example.
 */
enum class MainState
{
    Unknown,
    Initializing,
    WaitingForTermination,
    Terminating,
    Terminated
};


using MainStateCondition = Condition<MainState>;


#endif//ERP_PROCESSING_CONTEXT_MAINSTATE_HXX
