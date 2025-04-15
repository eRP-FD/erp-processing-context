/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASE_REDISWRAPPER_HXX
#define ERP_PROCESSING_CONTEXT_DATABASE_REDISWRAPPER_HXX

#include <sw/redis++/redis++.h>

#include "erp/service/RedisInterface.hxx"
#include "shared/util/Configuration.hxx"
#include <chrono>

class RedisClient : public RedisInterface
{
public:
    explicit RedisClient();
    explicit RedisClient(std::chrono::milliseconds socketTimeout);
    ~RedisClient() override = default;

    void healthCheck() override;
    bool exists(const std::string_view& key) override;
    std::optional<std::string> fieldValueForKey(const std::string_view& key,
                                                const std::string_view& field) override;
    bool hasKeyWithField(const std::string_view& key, const std::string_view& field) override;
    void setKeyFieldValue(const std::string_view& key, const std::string_view& field,
                          const std::string_view& value) override;
    void
    setKeyExpireAt(const std::string_view& key,
                   const std::chrono::time_point<std::chrono::system_clock,
                                                 std::chrono::milliseconds>& timestamp) override;
    int64_t incr(const std::string_view& key) override;
    void publish(const std::string_view& channel, const std::string_view& message) override;
    int64_t getIntValue(std::string_view key) override;
    void flushdb() override;
private:
    std::unique_ptr<sw::redis::Redis> mConnection;
    sw::redis::ConnectionOptions mOptions{};
    sw::redis::ConnectionPoolOptions mPoolOptions;
};

#endif
