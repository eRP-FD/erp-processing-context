// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#include "fhirtools/parser/ErrorListener.hxx"

#include <glog/logging.h>

using fhirtools::ErrorListener;

void ErrorListener::syntaxError(antlr4::Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
                                size_t charPositionInLine, const std::string& msg, std::exception_ptr e)
{
    (void) recognizer;
    (void) e;
    if (offendingSymbol)
    {
        LOG(ERROR) << "Syntax error at '" << offendingSymbol->getText() << "' in Fhir-Path Expression:" << line << ':'
                    << charPositionInLine << ": " << msg;
    }
    else
    {
        LOG(ERROR) << "Syntax error in Fhir-Path Expression:" << line << ':' << charPositionInLine << ": " << msg;
    }
    mHadError = true;
}

bool fhirtools::ErrorListener::hadError() const
{
    return mHadError;
}
