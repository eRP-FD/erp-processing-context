/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_JSON_BINARYDOCUMENT_HXX
#define EPA_LIBRARY_JSON_BINARYDOCUMENT_HXX

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace epa
{

class BinaryDocument
{
public:
    enum class Type
    {
        Bool,
        Int64,
        Double,
        String,
        Array,
        Object
    };

    enum class EncType
    {
        False,
        True,
        PositiveInt,
        NegativeInt,
        Double,
        String,
        FrequentString,
        UuidString,
        UrnUuidString,
        OidString,
        UrnOidString,
        TimeString,
        Array,
        Object,
        CodedValueObject
    };

    struct Value
    {
        explicit Value(Type type, Value* parent = nullptr);
        virtual ~Value();
        virtual void encode(std::string& buffer) const = 0;
        virtual void decode(EncType encType, std::uint64_t number, std::string_view& buffer) = 0;
        static std::unique_ptr<Value> decode(std::string_view& buffer, Value* parent = nullptr);

        const Type type;
        Value* const parent;
    };

    using ValuePtr = std::unique_ptr<Value>;

    struct Bool : public Value
    {
        explicit Bool(Value* parent = nullptr, bool value = false);
        void encode(std::string& buffer) const override;
        void decode(EncType encType, std::uint64_t number, std::string_view& buffer) override;

        bool value;
    };

    struct Int64 : public Value
    {
        explicit Int64(Value* parent = nullptr, std::int64_t value = 0);
        void encode(std::string& buffer) const override;
        void decode(EncType encType, std::uint64_t number, std::string_view& buffer) override;

        std::int64_t value;
    };

    struct Double : public Value
    {
        explicit Double(Value* parent = nullptr, double value = 0.0);
        void encode(std::string& buffer) const override;
        void decode(EncType encType, std::uint64_t number, std::string_view& buffer) override;

        double value;
    };

    struct String : public Value
    {
        explicit String(Value* parent = nullptr, std::string value = std::string());
        void encode(std::string& buffer) const override;
        void decode(EncType encType, std::uint64_t number, std::string_view& buffer) override;

        std::string value;
        static const char* const oidChars;
    };

    struct Array : public Value
    {
        explicit Array(Value* parent = nullptr);
        void encode(std::string& buffer) const override;
        void decode(EncType encType, std::uint64_t number, std::string_view& buffer) override;

        std::vector<ValuePtr> elements;
    };

    struct Object : public Value
    {
        explicit Object(Value* parent = nullptr);
        void encode(std::string& buffer) const override;
        void decode(EncType encType, std::uint64_t number, std::string_view& buffer) override;

        // The std::less<> allows to find by string_view (and helps to avoid temporary strings).
        std::map<std::string, ValuePtr, std::less<>> elements;
    };

    BinaryDocument();
    ~BinaryDocument();

    // First byte of encoded document. This is used for detection, and it could also be used for
    // version management in the future.
    static const char magic = '\x80';

    // Methods for writing.
    void clear();
    void addBool(bool value);
    void addInt64(std::int64_t value);
    void addDouble(double value);
    void addString(std::string value);
    void addKey(std::string key);
    void startArray();
    void endArray();
    void startObject();
    void endObject();
    std::string encode() const;

    // Methods for reading.
    static bool isBinaryDocument(std::string_view data);
    void decode(std::string_view data);
    const Value* getRoot() const;

private:
    void add(ValuePtr valuePtr);

    static void encodeTypeAndNumber(std::string& buffer, EncType encType, std::uint64_t number);
    static std::pair<EncType, std::uint64_t> decodeTypeAndNumber(std::string_view& buffer);

    std::unique_ptr<Value> mRootValue;
    Value* mCurrentParent = nullptr;
    std::optional<std::string> mCurrentKey;

    using StringRefToIndexMap =
        // NOLINTNEXTLINE(modernize-use-transparent-functors): Won't work because of the reference_wrapper
        std::map<std::reference_wrapper<const std::string>, size_t, std::less<const std::string>>;

    static const std::vector<std::string> mAllKeys;
    static const StringRefToIndexMap mAllKeyIndices;

    static const std::vector<std::string> mFrequentStrings;
    static const StringRefToIndexMap mFrequentStringIndices;

    using CodedValueEntry = std::tuple<std::string, std::string, std::string>;
    using CodedValueEntryRefToIndexMap = std::map<
        std::reference_wrapper<const CodedValueEntry>,
        size_t,
        // NOLINTNEXTLINE(modernize-use-transparent-functors): Won't work because of the reference_wrapper
        std::less<const CodedValueEntry>>;

    static const std::vector<CodedValueEntry> mCodedValues;
    static const CodedValueEntryRefToIndexMap mCodedValueIndices;
};

} // namespace epa

#endif
