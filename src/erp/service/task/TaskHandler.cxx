/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "TaskHandler.hxx"
#include "erp/model/Task.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/CadesBesSignature.hxx"
#include "shared/crypto/Jws.hxx"
#include "shared/model/Binary.hxx"
#include "shared/model/Device.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/Signature.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/TLog.hxx"


#include <cstddef>
#include <chrono>
#include <ctime>
#include <sstream>


TaskHandlerBase::TaskHandlerBase(const Operation operation,
                                 const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : ErpRequestHandler(operation, allowedProfessionOIDs)
{
}
TaskHandlerBase::TaskHandlerBase(Operation operation, const OIDsByWorkflow& allowedProfessionOiDsByWorkflow)
    : ErpRequestHandler(operation, allowedProfessionOiDsByWorkflow)
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
    A_19029_06.start("1. convert prescription bundle to JSON");
    Expect3(healthcareProviderPrescription.data().has_value(), "healthcareProviderPrescription unexpected empty Binary",
            std::logic_error);

    // read CAdES-BES signature, but do not verify it
    const CadesBesSignature cadesBesSignature =
        CadesBesSignature(std::string(*healthcareProviderPrescription.data()));

    // this comes from the database and has already been validated when initially received
    auto bundle = model::KbvBundle::fromXmlNoValidation(cadesBesSignature.payload());
    A_19029_06.finish();

    A_19029_06.start("assign identifier");
    bundle.setId(uuid);
    A_19029_06.finish();

    A_19029_06.start("2. serialize to normalized JSON");
    auto normalizedJson = bundle.serializeToCanonicalJsonString();
    A_19029_06.finish();

    A_19029_06.start("3. sign the JSON bundle using C.FD.SIG");
    JoseHeader joseHeader(JoseHeader::Algorithm::BP256R1);
    joseHeader.setX509Certificate(serviceContext.getCFdSigErp());
    joseHeader.setType(ContentMimeType::jose);
    joseHeader.setContentType(ContentMimeType::fhirJsonUtf8);

    const Jws jsonWebSignature(joseHeader, normalizedJson, serviceContext.getCFdSigErpPrv());
    const auto signatureData = jsonWebSignature.compactDetachedSerialized();
    A_19029_06.finish();

    A_19029_06.start("store the signature in the bundle");
    model::Signature signature(Base64::encode(signatureData), model::Timestamp::now(),
                               model::Device::createReferenceUrl(getLinkBase()));
    signature.setTargetFormat(MimeType::fhirJson);
    signature.setSigFormat(MimeType::jose);
    signature.setType(model::Signature::jwsSystem, model::Signature::jwsCode);
    bundle.setSignature(signature);
    A_19029_06.finish();

    return bundle;
}


// GEMREQ-start getAccessCode
std::optional<std::string> TaskHandlerBase::getAccessCode(const ServerRequest& request)
{
    auto accessCode = request.getQueryParameter("ac");
    if (! accessCode.has_value())
    {
        return request.header().header(Header::XAccessCode);
    }
    return accessCode;
}
// GEMREQ-end getAccessCode
// GEMREQ-start checkAccessCodeMatches
void TaskHandlerBase::checkAccessCodeMatches(const ServerRequest& request, const model::Task& task)
{
    auto accessCode = getAccessCode(request);
    A_20703.start("Set VAU-Error-Code header field to brute-force whenever AccessCode or Secret mismatches");
    VauExpect(accessCode == task.accessCode(), HttpStatus::Forbidden, VauErrorCode::brute_force,
              "AccessCode mismatch");
    A_20703.finish();
}
// GEMREQ-end checkAccessCodeMatches
