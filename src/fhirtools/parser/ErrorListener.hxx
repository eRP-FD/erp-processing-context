// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
//
// non-exclusively licensed to gematik GmbH

#ifndef FHIRTOOLS_ERRORLISTENER_H
#define FHIRTOOLS_ERRORLISTENER_H

#include "antlr4-runtime.h"

namespace fhirtools
{

class ErrorListener : public antlr4::BaseErrorListener
{
public:
    virtual void syntaxError(antlr4::Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
                             size_t charPositionInLine, const std::string& msg, std::exception_ptr e) override;
    [[nodiscard]] bool hadError() const;
private:
    bool mHadError = false;
};

}

#endif// FHIRTOOLS_ERRORLISTENER_H
