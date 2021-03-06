/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ZSTDTESTHELPER_HXX
#define ERP_PROCESSING_CONTEXT_ZSTDTESTHELPER_HXX

#include "erp/compression/Compression.hxx"

#include <string_view>

std::string sampleDictionary(Compression::DictionaryUse dictUse, uint8_t version);


#endif// ERP_PROCESSING_CONTEXT_ZSTDTESTHELPER_HXX
