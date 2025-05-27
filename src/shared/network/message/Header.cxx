/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/message/Header.hxx"

#include "shared/util/Expect.hxx"
#include "shared/util/UrlHelper.hxx"

#include <boost/algorithm/string.hpp>
#include <sstream>

const std::string Header::Accept = "Accept";
const std::string Header::AcceptLanguage = "Accept-Language";
const std::string Header::Authorization = "Authorization";
const std::string Header::Connection = "Connection";
const std::string Header::ContentEncoding = "Content-Encoding";
const std::string Header::ContentLength = "Content-Length";
const std::string Header::ContentType = "Content-Type";
const std::string Header::ContextStatus = "IBM-EPA-ContextStatus";
const std::string Header::ExternalInterface = "External-Interface";
const std::string Header::Host = "Host";
const std::string Header::KeepAlive = "Keep-Alive";
const std::string Header::InnerRequestOperation = "Inner-Request-Operation";
const std::string Header::InnerRequestRole = "Inner-Request-Role";
const std::string Header::InnerRequestFlowtype = "Inner-Request-Flowtype";
const std::string Header::InnerRequestClientId = "Inner-Request-Client-Id";
const std::string Header::InnerRequestLeips = "Inner-Request-Leips";
const std::string Header::InnerResponseCode = "Inner-Response-Code";
const std::string Header::BackendDurationMs = "Backend-Duration-Ms";
const std::string Header::MvoNumber = "Mvo-Number";
const std::string Header::ANR = "LANR";
const std::string Header::ZANR = "ZANR";
const std::string Header::PrescriptionId = "Prescription-ID";
const std::string Header::DigaRedeemCode = "Diga-Redeem-Code";
const std::string Header::GematikWorkflowProfil = "Gematik-Workflow-Profil";
const std::string Header::GematikPatientenrechnung = "Gematik-Patientenrechnung";
const std::string Header::KbvVerordnungsdaten = "KBV-Verordnungsdaten";
const std::string Header::DavAbgabedaten = "DAV-Abgabedaten";

const std::string Header::Tee3::VauCid = "VAU-CID";
const std::string Header::Tee3::XUserAgent = "x-useragent";
const std::string Header::Tee3::XInsurantId = "x-insurantid";
const std::string Header::Tee3::VauNP = "VAU-NP";
const std::string Header::Tee3::VauNonPuTracing = "VAU-nonPU-Tracing";

const std::string Header::Location = "Location";
const std::string Header::Server = "Server";
const std::string Header::Session = "Session";
const std::string Header::Signature = "signature";
const std::string Header::SigningCertificate = "certificate";
const std::string Header::Timestamp = "timestamp";
const std::string Header::TransferEncoding = "Transfer-Encoding";
const std::string Header::UserAgent = "User-Agent";
const std::string Header::VAUErrorCode = "VAU-Error-Code";
const std::string Header::WWWAuthenticate = "WWW-Authenticate";
const std::string Header::Warning = "Warning";
const std::string Header::XAccessCode = "X-AccessCode";
const std::string Header::XRequestId = "X-Request-Id";

const std::string Header::ConnectionClose = "close";
const std::string Header::ConnectionKeepAlive = "keep-alive";

// EPA Headers
const std::string Header::Epa::ClusterAddress = "IBM-EPA-tee-cluster-address";
const std::string Header::Epa::PodAddress = "IBM-EPA-tee-pod-address";


namespace {
    constexpr const char* headerFieldDelimiter = "\r\n";

    size_t convertContentLength (const Header::keyValueMap_t& fields)
    {
        auto field = fields.find(Header::ContentLength);
        if (field != fields.end())
            return std::stoull(std::string(field->second));
        else
            return 0;
    }
}


Header::Header (void)
    : Header(HttpStatus::Unknown)
{
}


Header::Header (HttpStatus statusCode)
    : Header({}, {}, Header::Version_1_1, {}, statusCode)
{
}


Header::Header(
    HttpMethod method,
    std::string&& target,
    unsigned int version,
    keyValueMap_t&& header,
    HttpStatus statusCode)
    : mMethod(method),
      mTarget(std::move(target)),
      mVersion(version),
      mHeader(std::move(header)),
      mStatusCode(statusCode)
{
    auto acceptHeader = this->header(Accept);
    if (acceptHeader)
    {
        parseAcceptField(acceptHeader.value());
    }
}


HttpMethod Header::method (void) const
{
    return mMethod;
}


void Header::setMethod (const HttpMethod method)
{
    mMethod = method;
}


const std::string& Header::target (void) const
{
    return mTarget;
}


void Header::setTarget (const std::string& target)
{
    mTarget = target;
}


unsigned int Header::version (void) const
{
    return mVersion;
}


void Header::setVersion (const unsigned int version)
{
    mVersion = version;
}


std::optional<const std::string> Header::header (const std::string& key) const
{
    auto header = mHeader.find(key);
    if (header != mHeader.end())
        return header->second;
    else
    {
        return std::nullopt;
    }
}


bool Header::hasHeader (const std::string& key) const
{
    return mHeader.find(key) != mHeader.end();
}


size_t Header::contentLength (void) const
{
    return convertContentLength(mHeader);
}


std::optional<const std::string> Header::contentType () const
{
    return header(ContentType);
}


HttpStatus Header::status (void) const
{
    return mStatusCode;
}


void Header::setStatus (HttpStatus status)
{
    mStatusCode = status;
}


bool Header::keepAlive (void) const
{
    auto connectionHeader{header(Connection)};
    if (connectionHeader)
    {
        return boost::iequals(*connectionHeader, KeepAlive);
    }
    return  version() >= Version_1_1;
}

void Header::setKeepAlive(bool keepAlive)
{
    auto connectionHeader = mHeader.find(Header::Connection);
    if (connectionHeader != mHeader.end())
    {
        connectionHeader->second = keepAlive?KeepAlive:ConnectionClose;
    }
    else if (keepAlive && version() <= Version_1_0)
    {
        mHeader[Header::Connection] = Header::KeepAlive;
    }
    else if (!keepAlive && version() >= Version_1_1)
    {
        mHeader[Header::Connection] = Header::ConnectionClose;
    }
}


std::optional<AcceptMimeType> Header::getAcceptMimeType(std::string_view mimeType) const
{
    const auto found =
        std::ranges::find_if(mAcceptMimeTypes,
                     [mimeType](const auto& m) { return m.getMimeType() == mimeType; });

    if (found != mAcceptMimeTypes.cend())
        return std::optional<AcceptMimeType>{*found};
    return std::nullopt;
}


const Header::keyValueMap_t& Header::headers (void) const
{
    return mHeader;
}


std::string Header::serializeFields (void) const
{
    std::ostringstream s (std::ios_base::binary);
    for (auto const& header : mHeader)
        s << header.first << ": " << header.second << headerFieldDelimiter;
    return s.str();
}


void Header::addHeaderFields (const std::unordered_map<std::string, std::string>& fields)
{
    for (const auto& field : fields)
        addHeaderField(field.first, field.second);
}


void Header::addHeaderField (const std::string& key, const std::string& value)
{
    mHeader.insert_or_assign(key, value);
    if (key == Accept)
    {
        parseAcceptField(value);
    }
}


void Header::removeHeaderField (const std::string& key)
{
    mHeader.erase(key);
    if (key == Accept)
    {
        mAcceptMimeTypes.clear();
    }
}


namespace
{
void checkInvariantsUnknown(size_t numericStatusCode, bool hasContentLength, bool hasTransferEncoding)
{
    Expect3(! hasContentLength || (numericStatusCode >= 200 && numericStatusCode != 204),
        "header must not contain " + Header::ContentLength + " for status codes 1xx or 204",
        std::logic_error);
    Expect3(! hasTransferEncoding || (numericStatusCode >= 200 && numericStatusCode != 204),
        "header must not contain " + Header::TransferEncoding + " for status codes 1xx or 204",
        std::logic_error);
}
void checkInvariantsGetHead(bool hasContentLength, bool hasTransferEncoding)
{
    Expect3(! hasContentLength && ! hasTransferEncoding,
        "header must not contain " + Header::ContentLength + " or " + Header::TransferEncoding +
            " for DELETE, GET, HEAD",
        std::logic_error);
}
void checkInvariantsDeletePatchPostPut(bool hasContentLength, bool hasTransferEncoding)
{
    Expect3(hasContentLength || hasTransferEncoding,
        "header must contain " + Header::ContentLength + " or " + Header::TransferEncoding +
            " for PATCH, POST, PUT",
        std::logic_error);
}
}


void Header::checkInvariants (void) const
{
    const bool hasContentLength = hasHeader(Header::ContentLength);
    const bool hasTransferEncoding = hasHeader(Header::TransferEncoding);

    Expect3(!(hasContentLength && hasTransferEncoding),
            "header must not contain both " + Header::ContentLength  + " and " +  Header::TransferEncoding,
            std::logic_error);

    switch(mMethod)
    {
        case HttpMethod::UNKNOWN: {
            checkInvariantsUnknown(static_cast<size_t>(mStatusCode), hasContentLength, hasTransferEncoding);
            break;
        }
        case HttpMethod::GET:
        case HttpMethod::HEAD:
            checkInvariantsGetHead(hasContentLength, hasTransferEncoding);
            break;
        case HttpMethod::DELETE:
        case HttpMethod::PATCH:
        case HttpMethod::POST:
        case HttpMethod::PUT:
            checkInvariantsDeletePatchPostPut(hasContentLength, hasTransferEncoding);
            break;
    }

    // CONNECT method is currently not supported (has no value in HttpMethod).
    // Therefore checking CONNECT and status 2xx is not necessary and not possible.
}


size_t StringHashCaseInsensitive::operator() (const std::string& s) const
{
    return std::hash<std::string>()(String::toLower(s));
}


bool StringComparatorCaseInsensitive::operator() (const std::string& a, const std::string& b) const
{
    return String::toLower(a) == String::toLower(b);
}

void Header::parseAcceptField(std::string_view acceptField)
{
    const auto splitByComma = String::split(acceptField, ',');
    Expect3(!splitByComma.empty(), "String::split function defect", std::logic_error);
    mAcceptMimeTypes.reserve(splitByComma.size());
    for (const auto& entry : splitByComma)
    {
        AcceptMimeType mimeType(entry);
        // q-factor==0 means not acceptable
        if (mimeType.getQFactorWeight() > 0)
        {
            mAcceptMimeTypes.emplace_back(std::move(mimeType));
        }
    }
}

void Header::setContentLength(size_t contentLength)
{
    if (contentLength > 0)
    {
        addHeaderField(Header::ContentLength, std::to_string(contentLength));
        return;
    }

    // handle when to set Content-Length: 0 and when to delete Content-Length:
    switch (mMethod)
    {
        case HttpMethod::UNKNOWN: {
            // response
            setContentLengthZeroMethodUnknown();
            return;
        }
        case HttpMethod::GET:
        case HttpMethod::HEAD:
            removeHeaderField(Header::ContentLength);
            return;
        case HttpMethod::DELETE:
        case HttpMethod::PATCH:
        case HttpMethod::POST:
        case HttpMethod::PUT:
            addHeaderField(Header::ContentLength, "0");
            return;
    }
    LogicErrorFail("uninitialized enum mHeader.method()");
}

std::string Header::profileVersionHeader(model::ProfileType profileType)
{
    switch (profileType)
    {
        case model::ProfileType::ActivateTaskParameters:
        case model::ProfileType::CreateTaskParameters:
        case model::ProfileType::GEM_ERP_PR_AuditEvent:
        case model::ProfileType::GEM_ERP_PR_Binary:
        case model::ProfileType::GEM_ERP_PR_Communication_DispReq:
        case model::ProfileType::GEM_ERP_PR_Communication_InfoReq:
        case model::ProfileType::GEM_ERP_PR_Communication_Reply:
        case model::ProfileType::GEM_ERP_PR_Communication_Representative:
        case model::ProfileType::GEM_ERP_PR_Composition:
        case model::ProfileType::GEM_ERP_PR_Device:
        case model::ProfileType::GEM_ERP_PR_Digest:
        case model::ProfileType::GEM_ERP_PR_Medication:
        case model::ProfileType::GEM_ERP_PR_MedicationDispense:
        case model::ProfileType::GEM_ERP_PR_MedicationDispense_DiGA:
        case model::ProfileType::GEM_ERP_PR_PAR_CloseOperation_Input:
        case model::ProfileType::GEM_ERP_PR_PAR_DispenseOperation_Input:
        case model::ProfileType::MedicationDispenseBundle:
        case model::ProfileType::GEM_ERP_PR_Bundle:
        case model::ProfileType::GEM_ERP_PR_Task:
            return GematikWorkflowProfil;
        case model::ProfileType::fhir:
        case model::ProfileType::Subscription:
        case model::ProfileType::OperationOutcome:
        case model::ProfileType::EPAOpRxPrescriptionERPOutputParameters:
        case model::ProfileType::EPAOpRxDispensationERPOutputParameters:
        case model::ProfileType::ProvidePrescriptionErpOp:
        case model::ProfileType::CancelPrescriptionErpOp:
        case model::ProfileType::ProvideDispensationErpOp:
        case model::ProfileType::OrganizationDirectory:
        case model::ProfileType::EPAMedicationPZNIngredient:
        case model::ProfileType::KBV_PR_EVDGA_HealthAppRequest:
            return {};
        case model::ProfileType::GEM_ERPCHRG_PR_ChargeItem:
        case model::ProfileType::GEM_ERPCHRG_PR_Consent:
        case model::ProfileType::GEM_ERPCHRG_PR_Communication_ChargChangeReq:
        case model::ProfileType::GEM_ERPCHRG_PR_Communication_ChargChangeReply:
        case model::ProfileType::PatchChargeItemParameters:
            return GematikPatientenrechnung;
        case model::ProfileType::KBV_PR_EVDGA_Bundle:
        case model::ProfileType::KBV_PR_ERP_Bundle:
        case model::ProfileType::KBV_PR_ERP_Composition:
        case model::ProfileType::KBV_PR_ERP_Medication_Compounding:
        case model::ProfileType::KBV_PR_ERP_Medication_FreeText:
        case model::ProfileType::KBV_PR_ERP_Medication_Ingredient:
        case model::ProfileType::KBV_PR_ERP_Medication_PZN:
        case model::ProfileType::KBV_PR_ERP_PracticeSupply:
        case model::ProfileType::KBV_PR_ERP_Prescription:
        case model::ProfileType::KBV_PR_FOR_Coverage:
        case model::ProfileType::KBV_PR_FOR_Organization:
        case model::ProfileType::KBV_PR_FOR_Patient:
        case model::ProfileType::KBV_PR_FOR_Practitioner:
        case model::ProfileType::KBV_PR_FOR_PractitionerRole:
            return KbvVerordnungsdaten;
        case model::ProfileType::DAV_PKV_PR_ERP_AbgabedatenBundle:
            return DavAbgabedaten;
    }
    return {};
}

void Header::setContentLengthZeroMethodUnknown()
{
    // no Content-Length field for Codes < 200 and 204
    const auto numericStatus = static_cast<size_t>(status());
    if (numericStatus >= 200 && numericStatus != 204)
    {
        addHeaderField(Header::ContentLength, "0");
    }
    else
    {
        removeHeaderField(Header::ContentLength);
    }
}
