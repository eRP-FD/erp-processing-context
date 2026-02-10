/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "ErpFhirWS.hxx"

#include "shared/fhir/Fhir.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/GLogConfiguration.hxx"

#include <boost/asio/io_context.hpp>
#include <boost/asio/require.hpp>
#include <boost/asio/execution/outstanding_work.hpp>
#include <memory>

int main(int argc, char** argv)
{
    Expect(argc >= 1, "missing executable path in commandline arguments");
    std::filesystem::current_path(std::filesystem::weakly_canonical(argv[0]).parent_path());
    GLogConfiguration::initLogging("erp-fhir-ws");
    if (!Environment::get(Configuration::ServerHostEnvVar))
    {
        Environment::set(Configuration::ServerHostEnvVar, "127.0.0.1");
    }
    if (!Environment::get(Configuration::ServerPortEnvVar))
    {
        Environment::set(Configuration::ServerPortEnvVar, "8085");
    }
    Environment::set("TEST_ADDITIONAL_XML_SCHEMAS", "");
    Fhir::init<Configuration::ERP>(Fhir::Init::now);
    boost::asio::io_context ctx;
    auto erpFhirWS = erp_fhir_ws::ErpFhirWS::create(ctx.get_executor());
    erpFhirWS->start(1);
    ctx.run();
}
