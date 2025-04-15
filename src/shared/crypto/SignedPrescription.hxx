/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef ERP_PROCESSIONG_CONTEXT_SIGNEDPRESCRIPTION_HXX
#define ERP_PROCESSIONG_CONTEXT_SIGNEDPRESCRIPTION_HXX

#include "shared/crypto/CadesBesSignature.hxx"

#include <string>

class TslManager;

class SignedPrescription : public CadesBesSignature
{
public:
    static SignedPrescription fromBin(const std::string& content, TslManager& tslManager,
                                      const std::vector<std::string_view>& professionOids);
    static SignedPrescription fromBinNoVerify(const std::string& content);
private:
    using CadesBesSignature::CadesBesSignature;
    static SignedPrescription doUnpackCadesBesSignature(const std::string& cadesBesSignatureFile,
                                                        TslManager* tslManager,
                                                        const std::vector<std::string_view>& professionOids);
};


#endif// ERP_PROCESSIONG_CONTEXT_SIGNEDPRESCRIPTION_HXX
