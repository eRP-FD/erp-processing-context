/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_EXPRESSIONTRACE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_EXPRESSIONTRACE_HXX

namespace fhirtools
{
#define EVAL_TRACE                                                                                                     \
    TVLOG(3) << std::source_location::current().function_name() << " " << debugInfo() << " collection: " << collection;
}

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_EXPRESSIONTRACE_HXX
