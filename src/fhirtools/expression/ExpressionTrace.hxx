/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_EXPRESSIONTRACE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_EXPRESSIONTRACE_HXX

namespace fhirtools
{
#define EVAL_TRACE                                                                                                     \
    TVLOG(3) << std::source_location::current().function_name() << " " << debugInfo() << " collection: " << collection;
}

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_EXPRESSIONTRACE_HXX
