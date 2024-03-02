/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCK_MOCKREDISSTORE_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCK_MOCKREDISSTORE_HXX

#include <map>
#include <string>

#include "erp/service/RedisInterface.hxx"

class MockRedisStore : public RedisInterface
{
public:
    MockRedisStore() = default;

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
private:
    void removeExpiredEntries();

    std::map<std::string, std::map<std::string, std::string>> mData;// Key: sub claim, Value: kv
};

#endif
