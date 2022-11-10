/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "test/util/LogTestBase.hxx"

#include <regex>


LogTestBase::TestLogSink::TestLogSink()
    : mMutex()
    , mLines()
{
    google::AddLogSink(this);
}


LogTestBase::TestLogSink::~TestLogSink()
{
    google::RemoveLogSink(this);
}


void LogTestBase::TestLogSink::visitLines(std::function<void(const std::string&)> visitor) const
{
    std::lock_guard lock(mMutex);
    for (const auto& line : mLines)
        visitor(line);
}


std::vector<std::string> LogTestBase::TestLogSink::lines() const
{
    std::scoped_lock lock(mMutex);
    return mLines;
}


std::string LogTestBase::TestLogSink::line(const size_t index) const
{
    std::lock_guard lock(mMutex);
    return mLines.at(index);
}

size_t LogTestBase::TestLogSink::lineCount() const
{
    std::lock_guard lock(mMutex);
    return mLines.size();
}


void LogTestBase::TestLogSink::clear()
{
    std::lock_guard lock(mMutex);
    mLines.clear();
}


void LogTestBase::TestLogSink::send(const google::LogSeverity /*severity*/, const char* /*full_filename*/,
                                    const char* /*base_filename*/, const int /*line*/, const struct ::tm* /*tm_time*/,
                                    const char* message, const size_t messageLength)
{
    std::lock_guard lock(mMutex);
    mLines.emplace_back(message, messageLength);
}


LogTestBase::LogTestBase()
    : mSink()
{
}


void LogTestBase::assertOneExactLogMessage(const std::string& expectedValue) const
{
    ASSERT_EQ(mSink.lineCount(), 1);
    mSink.visitLines([&](const auto& line) {
        ASSERT_EQ(line, expectedValue);
    });
}


std::vector<std::string> LogTestBase::assertOneLogMessage(const std::string& expectedPattern) const
{
    EXPECT_EQ(mSink.lineCount(), 1);
    const auto line = mSink.line(0);

    std::smatch match;
    EXPECT_TRUE(std::regex_match(line, match, std::regex(expectedPattern)))
        << "the input '" << mSink.line(0) << "' is not matched by regex '" << expectedPattern << "'";

    return std::vector<std::string>(match.begin(), match.end());
}


void LogTestBase::assertLogMessage(const std::string& expectedPattern, std::optional<size_t>& outIndex) const
{
    ASSERT_GT(mSink.lineCount(), 0);
    outIndex.reset();
    size_t index = 0;
    size_t matchCount = 0;
    const auto pattern = std::regex(expectedPattern);
    mSink.visitLines([&](const auto& line) {
        if (std::regex_match(line, pattern))
        {
            ++matchCount;
            outIndex = index;
        }
        ++index;
    });

    ASSERT_TRUE(outIndex.has_value()) << "no line matches '" << expectedPattern << "'";
}


void LogTestBase::assertNoLogMessage() const
{
    ASSERT_EQ(mSink.lineCount(), 0) << "first log message is '" << mSink.line(0) << "'";
}


void LogTestBase::assertNoLogMessage(const std::string& expectedPattern) const
{
    const auto pattern = std::regex(expectedPattern);
    mSink.visitLines([&](const auto& line) {
        ASSERT_FALSE(std::regex_match(line, pattern));
    });
}


void LogTestBase::clearLogMessages()
{
    mSink.clear();
}


size_t LogTestBase::getLogMessageCount() const
{
    return mSink.lineCount();
}
