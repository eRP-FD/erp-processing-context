/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASE_REDISWRAPPER_HXX
#define ERP_PROCESSING_CONTEXT_DATABASE_REDISWRAPPER_HXX

#include <sw/redis++/redis++.h>

#include "erp/service/RedisInterface.hxx"
#include "erp/util/Configuration.hxx"

class RedisClient : public RedisInterface
{
public:
    explicit RedisClient();
    ~RedisClient() override = default;

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

    int incr(const std::string_view& key) override;
private:
    std::unique_ptr<sw::redis::Redis> mConnection;
    sw::redis::ConnectionOptions mOptions{};
    sw::redis::ConnectionPoolOptions mPoolOptions;
};

#endif
