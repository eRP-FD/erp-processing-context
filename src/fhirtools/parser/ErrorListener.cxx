// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
//
// non-exclusively licensed to gematik GmbH

#include "fhirtools/parser/ErrorListener.hxx"
#include "erp/util/TLog.hxx"

using fhirtools::ErrorListener;

void ErrorListener::syntaxError(antlr4::Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
                                size_t charPositionInLine, const std::string& msg, std::exception_ptr e)
{
    (void) recognizer;
    (void) e;
    if (offendingSymbol)
    {
        TLOG(ERROR) << "Syntax error at '" << offendingSymbol->getText() << "' in Fhir-Path Expression:" << line << ':'
                    << charPositionInLine << ": " << msg;
    }
    else
    {
        TLOG(ERROR) << "Syntax error in Fhir-Path Expression:" << line << ':' << charPositionInLine << ": " << msg;
    }
    mHadError = true;
}

bool fhirtools::ErrorListener::hadError() const
{
    return mHadError;
}
