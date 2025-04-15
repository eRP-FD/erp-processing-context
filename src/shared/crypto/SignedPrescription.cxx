/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "SignedPrescription.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/common/VauErrorCode.hxx"
#include "shared/model/ProfessionOid.hxx"
#include "shared/util/Expect.hxx"

// GEMREQ-start A_20159-03#fromBin
SignedPrescription SignedPrescription::fromBin(const std::string& content, TslManager& tslManager,
                                               const std::vector<std::string_view>& professionOids)
{
    return doUnpackCadesBesSignature(content, &tslManager, professionOids);
}
// GEMREQ-end A_20159-03#fromBin

SignedPrescription SignedPrescription::fromBinNoVerify(const std::string& content)
{
    return doUnpackCadesBesSignature(content, nullptr, {});
}


// GEMREQ-start A_20159-03#doUnpackCadesBesSignature
SignedPrescription SignedPrescription::doUnpackCadesBesSignature(const std::string& cadesBesSignatureFile,
                                                                 TslManager* tslManager,
                                                                 const std::vector<std::string_view>& professionOids)
{
    try
    {
        if (tslManager)
        {
            A_19025_03.start("verify the QES signature");
            A_20159.start("verify HBA Signature Certificate ");
            return {cadesBesSignatureFile, *tslManager, false, professionOids};
            A_20159.finish();
            A_19025_03.finish();
        }
        else
        {
            return SignedPrescription(cadesBesSignatureFile);
        }
    }
    catch (const TslError& ex)
    {
        std::string description;
        // Use the first error data as the description, if available. Otherwise fall back
        // to the full exception description
        if (! ex.getErrorData().empty())
        {
            description = ex.getErrorData()[0].message;
        }
        else
        {
            description = ex.what();
        }
        VauFail(ex.getHttpStatus(), VauErrorCode::invalid_prescription, description);
    }
    catch (const CadesBesSignature::UnexpectedProfessionOidException& ex)
    {
        A_19225_02.start("Report 400 because of unexpected ProfessionOIDs in QES certificate.");
        A_25990.start("Report 400 because of unexpected QES certificate.");
        VauFail(HttpStatus::BadRequest, VauErrorCode::invalid_prescription, ex.what());
        A_25990.finish();
        A_19225_02.finish();
    }
    catch (const CadesBesSignature::VerificationException& ex)
    {
        VauFail(HttpStatus::BadRequest, VauErrorCode::invalid_prescription, ex.what());
    }
    catch (const ErpException& ex)
    {
        TVLOG(1) << "ErpException: " << ex.what();
        VauFail(ex.status(), VauErrorCode::invalid_prescription, ex.what());
    }
    catch (const model::ModelException& mo)
    {
        TVLOG(1) << "ModelException: " << mo.what();
        VauFail(HttpStatus::BadRequest, VauErrorCode::invalid_prescription, "ModelException");
    }
    catch (const std::exception& ex)
    {
        VauFail(HttpStatus::InternalServerError, VauErrorCode::invalid_prescription,
                ex.what());
    }
    catch (...)
    {
        VauFail(HttpStatus::InternalServerError, VauErrorCode::invalid_prescription,
                "unexpected throwable");
    }
}

// GEMREQ-end A_20159-03#doUnpackCadesBesSignature
