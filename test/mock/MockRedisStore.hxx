#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCK_MOCKREDISSTORE_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCK_MOCKREDISSTORE_HXX

#include <map>
#include <string>

#include "erp/service/RedisInterface.hxx"

class MockRedisStore : public RedisInterface
{
public:
    MockRedisStore() = default;

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

private:
    void removeExpiredEntries();

    std::map<std::string, std::map<std::string, std::string>> mData;// Key: sub claim, Value: kv
};

#endif
