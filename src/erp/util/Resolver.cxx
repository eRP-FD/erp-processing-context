/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/Expect.hxx"
#include "erp/util/Resolver.hxx"

#include <netdb.h>

namespace ip = boost::asio::ip;

ip::basic_resolver_results<ip::tcp> Resolver::resolve(const std::string& host, const std::string& service,
                                                      std::chrono::milliseconds timeout)
{
    addrinfo hints{};
    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_addrlen = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_next = nullptr;

    gaicb gcb{};
    gcb.ar_name = host.c_str();
    gcb.ar_service = service.c_str();
    gcb.ar_request = &hints;

    gaicb* query = &gcb;

    auto ret = getaddrinfo_a(GAI_NOWAIT, &query, 1, nullptr);
    Expect(! ret, "getaddrinfo_a() failed");

    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(timeout);
    const auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(timeout) - secs;
    timespec ts{.tv_sec = secs.count(), .tv_nsec = nsecs.count()};
    gai_suspend(&query, 1, &ts);
    using AddrInfoPtr = std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>;

    AddrInfoPtr result{query->ar_result, &freeaddrinfo};
    auto queryErr = gai_error(query);
    Expect(queryErr == 0, std::string{"Failed resolving endpoint: "}.append(gai_strerror(queryErr)));

    ip::basic_resolver_results<ip::tcp> resolverResult =
        ip::basic_resolver_results<ip::tcp>::create(result.get(), host, service);

    return resolverResult;
}
