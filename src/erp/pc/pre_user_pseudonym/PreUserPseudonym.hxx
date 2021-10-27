/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_PREUSERPSEUDONYM_PREUSERPSEUDONYM_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_PREUSERPSEUDONYM_PREUSERPSEUDONYM_HXX

#include "erp/crypto/CMAC.hxx"

class PreUserPseudonym
{
public:
    explicit PreUserPseudonym(const CmacSignature& preUserPseudonym);

    [[nodiscard]]
    static PreUserPseudonym fromUserPseudonym(const std::string_view& userPseudonym);

    [[nodiscard]]
    const CmacSignature& getSignature() const;

private:
    CmacSignature mPreUserPseudonym;
};


#endif
