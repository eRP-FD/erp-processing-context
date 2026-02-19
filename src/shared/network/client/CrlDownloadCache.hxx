/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/network/client/CrlProvider.hxx"
#include "shared/network/client/UrlRequestSender.hxx"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class X509Certificate;

class CrlDownloadCache : public CrlProvider
{
public:
    CrlDownloadCache(std::shared_ptr<UrlRequestSender> requestSender);
    ~CrlDownloadCache() override = default;
    shared_X509_CRL getCrl(const X509Certificate& cert) override;

private:
    struct CrlData {
        shared_X509_CRL x509Crl;
        std::chrono::system_clock::time_point nextUpdate;
    };

    std::pair<std::string, CrlData> downloadCrl(const std::vector<std::string>& urls);

    std::mutex mCacheMutex;
    std::unordered_map<std::string, CrlData> mCrlCache;

    std::shared_ptr<UrlRequestSender> mRequestSender;
};
