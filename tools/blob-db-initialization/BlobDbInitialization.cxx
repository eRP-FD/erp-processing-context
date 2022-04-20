/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

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

#include <date/date.h>
#include <iostream>
#include <filesystem>
#include <unordered_set>
#include <magic_enum.hpp>

#include "tools/EnrolmentApiClient.hxx"

class DummyBlobDatabase : public BlobDatabase
{
    void deleteBlob(BlobType, const ErpVector &) override
    {
        Fail("DummyBlobDatabase::deleteBlob should not be called.");
    }
    std::vector<Entry> getAllBlobsSortedById() const override
    {
        Fail("DummyBlobDatabase::getAllBlobsSortedById should not be called.");
    }
    BlobDatabase::Entry getBlob(BlobType, BlobId) const override
    {
        Fail("DummyBlobDatabase::getBlob should not be called.");
    }
    std::vector<bool> hasValidBlobsOfType(std::vector<BlobType> &&) const override
    {
        Fail("DummyBlobDatabase::hasValidBlobsOfType should not be called.");
    }

    BlobId storeBlob(BlobDatabase::Entry &&) override
    {
        Fail("DummyBlobDatabase::storeBlob should not be called.");
    }
};

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
    -v|--version                          print the build version number

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
    vausig
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
    std::string staticFilename;
    std::string staticFilenameRu;
    bool isDynamic;
    bool hasBegin;
    bool hasEnd;
};

static std::vector<BlobDescriptor> blobDescriptors = {
    {BlobType::EndorsementKey,             "ek",     "knownEk",                    "trustedEKSaved.blob",      "trustedEKSaved.blob",          true,  false, false},
    {BlobType::AttestationPublicKey,       "ak",     "knownAk",                    "AKPub.bin",                "AKPub.bin",                    true,  false, false},
    {BlobType::AttestationKeyPair,         "akpair", "knownAkKeyPair",             "",                         "",                             false, false, false},
    {BlobType::Quote,                      "quote",  "knownQuote",                 "trustedQuoteSaved.blob",   "trustedQuoteSaved.blob",       true,  false, false},
    {BlobType::EciesKeypair,               "ecies",  "eciesKeypair",               "ECIESKeyPairSaved.blob",   "eciesKeyPairSaved.blob",       false, false, true },
    {BlobType::TaskKeyDerivation,          "task",   "taskDerivationKey",          "StaticDerivationKey.blob", "taskDerivationKeySaved.blob",  false, false, false},
    {BlobType::CommunicationKeyDerivation, "comm",   "communicationDerivationKey", "StaticDerivationKey.blob", "commDerivationKeySaved.blob",  false, false, false},
    {BlobType::AuditLogKeyDerivation,      "audit",  "auditLogDerivationKey",      "StaticDerivationKey.blob", "auditDerivationKeySaved.blob", false, false, false},
    {BlobType::KvnrHashKey,                "kvnr",   "kvnrHashKey",                "HashKeySaved.blob",        "hashKeySaved.blob",            false, false, false},
    {BlobType::TelematikIdHashKey,         "tid",    "telematikIdHashKey",         "HashKeySaved.blob",        "hashKeySaved.blob",            false, false, false},
    {BlobType::VauSig,                     "vausig", "vauSig",                     "VAUSIGKeyPairSaved.blob",  "vausigKeyPairSaved.blob",      false, false, false}};


void expectArgument (const std::string name, const int index, const int argc, const char* argv[])
{
    if (index+1 >= argc)
        usage(argv[0], "option " + name + " has no argument");
    else if (argv[index+1] == nullptr)
        usage(argv[0], "option " + name + " has no argument");
    else if (argv[index+1][0] == '-')
        usage(argv[0], "option " + name + " has no argument");
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
CommandLineArguments processCommandLine(const int argc,
                                        const char* argv[])// NOLINT(readability-function-cognitive-complexity)
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
        else if (argument == "-v" || argument == "--version")
        {
            ::std::cout << ::ErpServerInfo::BuildVersion << ::std::endl;
            exit(EXIT_SUCCESS);
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
                    BlobType::VauSig});
            else if (argument == "static")
                arguments.blobTypes.insert({
                    BlobType::EciesKeypair,
                    BlobType::TaskKeyDerivation,
                    BlobType::CommunicationKeyDerivation,
                    BlobType::AuditLogKeyDerivation,
                    BlobType::KvnrHashKey,
                    BlobType::TelematikIdHashKey,
                    BlobType::VauSig});
            else if (argument == "dynamic")
                arguments.blobTypes.insert({
                    BlobType::EndorsementKey,
                    BlobType::AttestationPublicKey,
                    BlobType::Quote});
            else
            {
                bool found = false;
                for (const auto& descriptor : blobDescriptors)
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

struct StaticData
{
    ErpBlob blob;
    ::std::optional<::std::string> certificate;
};

StaticData readStaticData (const BlobDescriptor& descriptor, const CommandLineArguments& arguments)
{
    ::std::filesystem::path path = arguments.staticDirectory;
    // TODO: check if static blob types are to be uploaded and if directory is specified where the command line arguments are parsed.
    if (path.empty())
        usage("", "directory for static data has not been specified");
    switch (arguments.environment)
    {
        case TargetEnvironment::DEV: path /= descriptor.staticFilename;   break;
        case TargetEnvironment::RU:  path /= descriptor.staticFilenameRu; break;
    }

    Expect(::FileHelper::exists(path), "No blob file found for " + ::std::string{::magic_enum::enum_name(descriptor.type)} + " (expected " + path.string() +")");

    StaticData result;
    result.blob = ErpBlob::fromCDump(Base64::encode(FileHelper::readFileAsString(path)));
    if(descriptor.type == BlobType::VauSig)
    {
        path.replace_extension(".pem");

        Expect(::FileHelper::exists(path), "No certificate file found for VauSig (expected " + path.string() +")");

        result.certificate = ::FileHelper::readFileAsString(path);
    }

    return result;
}

EnrolmentApiClient::ValidityPeriod createValidity(const BlobDescriptor& descriptor)
{
    const auto now = ::std::chrono::system_clock::now();
    const auto endOffset = ::std::chrono::hours{24 * 365};

    ::EnrolmentApiClient::ValidityPeriod validityPeriod;
    if (descriptor.hasBegin)
    {
        validityPeriod.begin = now;
    }

    if (descriptor.hasEnd)
    {
        validityPeriod.end = now + endOffset;
    }

    return validityPeriod;
}

class BlobDbInitializationClient : public EnrolmentApiClient {
public:
    using EnrolmentApiClient::EnrolmentApiClient;

    void enroll(const CommandLineArguments& arguments)
    {
        if (hasDynamicBlobTypes(arguments))
        {
            enrollDynamic(arguments);
        }
        enrollStatic(arguments);
    }

private:


    bool hasDynamicBlobTypes(const CommandLineArguments& arguments)
    {
        return arguments.hasBlobType(BlobType::AttestationPublicKey) ||
               arguments.hasBlobType(BlobType::EndorsementKey) ||
               arguments.hasBlobType(BlobType::Quote);
    }

    // The "dynamic" blobs require special handling that can not easily be expressed in the BlobDescriptor.
    void enrollDynamic(const CommandLineArguments& arguments)
    {
        // Run the attestation sequence to compute blobs for known attestation key, known endorsement key and known quote.
        auto blobCache = std::make_shared<BlobCache>(
            std::make_unique<
                DummyBlobDatabase>());// We have to provide a blob cache as argument but it should not be used.
        dynamicBlobs =
            MockEnrolmentManager::createAndReturnAkEkAndQuoteBlob(*blobCache, arguments.certificateFilename, 2);

        if (arguments.hasBlobType(BlobType::EndorsementKey))
        {
            enrollEndorsmentKey(arguments);
        }
        if (arguments.hasBlobType(BlobType::AttestationPublicKey))
        {
            enrollAttestationPublicKey(arguments);
        }
        if (arguments.hasBlobType(BlobType::Quote))
        {
            enrollQuote(arguments);
        }
    }

    // The handling of the "static" blobs is easier and does not require special handling that could not be expressed by BlobDescriptor.
    void enrollStatic(const CommandLineArguments& arguments)
    {
        for (const auto type : arguments.blobTypes)
        {
            enroll(arguments, type);
        }
    }

    void enrollEndorsmentKey(const CommandLineArguments& arguments)
    {

        // The "dynamic" blobs require special handling that can not easily be expressed in the BlobDescriptor.
        try
        {
            const auto& descriptor = getDescriptor(BlobType::EndorsementKey);
            if (arguments.deleteBeforeStore)
            {
                deleteBlob(descriptor.type, descriptor.shortName);
            }
            storeBlob(descriptor.type, descriptor.shortName, dynamicBlobs.trustedEk, createValidity(descriptor));
        }
        catch (const ::std::exception& exception)
        {
            ::std::cerr << "Error while enrolling EndorsementKey: " << exception.what() << ::std::endl;
        }
    }

    void enrollAttestationPublicKey(const CommandLineArguments& arguments)
    {
        try
        {
            const auto& descriptor = getDescriptor(BlobType::AttestationPublicKey);
            const auto akname =
                std::string_view(reinterpret_cast<const char*>(dynamicBlobs.akName.data()), dynamicBlobs.akName.size());
            if (arguments.deleteBeforeStore)
            {
                deleteBlob(descriptor.type, akname);
            }

            storeBlob(descriptor.type, akname, dynamicBlobs.trustedAk, createValidity(descriptor));
        }
        catch (const ::std::exception& exception)
        {
            ::std::cerr << "Error while enrolling AttestationPublicKey: " << exception.what() << ::std::endl;
        }
    }

    void enrollQuote(const CommandLineArguments& arguments)
    {
        try
        {
            auto descriptor = getDescriptor(BlobType::Quote);

            // Quote blobs are stored per version (including the build type).
            // To avoid name conflicts we append an uuid to the name. The former solution with appending build type
            // caused a problem with a second enrolment when a key with the same name already exists. Deletion is now not
            // possible anymore but it not critical for this sepcial use case to enrol via this helper.
            // Alternative we could create a "real" name by calculating the SHA2 but that would make deletion of the blobs more difficult.
            if (descriptor.type == BlobType::Quote)
            {
                descriptor.shortName += "-" + Uuid().toString();
            }

            if (arguments.deleteBeforeStore)
            {
                deleteBlob(descriptor.type, descriptor.shortName);
            }
            storeBlob(descriptor.type, descriptor.shortName, dynamicBlobs.trustedQuote, createValidity(descriptor));
        }
        catch (const ::std::exception& exception)
        {
            ::std::cerr << "Error while enrolling Quote: " << exception.what() << ::std::endl;
        }
    }

    void enroll(const CommandLineArguments& arguments, BlobType type)
    {
        try
        {
            // Lookup descriptor by type.
            const auto descriptor =
                std::find_if(blobDescriptors.begin(), blobDescriptors.end(), [type](const auto& item) {
                    return item.type == type;
                });
            Expect(descriptor != blobDescriptors.end(), "blob type not handled");

            if (descriptor->isDynamic)
                return;// dynamic types have already been handled.

            if (arguments.deleteBeforeStore)
            {
                deleteBlob(descriptor->type, descriptor->shortName);
            }

            const auto staticData = readStaticData(*descriptor, arguments);
            storeBlob(descriptor->type, descriptor->shortName, staticData.blob, createValidity(*descriptor),
                      staticData.certificate);
        }
        catch (const ::std::exception& exception)
        {
            ::std::cerr << "Error while enrolling " << ::magic_enum::enum_name(type) << ": " << exception.what()
                        << ::std::endl;
        }
    }

    EnrolmentHelper::Blobs dynamicBlobs;
};

int main(const int argc, const char* argv[])
{
    GLogConfiguration::init_logging(argv[0]);
    ThreadNames::instance().setThreadName(std::this_thread::get_id(), "main");

    const auto arguments = processCommandLine(argc, argv);
    Environment::set("ERP_SERVER_HOST", arguments.hostname);
    Environment::set("ERP_ENROLMENT_SERVER_PORT", std::to_string(arguments.portnumber));

    std::cerr << "will read certificate from " << arguments.certificateFilename
              << " and write blobs via enrolment API at " << arguments.hostname << ":" << arguments.portnumber
              << std::endl;


    BlobDbInitializationClient client{arguments.hostname, arguments.portnumber, ::gsl::narrow<uint16_t>(Constants::httpTimeoutInSeconds), false};
    client.enroll(arguments);

    return EXIT_SUCCESS;
}
