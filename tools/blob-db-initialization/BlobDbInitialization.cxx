#include "tools/blob-db-initialization/BlobDbInitialization.hxx"

#include "erp/client/HttpsClient.hxx"
#include "erp/common/Constants.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/GLog.hxx"
#include "mock/enrolment/MockEnrolmentManager.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/hsm/MockBlobDatabase.hxx"

#include <iostream>
#include <unordered_set>


void usage (const char* argv0, const std::string message = "")
{
    if ( ! message.empty())
        std::cerr << "ERROR: " << message << "\n\n";

    std::cout << "Usage: " << argv0 << " {options}+ {blob-type}+\n";
    std::cout << R"(
Upload HSM blobs via the enrollment API. Blobs can either be created dynamically in an attestation
sequence (attestation and endorsement key, quote) or loaded from files that contain static data
(for all other blob types).

Options:
    -c|--certificate <certificate-file>   file containing the cacertecc.crt certificate
    -h|--host <host-name>                 host of the enrolment API, defaults to "localhost"
    -p|--port <port-number>               port number of the enrolment API, defaults to 9191
    -s|--static <directory>               directory that contains static data to upload
    -d|--delete                           delete a blob before a new one is stored
    --dev                                 use static data for the DEV environment
    --ru                                  use static data for the RU environment


Blob types:
    all              all blobs
    dynamic          all dynamic blobs (knowAk, knownEk, knownQuote)
    static           all static blobs (all except knowAk, knownEk, knownQuote)
    ak|knownAk
    ek|knownEk
    quote|knownQuote
    ecies|eciesKeypair
    task|taskDerivationKey
    comm|communicationDerivationKey
    audit|auditLogDerivationKey
    kvnr|kvnrHashKey
    tid|telematikIdHashKey
    vausig|vauSigPrivateKey
)";

    if (message.empty())
        exit(EXIT_SUCCESS);
    else
        exit(EXIT_FAILURE);
}

enum class TargetEnvironment
{
    DEV,
    RU
};

struct CommandLineArguments
{
    std::string certificateFilename;
    std::string hostname;
    std::string staticDirectory;
    bool deleteBeforeStore = false;
    uint16_t portnumber;
    std::unordered_set<BlobType> blobTypes;
    TargetEnvironment environment = TargetEnvironment::DEV;
    bool hasBlobType (const BlobType type) const {return blobTypes.find(type)!=blobTypes.end();}
};


struct BlobDescriptor
{
    BlobType type;
    std::string shortName;
    std::string longName;
    std::string endpoint;
    std::string staticFilename;
    std::string staticFilenameRu;
    bool isDynamic;
    bool hasExpiry;
    bool hasStartEnd;
};


static std::vector<BlobDescriptor> blobDescriptors = {
    {BlobType::EndorsementKey,             "ek",     "knownEk",                    "KnownEndorsementKey",         "trustedEKSaved.blob",      "trustedEKSaved.blob",          true,  false, false},
    {BlobType::AttestationPublicKey,       "ak",     "knownAk",                    "KnownAttestationKey",         "AKPub.bin",                "AKPub.bin",                    true,  false, false},
    {BlobType::AttestationKeyPair,         "akpair", "knownAkKeyPair",             "",                            "",                         "",                             false, false, false},
    {BlobType::Quote,                      "quote",  "knownQuote",                 "KnownQuote",                  "trustedQuoteSaved.blob",   "trustedQuoteSaved.blob",       true,  false, false},
    {BlobType::EciesKeypair,               "ecies",  "eciesKeypair",               "EciesKeypair",                "ECIESKeyPairSaved.blob",   "eciesKeyPairSaved.blob",       false, true,  false},
    {BlobType::TaskKeyDerivation,          "task",   "taskDerivationKey",          "Task/DerivationKey",          "StaticDerivationKey.blob", "taskDerivationKeySaved.blob",  false, false, true },
    {BlobType::CommunicationKeyDerivation, "comm",   "communicationDerivationKey", "Communication/DerivationKey", "StaticDerivationKey.blob", "commDerivationKeySaved.blob",  false, false, true },
    {BlobType::AuditLogKeyDerivation,      "audit",  "auditLogDerivationKey",      "AuditLog/DerivationKey",      "StaticDerivationKey.blob", "auditDerivationKeySaved.blob", false, false, true },
    {BlobType::KvnrHashKey,                "kvnr",   "kvnrHashKey",                "KvnrHashKey",                 "HashKeySaved.blob",        "hashKeySaved.blob",            false, false, false},
    {BlobType::TelematikIdHashKey,         "tid",    "telematikIdHashKey",         "TelematikIdHashKey",          "HashKeySaved.blob",        "hashKeySaved.blob",            false, false, false},
    {BlobType::VauSigPrivateKey,           "vausig", "vauSigPrivateKey",           "VauSigPrivateKey",            "VAUSIGKeyPairSaved.blob",  "vausigKeyPairSaved.blob",      false, false, false}
};


void expectArgument (const std::string name, const int index, const int argc, const char* argv[])
{
    if (index+1 >= argc)
        usage(argv[0], "option " + name + " has no argument");
    else if (argv[index+1] == nullptr)
        usage(argv[0], "option " + name + " has no argument");
    else if (argv[index+1][0] == '-')
        usage(argv[0], "option " + name + " has no argument");
}


CommandLineArguments processCommandLine (const int argc, const char* argv[])
{
    CommandLineArguments arguments;
    arguments.hostname = Environment::get("ERP_SERVER_HOST").value_or("localhost");
    auto portnumber = Environment::get("ERP_ENROLMENT_SERVER_PORT").value_or("9191");
    size_t pos = 0;
    arguments.portnumber = gsl::narrow<uint16_t>(std::stoi(portnumber, &pos));
    Expect(pos == portnumber.size(), "Invalid portnumber: " + portnumber);

    for (int index=1; index<argc; ++index)
    {
        if (argv[index] == nullptr)
            usage(argv[0], "invalid option");
        const std::string argument = argv[index];
        if (argument.empty())
            usage(argv[0], "invalid option");

        if (argument == "-c" || argument == "--certificate")
        {
            expectArgument("-c|--certificate", index, argc, argv);
            arguments.certificateFilename = argv[++index];
        }
        else if (argument == "-h" || argument == "--host")
        {
            expectArgument("-h|--host", index, argc, argv);
            arguments.hostname = argv[++index];
        }
        else if (argument == "-p" || argument == "--port")
        {
            expectArgument("-p|--port", index, argc, argv);
            arguments.portnumber = gsl::narrow<uint16_t>(std::stoi(argv[++index]));
        }
        else if (argument == "-s" || argument == "--static")
        {
            expectArgument("-s|--static", index, argc, argv);
            arguments.staticDirectory = argv[++index];
        }
        else if (argument == "-d" || argument == "--delete")
        {
            arguments.deleteBeforeStore = true;
        }
        else if (argument == "--dev")
        {
            arguments.environment = TargetEnvironment::DEV;
        }
        else if (argument == "--ru")
        {
            arguments.environment = TargetEnvironment::RU;
        }
        else if (argument[0] == '-')
        {
            usage(argv[0], "unknown option "+argument);
        }
        else
        {
            // Process blob type.
            if (argument == "all")
                arguments.blobTypes.insert({
                    BlobType::EndorsementKey,
                    BlobType::AttestationPublicKey,
                    BlobType::Quote,
                    BlobType::EciesKeypair,
                    BlobType::TaskKeyDerivation,
                    BlobType::CommunicationKeyDerivation,
                    BlobType::AuditLogKeyDerivation,
                    BlobType::KvnrHashKey,
                    BlobType::TelematikIdHashKey,
                    BlobType::VauSigPrivateKey});
            else if (argument == "static")
                arguments.blobTypes.insert({
                    BlobType::EciesKeypair,
                    BlobType::TaskKeyDerivation,
                    BlobType::CommunicationKeyDerivation,
                    BlobType::AuditLogKeyDerivation,
                    BlobType::KvnrHashKey,
                    BlobType::TelematikIdHashKey,
                    BlobType::VauSigPrivateKey});
            else if (argument == "dynamic")
                arguments.blobTypes.insert({
                    BlobType::EndorsementKey,
                    BlobType::AttestationPublicKey,
                    BlobType::Quote});
            else
            {
                bool found = false;
                for (const auto descriptor : blobDescriptors)
                    if (argument==descriptor.shortName || argument==descriptor.longName)
                    {
                        arguments.blobTypes.insert(descriptor.type);
                        found = true;
                        break;
                    }
                if ( ! found)
                    usage(argv[0], "unknown blob type " + argument);
            }
        }
    }

    if (arguments.certificateFilename.empty())
    {
        if (arguments.staticDirectory.empty())
            usage(argv[0], "certificate file missing");
        else
        {
            arguments.certificateFilename = arguments.staticDirectory;
            if ( ! String::ends_with(arguments.certificateFilename, "/"))
                arguments.certificateFilename += "/";
            arguments.certificateFilename += "cacertecc.crt";
            std::cout << "using " << arguments.certificateFilename << " as certificate filename\n";
        }
    }
    if (arguments.blobTypes.empty())
        usage(argv[0], "no blob type specified for upload");

    return arguments;
}


const BlobDescriptor& getDescriptor (const BlobType type)
{
    return *std::find_if(
        blobDescriptors.begin(),
        blobDescriptors.end(),
        [type](const auto& descriptor){return descriptor.type == type;});
}


Header createHeader (
    const HttpMethod method,
    std::string&& target)
{
    return Header(
        method,
        std::move(target),
        Header::Version_1_1,
        {
            {Header::Authorization, "Basic " + Configuration::instance().getStringValue(ConfigurationKey::ENROLMENT_API_CREDENTIALS)}
        },
        HttpStatus::Unknown,
        false);
}


std::string createPutBody (
    const std::string_view id,
    const std::string_view data,
    const size_t generation,
    const bool hasExpiry,
    const bool hasStartEnd)
{
    std::string request = R"({"id":")"
           + Base64::encode(id)
           + R"(","blob":{"data":")"
           + Base64::encode(data)
           + R"(","generation":)"
           + std::to_string(generation)
           + R"(})";
    if (hasExpiry || hasStartEnd)
    {
        request += R"(,"metadata":{)";

        const auto now = std::chrono::system_clock::now();
        const auto endOffset = std::chrono::hours(24 * 365);
        if (hasExpiry)
        {
            request += R"("expiryDateTime":)" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>((now + endOffset).time_since_epoch()).count());
        }
        else
        {
            request +=  R"("startDateTime":)" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>((now).time_since_epoch()).count());
            request +=  R"(,"endDateTime":)" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>((now + endOffset).time_since_epoch()).count());
        }
        request += "}";
    }
    request += "}";
    return request;
}


std::string createDeleteBody (
    const std::string_view id)
{
    return R"({"id":")"
           + Base64::encode(id)
           + R"("})";
}


void deleteBlob (HttpsClient& client, const BlobDescriptor& descriptor, const std::string_view& name)
{
    std::cout << "deleting " << descriptor.endpoint << std::endl;
    client.send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/" + descriptor.endpoint),
            createDeleteBody(name)));
}


void deleteBlob (HttpsClient& client, const BlobDescriptor& descriptor)
{
    deleteBlob(client, descriptor, descriptor.shortName);
}


void storeBlob (
    HttpsClient& client,
    const BlobDescriptor& descriptor,
    const std::string_view& name,
    const std::string_view& data,
    const size_t generation)
{
    std::cout << "uploading " << descriptor.endpoint << std::endl;
    auto response = client.send(
        ClientRequest(
            createHeader(HttpMethod::PUT, "/Enrolment/" + descriptor.endpoint),
            createPutBody(
                name,
                data,
                generation,
                descriptor.hasExpiry,
                descriptor.hasStartEnd)));
    if (response.getHeader().status() != HttpStatus::Created)
        std::cerr << "    upload of " + descriptor.endpoint + " failed with " << toNumericalValue(response.getHeader().status()) << " " << response.getHeader().status() << std::endl;
}


void storeBlob (
    HttpsClient& client,
    const BlobDescriptor& descriptor,
    const std::string_view& data,
    const size_t generation)
{
    storeBlob(client, descriptor, descriptor.shortName, data, generation);
}


ErpBlob readStaticData (const BlobDescriptor& descriptor, const CommandLineArguments& arguments)
{
    std::string path = arguments.staticDirectory;
    // TODO: check if static blob types are to be uploaded and if directory is specified where the command line arguments are parsed.
    if (path.empty())
        usage("", "directory for static data has not been specified");
    if (path[path.size()-1] != '/')
        path += '/';
    switch (arguments.environment)
    {
        case TargetEnvironment::DEV: path += descriptor.staticFilename;   break;
        case TargetEnvironment::RU:  path += descriptor.staticFilenameRu; break;
    }
    const auto content = FileHelper::readFileAsString(path);
    return ErpBlob::fromCDump(Base64::encode(content));
}


int main (const int argc, const char* argv[])
{
    GLogConfiguration::init_logging(argv[0]);
    ThreadNames::instance().setThreadName(std::this_thread::get_id(), "main");

    const auto arguments = processCommandLine(argc, argv);
    Environment::set("ERP_SERVER_HOST", arguments.hostname);
    Environment::set("ERP_ENROLMENT_SERVER_PORT", std::to_string(arguments.portnumber));

    std::cerr << "will read certificate from " << arguments.certificateFilename
              << " and write blobs via enrolment API at " << arguments.hostname << ":" << arguments.portnumber
              << std::endl;

    const bool hasDynamicBlobTypes = arguments.hasBlobType(BlobType::AttestationPublicKey)
                                  || arguments.hasBlobType(BlobType::EndorsementKey)
                                  || arguments.hasBlobType(BlobType::Quote);
    EnrolmentHelper::Blobs dynamicBlobs;
    if (hasDynamicBlobTypes)
    {
        // Run the attestation sequence to compute blobs for known attestation key, known endorsement key and known quote.
        auto blobCache = std::make_shared<BlobCache>(std::make_unique<MockBlobDatabase>()); // We have to provide a blob cache as argument but it should not be used.
        MockEnrolmentManager enrolmentManager;
        dynamicBlobs = enrolmentManager.createAndReturnAkEkAndQuoteBlob(
            *blobCache,
            arguments.certificateFilename,
            2);
    }

    HttpsClient client (arguments.hostname, arguments.portnumber, Constants::httpTimeoutInSeconds, false);

    // The "dynamic" blobs require special handling that can not easily be expressed in the BlobDescriptor.
    if (arguments.hasBlobType(BlobType::EndorsementKey))
    {
        const auto descriptor = getDescriptor(BlobType::EndorsementKey);
        if (arguments.deleteBeforeStore)
            deleteBlob(client, descriptor);
        storeBlob(client, descriptor, dynamicBlobs.trustedEk.data, dynamicBlobs.trustedEk.generation);
    }

    if (arguments.hasBlobType(BlobType::AttestationPublicKey))
    {
        const auto descriptor = getDescriptor(BlobType::AttestationPublicKey);
        const auto akname = std::string_view(reinterpret_cast<const char*>(dynamicBlobs.akName.data()), dynamicBlobs.akName.size());
        if (arguments.deleteBeforeStore)
            deleteBlob(client, descriptor, akname);
        storeBlob(client, descriptor, akname, dynamicBlobs.trustedAk.data, dynamicBlobs.trustedAk.generation);
    }

    if (arguments.hasBlobType(BlobType::Quote))
    {
        auto descriptor = getDescriptor(BlobType::Quote);

        // Quote blobs are stored per version (including the build tyoe).
        // To avoid name conflicts we append the build type to the name.
        // Alternative we could create a "real" name by calculating the SHA2 but that would make deletion of the blobs more difficult.
        if (descriptor.type == BlobType::Quote)
            descriptor.shortName += "-" + std::string(ErpServerInfo::BuildType);

        if (arguments.deleteBeforeStore)
            deleteBlob(client, descriptor);
        storeBlob(client, descriptor, dynamicBlobs.trustedQuote.data, dynamicBlobs.trustedQuote.generation);
    }

    // The handling of the "static" blobs is easier and does not require special handling that could not be expressed by BlobDescriptor.
    for (const auto type : arguments.blobTypes)
    {
        // Lookup descriptor by type.
        const auto descriptor = std::find_if(blobDescriptors.begin(), blobDescriptors.end(), [type](const auto& item){return item.type==type;});
        Expect(descriptor != blobDescriptors.end(), "blob type not handled");

        if (descriptor->isDynamic)
            continue; // dynamic types have already been handled.

        if (arguments.deleteBeforeStore)
            deleteBlob(client, *descriptor);
        const auto blob = readStaticData(*descriptor, arguments);
        storeBlob(client, *descriptor, blob.data, blob.generation);
    }

    return EXIT_SUCCESS;
}
