/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ZSTDTESTHELPER_HXX
#define ERP_PROCESSING_CONTEXT_ZSTDTESTHELPER_HXX

#include "shared/compression/Compression.hxx"

#include <string_view>

std::string sampleDictionary(Compression::DictionaryUse dictUse, uint8_t version);


#endif// ERP_PROCESSING_CONTEXT_ZSTDTESTHELPER_HXX
