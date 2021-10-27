/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_REDISINTERFACE_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_REDISINTERFACE_HXX

#include <chrono>
#include <optional>
#include <string_view>
#ifndef _WIN32
#include <unistd.h>
#endif

class RedisInterface
{
public:
    virtual ~RedisInterface() = default;

    virtual bool exists(const std::string_view& key) = 0;
    virtual std::optional<std::string> fieldValueForKey(const std::string_view& key,
                                                        const std::string_view& field) = 0;
    virtual bool hasKeyWithField(const std::string_view& key,
                                 const std::string_view& field) = 0;
    virtual void setKeyFieldValue(const std::string_view& key, const std::string_view& field,
                                  const std::string_view& value) = 0;
    virtual void
    setKeyExpireAt(const std::string_view& key,
                   const std::chrono::time_point<std::chrono::system_clock,
                   std::chrono::milliseconds>& timestamp) = 0;

};

#endif
