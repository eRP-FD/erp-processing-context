/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef TEST_UTIL_LOGTESTBASE_HXX
#define TEST_UTIL_LOGTESTBASE_HXX

#include "shared/util/GLog.hxx"

#include <gtest/gtest.h>
#include <mutex>
#include <regex>
#include <unordered_map>


/**
 * A collection of helper functions that support verification of log messages.
 * The embedded class TestLogSink is used to collect log messages from GLog.
 */
class LogTestBase
{
public:
    /**
     * A custom sink for GLog to collect all log lines while the sink is installed for later analysis.
     * This is a RAII class. The constructor installs the sink, the destructor removes it.
     * Note that display of log messages on the console or writing log messages to files is
     * not affected.
     */
    class TestLogSink : public google::LogSink
    {
    public:
        TestLogSink();
        ~TestLogSink() override;
        void send(google::LogSeverity, const char*, const char*, int, const struct ::tm*, const char* message,
                  size_t messageLength) override;

        /**
         * Call `visitor` for each line currently in mLines under locked mutex.
         * That means that all messages that arrive while the mutex is locked are not presented to the visitor.
         */
        void visitLines(const std::function<void(const std::string&)>& visitor) const;

        std::vector<std::string> lines() const;
        std::string line(const size_t index) const;
        size_t lineCount() const;
        void clear();

    private:
        mutable std::mutex mMutex;
        std::vector<std::string> mLines;
    };

    using FlatKeyValues = std::unordered_map<std::string, std::string>;

    LogTestBase();

    /**
     * Assert that a) there is exactly one log message in `mLogMessages` and that b) it is equal to
     * the given `expectedValue`.
     */
    void assertOneExactLogMessage(const std::string& expectedValue) const;

    /**
     * Assert that a) there is exactly one log message in `mLogMessages` and that b) it matches
     * the regular expression `expectedPattern`.
     * Returns the captured match strings so that the caller can analyze any captured values.
     */
    std::vector<std::string> assertOneLogMessage(const std::string& expectedPattern) const;

    /**
     * Assert that any log message matches the `expectedPattern`.
     * Returns the index of the matching message in `outIndex`. This method uses a parameter to return a value
     * so that the implementation can use ASSERT_* and FAIL macros.
     */
    void assertLogMessage(const std::string& expectedPattern, std::optional<size_t>& outIndex) const;

    /**
     * Assert that `mLogMessages` is empty.
     */
    void assertNoLogMessage() const;

    /**
     * Assert that there is no log message that matches the `expectedPattern`.
     */
    void assertNoLogMessage(const std::string& expectedPattern) const;

    /*
     * Discard all log messages collected so far.
     */
    void clearLogMessages();

    /**
     * Return the number of all messages received since the last call to `clearLogMessages()`.
     */
    size_t getLogMessageCount() const;

    /**
     * Assert that a) there is exactly one log message and that b) it can be parsed as a JSON line.
     * Then return the index of the key/value pair whose key is equal to the given `key`.
     * Intended use is to verify a specific ordering of key/pairs.
     */
    size_t getLogKeyIndex(const std::string& key) const;


private:
    TestLogSink mSink;
};

#endif
