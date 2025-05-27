/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SHARED_TRANSACTIONMODE_HXX
#define ERP_PROCESSING_CONTEXT_SHARED_TRANSACTIONMODE_HXX

#include <cstdint>

enum class TransactionMode : uint8_t {
    autocommit,
    transaction,
};

#endif// ERP_PROCESSING_CONTEXT_SHARED_TRANSACTIONMODE_HXX
