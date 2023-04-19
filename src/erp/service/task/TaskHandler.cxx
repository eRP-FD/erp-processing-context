/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "TaskHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/crypto/Jws.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/ModelException.hxx"
#include "erp/model/Signature.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/TLog.hxx"


#include <algorithm>
#include <cstddef>
#include <chrono>
#include <ctime>
#include <sstream>


TaskHandlerBase::TaskHandlerBase(const Operation operation,
                                 const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : ErpRequestHandler(operation, allowedProfessionOIDs)
{
}

void TaskHandlerBase::addToPatientBundle(model::Bundle& bundle, const model::Task& task,
                                         const std::optional<model::KbvBundle>& patientConfirmation,
                                         const std::optional<model::Bundle>& receipt)
{
    bundle.addResource(makeFullUrl("/Task/" + task.prescriptionId().toString()),
                       {}, {}, task.jsonDocument());

    if (patientConfirmation.has_value())
    {
        bundle.addResource(patientConfirmation->getId().toUrn(), {}, {}, patientConfirmation->jsonDocument());
    }
    if (receipt.has_value())
    {
        bundle.addResource(receipt->getId().toUrn(), {}, {}, receipt->jsonDocument());
    }
}


model::KbvBundle TaskHandlerBase::convertToPatientConfirmation(const model::Binary& healthcareProviderPrescription,
                                                            const Uuid& uuid, PcServiceContext& serviceContext)
{
    A_19029_05.start("1. convert prescription bundle to JSON");
    Expect3(healthcareProviderPrescription.data().has_value(), "healthcareProviderPrescription unexpected empty Binary",
            std::logic_error);

    // read CAdES-BES signature, but do not verify it
    const CadesBesSignature cadesBesSignature =
        CadesBesSignature(std::string(*healthcareProviderPrescription.data()));

    // this comes from the database and has already been validated when initially received
    auto bundle = model::KbvBundle::fromXmlNoValidation(cadesBesSignature.payload());
    A_19029_05.finish();

    A_19029_05.start("assign identifier");
    bundle.setId(uuid);
    A_19029_05.finish();

    A_19029_05.start("2. serialize to normalized JSON");
    auto normalizedJson = bundle.serializeToCanonicalJsonString();
    A_19029_05.finish();

    A_19029_05.start("3. sign the JSON bundle using C.FD.SIG");
    JoseHeader joseHeader(JoseHeader::Algorithm::BP256R1);
    joseHeader.setX509Certificate(serviceContext.getCFdSigErp());
    joseHeader.setType(ContentMimeType::jose);
    joseHeader.setContentType(ContentMimeType::fhirJsonUtf8);

    const Jws jsonWebSignature(joseHeader, normalizedJson, serviceContext.getCFdSigErpPrv());
    const auto signatureData = jsonWebSignature.compactDetachedSerialized();
    A_19029_05.finish();

    A_19029_05.start("store the signature in the bundle");
    model::Signature signature(Base64::encode(signatureData), model::Timestamp::now(),
                               model::Device::createReferenceString(getLinkBase()));
    signature.setTargetFormat(MimeType::fhirJson);
    signature.setSigFormat(MimeType::jose);
    signature.setType(model::Signature::jwsSystem, model::Signature::jwsCode);
    bundle.setSignature(signature);
    A_19029_05.finish();

    return bundle;
}


model::PrescriptionId TaskHandlerBase::parseId(const ServerRequest& request, AccessLog& accessLog)
{
    const auto prescriptionIdValue = request.getPathParameter("id");
    ErpExpect(prescriptionIdValue.has_value(), HttpStatus::BadRequest, "id path parameter is missing");

    try
    {
        auto prescriptionId = model::PrescriptionId::fromString(prescriptionIdValue.value());
        accessLog.prescriptionId(prescriptionIdValue.value());
        return prescriptionId;
    }
    catch (const model::ModelException&)
    {
        ErpFail(HttpStatus::NotFound, "Failed to parse prescription ID from URL parameter");
    }
}

// GEMREQ-start checkAccessCodeMatches
void TaskHandlerBase::checkAccessCodeMatches(const ServerRequest& request, const model::Task& task)
{
    auto accessCode = request.getQueryParameter("ac");
    if (! accessCode.has_value())
    {
        accessCode = request.header().header(Header::XAccessCode);
    }
    A_20703.start("Set VAU-Error-Code header field to brute-force whenever AccessCode or Secret mismatches");
    VauExpect(accessCode == task.accessCode(), HttpStatus::Forbidden, VauErrorCode::brute_force,
              "AccessCode mismatch");
    A_20703.finish();
}
// GEMREQ-end checkAccessCodeMatches

// GEMREQ-start A_20159-02#doUnpackCadesBesSignature
CadesBesSignature TaskHandlerBase::doUnpackCadesBesSignature(const std::string& cadesBesSignatureFile,
                                                             TslManager* tslManager)
{
    try
    {
        if (tslManager)
        {
            A_19025_02.start("verify the QES signature");
            A_20159.start("verify HBA Signature Certificate ");
            return {cadesBesSignatureFile,
                    *tslManager,
                    false,
                    {profession_oid::oid_arzt,
                     profession_oid::oid_zahnarzt,
                     profession_oid::oid_arztekammern}};
            A_20159.finish();
            A_19025_02.finish();
        }
        else
        {
            return CadesBesSignature(cadesBesSignatureFile);
        }
    }
    catch (const TslError& ex)
    {
        VauFail(ex.getHttpStatus(), VauErrorCode::invalid_prescription, ex.what());
    }
    catch (const CadesBesSignature::UnexpectedProfessionOidException& ex)
    {
        A_19225.start("Report 400 because of unexpected ProfessionOIDs in QES certificate.");
        VauFail(HttpStatus::BadRequest, VauErrorCode::invalid_prescription, ex.what());
        A_19225.finish();
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
// GEMREQ-end A_20159-02#doUnpackCadesBesSignature

CadesBesSignature TaskHandlerBase::unpackCadesBesSignature(const std::string& cadesBesSignatureFile,
                                                           TslManager& tslManager)
{
    return doUnpackCadesBesSignature(cadesBesSignatureFile, &tslManager);
}

CadesBesSignature TaskHandlerBase::unpackCadesBesSignatureNoVerify(const std::string& cadesBesSignatureFile)
{
    return doUnpackCadesBesSignature(cadesBesSignatureFile, nullptr);
}
