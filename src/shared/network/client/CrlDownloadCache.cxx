/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/CrlDownloadCache.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/tsl/X509Certificate.hxx"
#include "shared/util/Expect.hxx"

#include <openssl/asn1.h>
#include <set>

CrlDownloadCache::CrlDownloadCache(std::shared_ptr<UrlRequestSender> requestSender)
    : mRequestSender{std::move(requestSender)}
{
}

shared_X509_CRL CrlDownloadCache::getCrl(const X509Certificate& cert)
{
    const auto crlUrls = cert.getHttpCrlDistributionPoints();

    if (crlUrls.empty())
    {
        return shared_X509_CRL::make(nullptr);
    }

    // try to find the item in our cache
    {
        std::scoped_lock lock{mCacheMutex};
        for (const auto& url : crlUrls)
        {
            // we might download a delta instead of the full crl
            if (auto entry = mCrlCache.find(url);
                entry != mCrlCache.end() && entry->second.nextUpdate > std::chrono::system_clock::now())
            {
                return entry->second.x509Crl;
            }
        }
    }

    // in case we dont find it, download the crl and add it to the cache
    auto urlAndCrlData = downloadCrl(crlUrls);

    std::scoped_lock lock{mCacheMutex};
    auto item = mCrlCache.insert_or_assign(urlAndCrlData.first, std::move(urlAndCrlData.second));
    return item.first->second.x509Crl;
}

std::pair<std::string, CrlDownloadCache::CrlData> CrlDownloadCache::downloadCrl(const std::vector<std::string>& urls)
{
    for (const auto& url : urls)
    {
        try
        {
            const auto response = mRequestSender->send(url, HttpMethod::GET, "", {}, std::nullopt, false);
            if (response.getHeader().status() == HttpStatus::OK)
            {
                const auto& body = stringToBio(response.getBody());
                // rfc5280:
                // When the HTTP or FTP URI scheme is used, the
                // URI MUST point to a single DER encoded CRL as specified in
                // [RFC2585]
                auto crlPtr = shared_X509_CRL::make(d2i_X509_CRL_bio(body.get(), nullptr));
                Expect(crlPtr != nullptr, "not a crl");
                const auto* asnNextUpdate = X509_CRL_get0_nextUpdate(crlPtr);
                struct tm tm{};
                OpenSslExpect(1 == ASN1_TIME_to_tm(asnNextUpdate, &tm), "Unable to convert ASN1 time");
                auto nextUpdate = model::Timestamp::fromTmInUtc(tm).toChronoTimePoint();
                // rfc5280, 6.3.3, a.1.ii (skip use-deltas)
                Expect(nextUpdate > std::chrono::system_clock::now(), "Outdated CRL");
                return std::make_pair(url, CrlData{.x509Crl = std::move(crlPtr), .nextUpdate = nextUpdate});
            }

            TLOG(WARNING) << "Failed calling URI " << url << " with HTTP Response: " << response.getHeader().status();
        }
        catch (const std::runtime_error& e)
        {
            TLOG(ERROR) << "Can not download crl from " << url << ", error: " << e.what();
        }
    }
    Fail("unable to get any crl");
}
