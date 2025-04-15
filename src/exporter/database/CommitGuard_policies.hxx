/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_EXPORTER_COMMITGUARD_POLICIES_HXX
#define ERP_PROCESSING_CONTEXT_EXPORTER_COMMITGUARD_POLICIES_HXX

#include <concepts>

namespace commit_guard_policies
{

template<template<class Database> class Strategy, class Database>
concept DbExceptionHandlingStrategy = requires(Strategy<Database> strategy, Database& db) {
    { Strategy<Database>::onException(db) } -> std::same_as<void>;
};

template<class Database>
struct ExceptionAbortStrategy {
    using database_t = Database;
    static void onException(database_t&)
    {
    }
};

template<class Database>
struct ExceptionCommitStrategy {
    using database_t = Database;
    static void onException(database_t& db)
    {
        db.commitTransaction();
    }
};

}

#endif//ERP_PROCESSING_CONTEXT_EXPORTER_COMMITGUARD_POLICIES_HXX
