/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/mock/MockRedisStore.hxx"

bool MockRedisStore::exists(const std::string_view& key)
{
    removeExpiredEntries();
    bool val = mData.find(std::string{key}) != mData.end();
    return val;
}

std::optional<std::string> MockRedisStore::fieldValueForKey(const std::string_view& key,
                                                            const std::string_view& field)
{
    removeExpiredEntries();
    auto kv = mData[std::string{key}];
    auto val = kv[std::string{field}];
    return val;
}

bool MockRedisStore::hasKeyWithField(const std::string_view& key, const std::string_view& field)
{
    removeExpiredEntries();
    auto kv = mData[std::string{key}];
    auto val = kv.find(std::string{field}) != kv.end();
    return val;
}

void MockRedisStore::setKeyFieldValue(const std::string_view& key, const std::string_view& field,
                                      const std::string_view& value)
{
    removeExpiredEntries();
    mData[std::string{key}][std::string{field}] = value;
}

void MockRedisStore::setKeyExpireAt(
    const std::string_view& key,
    const std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>& timestamp)
{
    removeExpiredEntries();
    auto s = std::to_string(timestamp.time_since_epoch().count());
    mData[std::string{key}]["expireat"] = s;
}

int64_t MockRedisStore::incr(const std::string_view& key)
{
    const std::string_view field = "_mockCount";
    int64_t count = 1;
    if (exists(key)) {
        auto countStr = fieldValueForKey(key, field).value();
		char* end = nullptr;
        count = std::strtoll(countStr.c_str(), &end, 10);
        count++;
    }
    setKeyFieldValue(key, field, std::to_string(count));
    return count;
}

void MockRedisStore::publish(const std::string_view& channel [[maybe_unused]], const std::string_view& message [[maybe_unused]])
{
    // NOTE: implementation probably not required.
}

void MockRedisStore::removeExpiredEntries()
{
    // Perform basic cleanup which simulates Redis expireat feature.
    const auto tNow = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count();
    using M = std::map<std::string, std::map<std::string, std::string>>;
    for (M::const_iterator it = mData.cbegin(); it != mData.cend();)
    {
        auto kv = *it;
        bool skip = false;
        if (kv.second.find("expireat") != kv.second.end())
        {
            auto exp_ms_tmp = static_cast<std::map<std::string, std::string>> (kv.second)["expireat"];
            int64_t exp_ms = std::stoll(exp_ms_tmp);
            if (exp_ms - tNow <= 0) {
                it = mData.erase(it);
                skip = true;
            }
        }
        if (!skip)
        it++;
    }
}
