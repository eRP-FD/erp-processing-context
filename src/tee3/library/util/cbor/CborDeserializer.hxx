/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_CBOR_CBORDESERIALIZER_HXX
#define EPA_LIBRARY_UTIL_CBOR_CBORDESERIALIZER_HXX

#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/stream/ReadableByteStream.hxx"
#include "library/util/cbor/Cbor.hxx"
#include "library/util/cbor/CborError.hxx"
#include "library/util/cbor/CborReader.hxx"

#include <functional>
#include <string_view>

namespace epa
{

/**
 * Due to a clang compiler bug (https://github.com/llvm/llvm-project/issues/36032) the following
 * enum and struct are defined outside the CborDeserializer and have clunky names.
 */
enum class CborDeserializerErrorMode
{
    Error,
    Warning,
    Ignore
};
struct CborDeserializerContext
{
    /// How to treat registered keys that are not found in the CBOR stream.
    const CborDeserializerErrorMode missingItems = CborDeserializerErrorMode::Error;
    /// How to treat key/pairs that are found in the CBOR stream but have not been registered.
    const CborDeserializerErrorMode extraItems = CborDeserializerErrorMode::Ignore;
    /// Strings (text and binary) that are longer than this value are split into smaller parts.
    const size_t maxStringLength = CborReader::defaultMaxStringLength;
};


/**
 * The CborDeserializer takes a serializable object, i.e. one that matches the CborSerializable
 * concept, and a CBOR byte stream. The stream is parsed and each time a registered key/value
 * pair is found, the associated member in the serializable object is set.
 *
 * Note that there is currently only limited support to deserialize arrays.
 * You could use `expect()` to register deserialization of individual array parts. Better support
 * can be implemented when there is a use case for it and an interface becomes clear.
 */
class CborDeserializer
{
public:
    ~CborDeserializer() = default;

    /**
     * Deserialize an object of type T from the given `cborData`. T must provide templated
     * methods `processMembers()` to tell us how map CBOR items to members.
     *
     * Use like this:
     * class Serializable
     * {
     *     std::string member;
     *
     *     // This is a non-const method so that it can be used for both serialization
     *     // and deserialization. It is a template so that `Processor` can be templated on
     *     // the second argument, the value, to the processor's call operator(). This allows the
     *     // processor to process members of `Serializable` type-safe and efficiently and can avoid
     *     // the use of e.g. std::any.
     *     template<typename Processor>
     *     void processMembers (Processor& processor)
     *     {
     *         processor("/member", member);
     *     }
     * };
     */
    template<CborDeserializable T>
    static T deserialize(const BinaryBuffer& cborData, CborDeserializerContext context = {});

    /**
     * When you can't or don't want to implement a `processMembers()` function, you can
     * call expect() for each member of T that you want to read.
     * Then call the non-templated variant of deserialize().
     */
    template<typename T>
    void expect(std::string_view path, T& t);

    /**
     * Ignore key/value pairs when you know that they will be in a CBOR stream but your
     * are not intereseted in their values.
     */
    void ignore(std::string_view path);

    /**
     * Execute the deserialization based on item-to-member mappings which have been established
     * with previous calls to expect().
     */
    void deserialize(const BinaryBuffer& cborData, CborDeserializerContext context);

private:
    struct ExpectedItem
    {
        std::function<void(Cbor::Data&&, const std::string_view&)> receiver;
        size_t callCount = 0;
    };
    using ExpectedItems = std::unordered_map<std::string, ExpectedItem>;
    ExpectedItems mExpectedItems;

    enum DeserializationStatus
    {
        OK,
        MissingItem,
        ExtraItem
    };
    static DeserializationStatus processValue(
        ExpectedItems& expectedItems,
        const std::string_view path,
        const Cbor::Type type,
        const bool isLast,
        Cbor::Data&& data,
        const CborDeserializerContext& context);


    /**
     * Call this method after deserialization of an object to handle members that have been
     * processed more than once or not at all.
     * The `context` defines if these cases are treated as error, warning or are ignored.
     */
    static DeserializationStatus processDeserializationErrors(
        const std::string& itemName,
        size_t callCount,
        const CborDeserializerContext& context);
};


namespace
{
    /**
     * This class is a helper for the deserialize() method. It exists so that the operator()
     * method can be templated and provide type safe access to each member value of a serializable
     * object.
     */
    struct MemberRegisterer
    {
        CborDeserializer& deserializer;
        std::string prefix;

        template<typename Member>
        void operator()(const std::string_view path, Member& value)
        {
            if constexpr (CborSerializable<Member>)
            {
                // Value is a complex type which is serializable itself.
                // Serialize it into a nested key/value map.
                MemberRegisterer nested{
                    .deserializer = deserializer, .prefix = prefix + std::string(path)};
                value.processMembers(nested);
            }
            else
                deserializer.expect(prefix + std::string(path), value);
        }

        void operator()(const std::string_view path)
        {
            deserializer.ignore(prefix + std::string(path));
        }
    };
}


template<CborDeserializable T>
T CborDeserializer::deserialize(const BinaryBuffer& cborData, const CborDeserializerContext context)
{
    CborDeserializer deserializer;

    // Prepare a new instance of T.
    T object;

    // Register member handlers that will be provided with a (string_view) path and a reference to
    // the member. The path will be used to match against Cbor names.
    auto registerer = MemberRegisterer{.deserializer = deserializer, .prefix = "/[0]"};
    object.processMembers(registerer);

    // Parse the CBOR data. That will call the registered handlers for each cbor item.
    deserializer.deserialize(cborData, context);

    return object;
}


namespace
{
    template<std::integral T, std::integral U>
    void assignToInteger(T& t, const U& value, const std::string_view&)
    {
        t = gsl::narrow<T>(value);
    }
    template<typename T>
    void assignToInteger(T& t, const uint8_t& value, const std::string_view&)
        requires std::is_enum_v<T>
    {
        t = static_cast<T>(value);
    }
    template<typename T, typename U>
    void assignToInteger(T&, const U&, const std::string_view& path)
        requires(! std::is_integral_v<T>)
    {
        Failure() << "can not assign value to member " << path;
    }
}

//NOLINTBEGIN(readability-function-cognitive-complexity)
template<typename T>
void CborDeserializer::expect(const std::string_view path, T& t)
{
    // clang-format off
    auto receiver = [&t]
        (const Cbor::Data& data, const std::string_view& path)
        {
            using U = std::decay_t<decltype(t)>;
            switch (data.index())
            {
                case 0:
                    assignToInteger(t, std::get<uint8_t>(data), path);
                    break;
                case 1:
                    assignToInteger(t, std::get<uint16_t>(data), path);
                    break;
                case 2:
                    assignToInteger(t, std::get<uint32_t>(data), path);
                    break;
                case 3:
                    assignToInteger(t, std::get<uint64_t>(data), path);
                    break;
                case 4:
                    assignToInteger(t, -std::get<int8_t>(data), path);
                    break;
                case 5:
                    assignToInteger(t, -std::get<int16_t>(data), path);
                    break;
                case 6:
                    assignToInteger(t, -std::get<int32_t>(data), path);
                    break;
                case 7:
                    assignToInteger(t, -std::get<int64_t>(data), path);
                    break;
                case 8: // StreamBuffers
                    if constexpr (std::is_same_v<U, std::string>)
                    {
                        for (const auto& buffer : std::get<StreamBuffers>(std::move(data)))
                            t += buffer.toString();
                    }
                    else if constexpr (std::is_same_v<U, BinaryBuffer>)
                    {
                        BinaryBuffer b(std::get<StreamBuffers>(std::move(data)).toString());
                        if (t.size() == 0)
                            t = std::move(b);
                        else
                            t = BinaryBuffer::concatenate(t, b);
                    }
                    else if constexpr (std::is_same_v<U, SensitiveDataGuard>)
                    {
                        BinaryBuffer b(std::get<StreamBuffers>(std::move(data)).toString());
                        if (t.size() == 0)
                            t = SensitiveDataGuard(std::move(b));
                        else
                            t = SensitiveDataGuard(BinaryBuffer::concatenate(*t, b));
                    }
                    else if constexpr (std::is_same_v<U, Tee3Protocol::VauCid>)
                    {
                        t = Tee3Protocol::VauCid(std::get<StreamBuffers>(std::move(data)).toString());
                    }
                    else if constexpr (std::is_same_v<U, std::string_view>)
                    {
                        // For string_views, we do not assign but only verify that the
                        // two values are the same.
                        Assert(t == std::get<StreamBuffers>(std::move(data)).toString())
                                  << assertion::exceptionType<CborError>(CborError::Code::DeserializationError);
                    }
                    else
                    {
                        Failure() << "don't know how to assign StreamBuffers to type "
                                  << typeid(T).name()
                                  << assertion::exceptionType<CborError>(CborError::Code::DeserializationError);
                    }
                    break;

                // std::vector<BinaryBuffer>
                case 9:
                    if constexpr (std::is_same_v<U, std::vector<BinaryBuffer>>)
                    {
                        t = std::get<std::vector<BinaryBuffer>>(data);
                    }
                    else
                    {
                        Failure() << "unsupported array type " << typeid(T).name()
                                  << assertion::exceptionType<CborError>(CborError::Code::DeserializationError);

                    }
                    break;

                // bool
                case 10:
                    if constexpr (std::is_same_v<U, bool>)
                        t = std::get<bool>(data);
                    else
                        Failure() << "don't know how to assign bool to type " << typeid(T).name();
                    break;

                case 11: // monostate (e.g. null)
                    break;
                default:
                    Failure() << "unsupported cbor data type"
                                  << assertion::exceptionType<CborError>(CborError::Code::DeserializationError);

            }
        };
    // clang-format on

    mExpectedItems.emplace(path, ExpectedItem{.receiver = std::move(receiver), .callCount = 0});
}
//NOLINTEND(readability-function-cognitive-complexity)


inline void CborDeserializer::ignore(const std::string_view path)
{
    // Register a no-op lambda that prevents `path` from being reported as unknown item
    // when a value is discovered in a CBOR stream.
    mExpectedItems.emplace(
        path,
        ExpectedItem{
            .receiver = [](const Cbor::Data&, const std::string_view&) {}, .callCount = 0});
}

} // namespace epa

#endif
