/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_LOG_JSONLOGIMPLEMENTATION_HXX
#define EPA_LIBRARY_LOG_JSONLOGIMPLEMENTATION_HXX

#include "library/log/Logging.hxx"
#include "library/log/PlainLogImplementation.hxx"

#include <string>
#include <vector>

namespace epa
{

/**
 * Produce log messages in JSON format, i.e. in a JSONL (https://jsonlines.org/) format
 *
 *     { "key1" : "value1", "key2" : "value2" ... }
 *
 * The message that is collected in the `mMessage` stream object is added as "message" field value.
 * Additional key/value pairs can be added like this
 *
 *     LOG(INFO) << "message" << logging::keyValue("key", "value");
 *
 * There are a couple of "hard-coded" key/value pairs that are added as well (the key is hard-coded,
 * the value is not):
 *
 *     - session          as content of the "Session" request header field
 *     - severity         from the LOG(severity) macro
 *     - session-internal with a randomly created unique identifier so that log messages that
 *                        are written before the `session` field is initialized, can be identified
 *                        to belong to the same session.
 *     - thread           name of the thread that produced the log message
 *
 * More may be added in the future (e.g. time/date, trace id).
 */
class JsonLogImplementation : public PlainLogImplementation
{
public:
    JsonLogImplementation(Log::Severity severity, const Location& location, Log::Mode mode);

    /**
     * `keyValue` pairs are added in the relative order in which this method is called.
     * They are all placed after the `message` value.
     */
    void addKeyValue(const Log::KeyValue& keyValue) override;

    /**
     * The callback in `writer` will be called after all other key/value pairs have been written.
     */
    void addJsonSerializer(const Log::JsonSerializer& serializer) override;

    /**
     * Returns `true`.
     */
    bool isJson() const override;

    void setFlag(Log::Flag flag) override;

protected:
    /**
     * Return the string that will be passed on to GLog. For `JsonLogImplementation` this is
     * a serialization of the all key/pair values.
     */
    std::string assembleLogLine() const override;

private:
    /**
     * Store explicitly provided key/value pairs in a vector to control their relative order
     * when printed.
     */
    std::vector<Log::KeyValue> mKeyPairs;
    std::vector<Log::JsonSerializer::CallbackType> mJsonSerializerCallbacks;
    bool mShowSessionDescription;
};

} // namespace epa

#endif
