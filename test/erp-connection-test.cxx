/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/client/UrlRequestSender.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/GLog.hxx"
#include "erp/util/GLogConfiguration.hxx"


int main (int argc, char** argv)
{
    Environment::set("ERP_VLOG_MAX_VALUE", "4");

    GLogConfiguration::init_logging(argv[0]);

    if (argc >= 2 && argc <= 3)
    {
        try
        {
            std::string certificates;
            if (argc == 3)
            {
                const std::string path = argv[2];
                if(!path.empty())
                {
                    Expect(FileHelper::exists(path), "CA file must exist");

                    certificates = FileHelper::readFileAsString(path);
                }
                LOG(INFO) << "Using file with certificates:\n" << certificates << "\n\n";
            }
            else
            {
                LOG(INFO) << "Using no certificates";
            }

            UrlRequestSender requestSender(SafeString{std::move(certificates)}, 30/*connectionTimeoutSeconds*/);

            ClientResponse response = requestSender.send(
                argv[1],
                HttpMethod::GET,
                "",
                {},
                "DHE-RSA-AES256-SHA:DHE-RSA-AES128-GCM-SHA256",
                true);

            LOG(INFO) << "Response status: " << response.getHeader().status();
            LOG(INFO) << "Response body:\n[\n" << response.getBody() << "\n]\n\n";
        }
        catch(const std::exception& e)
        {
            LOG(ERROR) << "exception by request sending: " << e.what();
        }
    }
    else
    {
        LOG(ERROR) << "following parameters are expected:\n"
                    << "erp-connection-test <url> [<path to pem-file with certificates>]";
    }
}
