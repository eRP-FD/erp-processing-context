/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/json/BinaryDocument.hxx"
#include "library/util/Assert.hxx"
#include "library/util/ByteHelper.hxx"
#include "library/util/Time.hxx"

#include <boost/regex.hpp>

namespace epa
{

//==============================================================================
// Ideas for further optimizations (TODO):
//
//  - Detect KVNRs and encode them optimized.
//
//  - Add language codes to mFrequentStrings.
//
//  - Split EncType::CodedValueObject into all the coded value types (FormatCodeObject,
//    EventCodeObject, and so on). Split mCodedValues accordingly...
//
//  - Also split EncType::FrequentString into more types...
//
//  - Write a function which guesses the EncType by analyzing the parents of the value. For example,
//    if the parent is an Array and if grandpa is an Object, and if the key of that array is named
//    "confidentialityCodes", then the value is most likely an EncType::ConfidentialityCodeObject.
//    Then add another EncType named EncType::Guess which the encoder produces if the guessed type
//    is identical to the real value type. And then let EncType::Guess take only one bit in the type
//    encoding.
//
//  - If we drop zlib compression(!) then we could make it all a bit-stream instead of a byte-stream
//    so that every value can have any number of bits. This could improve and simplify some things.
//    encodeTypeAndNumber can be split into two functions: encodeType and encodeNumber. We could
//    have two or three different encodeNumber functions that use different huffman codes depending
//    on how likely it is that the number is small or in a certain range. In the end, for example, a
//    Bool value will usually take only two bits: one bit that says "yes it's the guessed type" and
//    and one bit that says true or false.
//
//  - The "yes it's the guessed type" information could optionally be said for a whole subtree
//    instead of each value. i.e. simply have one more EncType besides EncType::Guess namely
//    EncType::GuessForTree which can be used for arrays and objects. All children have no encoded
//    type then.
//
//  - Elements of an object are always sorted by key names. This can be exploited when encoding the
//    keys.
//
//  - The key encoding could also depend on the parents (like with the guessed type): Have different
//    sets of keys (= object types) and have a function that guesses which set to use, and encode in
//    one bit for each object whether that guessing is right. For the top-level object, the type
//    should be given by the caller (i.e. whether it's a folder, document entry, submission set, or
//    audit log).
//
//  - Add EncType::AsciiString where each char is encoded with 7-bit.
//
// Maybe all this could bring about 10 to 30 percent in saving size without significantly losing
// speed?
//==============================================================================


BinaryDocument::Value::Value(Type type, Value* parent)
  : type(type),
    parent(parent)
{
}


BinaryDocument::Value::~Value() = default;


std::unique_ptr<BinaryDocument::Value> BinaryDocument::Value::decode(
    std::string_view& buffer,
    Value* parent)
{
    auto [encType, number] = decodeTypeAndNumber(buffer);
    ValuePtr valuePtr;
    switch (encType)
    {
        case EncType::False:
        case EncType::True:
            valuePtr = std::make_unique<Bool>(parent);
            break;
        case EncType::PositiveInt:
        case EncType::NegativeInt:
            valuePtr = std::make_unique<Int64>(parent);
            break;
        case EncType::Double:
            valuePtr = std::make_unique<Double>(parent);
            break;
        case EncType::String:
        case EncType::FrequentString:
        case EncType::UuidString:
        case EncType::UrnUuidString:
        case EncType::OidString:
        case EncType::UrnOidString:
        case EncType::TimeString:
            valuePtr = std::make_unique<String>(parent);
            break;
        case EncType::Array:
            valuePtr = std::make_unique<Array>(parent);
            break;
        case EncType::Object:
        case EncType::CodedValueObject:
            valuePtr = std::make_unique<Object>(parent);
            break;
    };
    Assert(valuePtr);
    valuePtr->decode(encType, number, buffer);
    return valuePtr;
}


BinaryDocument::Bool::Bool(Value* parent, bool value)
  : Value(Type::Bool, parent),
    value(value)
{
}


void BinaryDocument::Bool::encode(std::string& buffer) const
{
    encodeTypeAndNumber(buffer, value ? EncType::True : EncType::False, 0);
}


void BinaryDocument::Bool::decode(EncType encType, std::uint64_t number, std::string_view&)
{
    Assert(number == 0);
    if (encType == EncType::True)
    {
        value = true;
    }
    else
    {
        Assert(encType == EncType::False);
        value = false;
    }
}


BinaryDocument::Int64::Int64(Value* parent, std::int64_t value)
  : Value(Type::Int64, parent),
    value(value)
{
}


void BinaryDocument::Int64::encode(std::string& buffer) const
{
    if (value >= 0)
        encodeTypeAndNumber(buffer, EncType::PositiveInt, static_cast<std::uint64_t>(value));
    else
        encodeTypeAndNumber(buffer, EncType::NegativeInt, static_cast<std::uint64_t>(-value - 1));
}


void BinaryDocument::Int64::decode(EncType encType, std::uint64_t number, std::string_view&)
{
    if (encType == EncType::PositiveInt)
    {
        value = static_cast<std::int64_t>(number);
    }
    else
    {
        Assert(encType == EncType::NegativeInt);
        value = -1 - static_cast<std::int64_t>(number);
    }
}


BinaryDocument::Double::Double(Value* parent, double value)
  : Value(Type::Double, parent),
    value(value)
{
}


void BinaryDocument::Double::encode(std::string& buffer) const
{
    encodeTypeAndNumber(buffer, EncType::Double, 0);

    // TODO: Depends on floating point format and endianness, this not so portable. Maybe simply do
    // not support double? Or is it used somewhere? If we need double, common values like 0.0 and
    // 1.0 could be encoded more efficient.
    buffer.append(reinterpret_cast<const char*>(&value), sizeof(double));
}


void BinaryDocument::Double::decode(EncType encType, std::uint64_t number, std::string_view& buffer)
{
    Assert(encType == EncType::Double);
    Assert(number == 0);
    Assert(buffer.size() >= sizeof(double));

    value = *reinterpret_cast<const double*>(buffer.data());

    buffer.remove_prefix(sizeof(double));
}


BinaryDocument::String::String(Value* parent, std::string value)
  : Value(Type::String, parent),
    value(std::move(value))
{
}

//NOLINTBEGIN(readability-function-cognitive-complexity, cppcoreguidelines-pro-bounds-constant-array-index)
void BinaryDocument::String::encode(std::string& buffer) const
{
    auto it = mFrequentStringIndices.find(value);
    if (it != mFrequentStringIndices.end())
    {
        encodeTypeAndNumber(buffer, EncType::FrequentString, it->second);
        return;
    }

    static const boost::regex reUuid("(urn:uuid:)?[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}");
    if ((value.size() == 36 || value.size() == 45) && boost::regex_match(value, reUuid))
    {
        encodeTypeAndNumber(
            buffer, value.size() == 36 ? EncType::UuidString : EncType::UrnUuidString, 0);
        std::string_view view(value.data() + value.size() - 36, 36);
        buffer.append(ByteHelper::fromHex(view.substr(0, 8)));
        buffer.append(ByteHelper::fromHex(view.substr(9, 4)));
        buffer.append(ByteHelper::fromHex(view.substr(14, 4)));
        buffer.append(ByteHelper::fromHex(view.substr(19, 4)));
        buffer.append(ByteHelper::fromHex(view.substr(24, 12)));
        return;
    }

    if (! value.empty())
    {
        static const std::array<unsigned char, 256> oidCharMap = []() {
            std::array<unsigned char, 256> init = {};
            init.fill(16);
            for (unsigned char i = 0; oidChars[i]; i++)
                init[static_cast<unsigned char>(oidChars[i])] = i;
            return init;
        }();
        size_t start = 0;
        size_t end = value.size();
        if (end >= 8 && std::string_view(value).substr(0, 8) == "urn:oid:")
            start = 8;
        size_t i = start;
        while (i < end && oidCharMap[static_cast<unsigned char>(value[i])] <= 15)
            i++;
        if (i == end)
        {
            encodeTypeAndNumber(
                buffer, start == 0 ? EncType::OidString : EncType::UrnOidString, end - start);
            for (size_t j = start; j < end; j += 2)
            {
                unsigned char hi = oidCharMap[static_cast<unsigned char>(value[j])];
                unsigned char lo =
                    j + 1 < end ? oidCharMap[static_cast<unsigned char>(value[j + 1])] : 0;
                Assert(hi <= 15 && lo <= 15);
                buffer += static_cast<char>((hi << 4) | lo);
            }
            return;
        }
    }

    static const boost::regex reTime("[0-9]+-[0-1][0-9]-[0-3][0-9]T[0-2][0-9](:[0-5][0-9]){2}Z");
    if (value.size() >= 20 && value.size() <= 22 && boost::regex_match(value, reTime))
    {
        try
        {
            int64_t secondsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(
                                            Time::fromRfc3339(value).time_since_epoch())
                                            .count();
            // I think 40 bits should be enough...
            unsigned char data[5];
            data[0] = static_cast<unsigned char>(secondsSinceEpoch >> 32);
            data[1] = static_cast<unsigned char>(secondsSinceEpoch >> 24);
            data[2] = static_cast<unsigned char>(secondsSinceEpoch >> 16);
            data[3] = static_cast<unsigned char>(secondsSinceEpoch >> 8);
            data[4] = static_cast<unsigned char>(secondsSinceEpoch);
            encodeTypeAndNumber(buffer, EncType::TimeString, 0);
            buffer.append(reinterpret_cast<const char*>(data), sizeof(data));
            return;
        }
        // NOLINTBEGIN(bugprone-empty-catch)
        catch (const TimeFormatException&)
        {
            // Just fall through to encode in a different way.
        }
        // NOLINTEND(bugprone-empty-catch)
    }

    encodeTypeAndNumber(buffer, EncType::String, value.size());
    buffer.append(value);
}
//NOLINTEND(readability-function-cognitive-complexity, cppcoreguidelines-pro-bounds-constant-array-index)

void BinaryDocument::String::decode(EncType encType, std::uint64_t number, std::string_view& buffer)
{
    if (encType == EncType::FrequentString)
    {
        Assert(number < mFrequentStrings.size());
        value = mFrequentStrings.at(number);
    }
    else if (encType == EncType::UuidString || encType == EncType::UrnUuidString)
    {
        Assert(buffer.size() >= 16);
        std::string_view binaryUuid = buffer.substr(0, 16);
        std::string hexStr = ByteHelper::toHex(binaryUuid);
        std::string_view hex(hexStr);
        if (encType == EncType::UrnUuidString)
        {
            value = "urn:uuid:";
            value.reserve(45);
        }
        else
        {
            value.clear();
            value.reserve(36);
        }
        value.append(hex.substr(0, 8));
        value.append("-");
        value.append(hex.substr(8, 4));
        value.append("-");
        value.append(hex.substr(12, 4));
        value.append("-");
        value.append(hex.substr(16, 4));
        value.append("-");
        value.append(hex.substr(20, 12));
        buffer.remove_prefix(16);
    }
    else if (encType == EncType::OidString || encType == EncType::UrnOidString)
    {
        size_t dataLen = (number + 1) / 2;
        Assert(buffer.size() >= dataLen);
        const char* data = buffer.data();
        value.clear();
        if (encType == EncType::UrnOidString)
        {
            value.reserve(8 + number);
            value.append("urn:oid:");
        }
        else
        {
            value.reserve(number);
        }
        Assert(strlen(oidChars) == 16);
        for (size_t i = 0; i < number; i += 2)
        {
            int v = static_cast<unsigned char>(data[i >> 1]);
            value += oidChars[v >> 4];
            if (i + 1 < number)
                value += oidChars[v & 15];
        }
        buffer.remove_prefix(dataLen);
    }
    else if (encType == EncType::TimeString)
    {
        Assert(buffer.size() >= 5);
        const auto* data = reinterpret_cast<const unsigned char*>(buffer.data());
        auto secondsSinceEpoch = static_cast<std::int64_t>(data[0]) << 32;
        secondsSinceEpoch |= static_cast<std::int64_t>(data[1]) << 24;
        secondsSinceEpoch |= static_cast<std::int64_t>(data[2]) << 16;
        secondsSinceEpoch |= static_cast<std::int64_t>(data[3]) << 8;
        secondsSinceEpoch |= static_cast<std::int64_t>(data[4]);
        if (data[0] & 0x80)
            secondsSinceEpoch |= static_cast<std::int64_t>(0xffffff) << 40;
        value = Time::toRfc3339(Time::fromSecondsSinceEpoch(secondsSinceEpoch));
        buffer.remove_prefix(5);
    }
    else
    {
        Assert(encType == EncType::String);
        Assert(buffer.size() >= number);
        value = std::string(buffer.data(), number);
        buffer.remove_prefix(number);
    }
}


const char* const BinaryDocument::String::oidChars = "0123456789.,:;+-";


BinaryDocument::Array::Array(Value* parent)
  : Value(Type::Array, parent)
{
}


void BinaryDocument::Array::encode(std::string& buffer) const
{
    encodeTypeAndNumber(buffer, EncType::Array, elements.size());
    for (const auto& valuePtr : elements)
        valuePtr->encode(buffer);
}


void BinaryDocument::Array::decode(EncType encType, std::uint64_t number, std::string_view& buffer)
{
    Assert(encType == EncType::Array);
    elements.clear();
    elements.reserve(number);
    for (size_t i = 0; i < number; i++)
        elements.emplace_back(Value::decode(buffer, this));
}


BinaryDocument::Object::Object(Value* parent)
  : Value(Type::Object, parent)
{
}

//NOLINTBEGIN(readability-function-cognitive-complexity)
void BinaryDocument::Object::encode(std::string& buffer) const
{
    // Is it a known coded value?
    for (;;)
    {
        if (elements.size() != 3)
            break;

        auto it1 = elements.find(std::string_view("code"));
        if (it1 == elements.end())
            break;
        auto it2 = elements.find(std::string_view("codeSystem"));
        if (it2 == elements.end())
            break;
        auto it3 = elements.find(std::string_view("displayName"));
        if (it3 == elements.end())
            break;

        if (it1->second->type != Type::String)
            break;
        if (it2->second->type != Type::String)
            break;
        if (it3->second->type != Type::String)
            break;

        CodedValueEntry codedValue(
            dynamic_cast<const String&>(*it1->second).value,
            dynamic_cast<const String&>(*it2->second).value,
            dynamic_cast<const String&>(*it3->second).value);

        auto it = mCodedValueIndices.find(codedValue);
        if (it == mCodedValueIndices.end())
            break;

        // Yes it is a known coded value, store the index.
        encodeTypeAndNumber(buffer, EncType::CodedValueObject, it->second);
        return;
    }

    // Normal encoding of all the elements.
    encodeTypeAndNumber(buffer, EncType::Object, elements.size());
    for (const auto& [key, valuePtr] : elements)
    {
        auto it = mAllKeyIndices.find(key);
        Assert(it != mAllKeyIndices.end())
            << "Key not registered in BinaryDocument::mAllKeys: " << key;
        size_t keyIndex = it->second;

        unsigned char keyIndexBuf[2];
        size_t keyIndexLen = 1;
        if (keyIndex < 0x80)
        {
            keyIndexBuf[0] = static_cast<unsigned char>(keyIndex);
        }
        else if (keyIndex < 0x4000)
        {
            keyIndexBuf[0] = 0x80 | static_cast<unsigned char>(keyIndex >> 8);
            keyIndexBuf[1] = static_cast<unsigned char>(keyIndex);
            keyIndexLen = 2;
        }
        else
        {
            Failure() << "Key registry has grown - please implement support for larger key indices";
        }
        buffer.append(reinterpret_cast<const char*>(keyIndexBuf), keyIndexLen);
        valuePtr->encode(buffer);
    }
}
//NOLINTEND(readability-function-cognitive-complexity)


void BinaryDocument::Object::decode(EncType encType, std::uint64_t number, std::string_view& buffer)
{
    elements.clear();

    if (encType == EncType::CodedValueObject)
    {
        Assert(number < mCodedValues.size());
        const CodedValueEntry& codedValue = mCodedValues.at(number);
        elements.emplace("code", std::make_unique<String>(this, std::get<0>(codedValue)));
        elements.emplace("codeSystem", std::make_unique<String>(this, std::get<1>(codedValue)));
        elements.emplace("displayName", std::make_unique<String>(this, std::get<2>(codedValue)));
        return;
    }

    Assert(encType == EncType::Object);
    for (size_t i = 0; i < number; i++)
    {
        Assert(! buffer.empty());
        size_t keyIndex = static_cast<unsigned char>(buffer.front());
        buffer.remove_prefix(1);
        if ((keyIndex & 0x80) != 0)
        {
            Assert(! buffer.empty());
            keyIndex = (keyIndex & 0x7f) << 8;
            keyIndex |= static_cast<unsigned char>(buffer.front());
            buffer.remove_prefix(1);
        }
        Assert(keyIndex < mAllKeys.size());
        elements.emplace(mAllKeys.at(keyIndex), Value::decode(buffer, this));
    }
}


BinaryDocument::BinaryDocument() = default;


BinaryDocument::~BinaryDocument() = default;


void BinaryDocument::clear()
{
    mRootValue.reset();
    mCurrentParent = nullptr;
    mCurrentKey.reset();
}


void BinaryDocument::addBool(bool value)
{
    add(std::make_unique<Bool>(mCurrentParent, value));
}


void BinaryDocument::addInt64(std::int64_t value)
{
    add(std::make_unique<Int64>(mCurrentParent, value));
}


void BinaryDocument::addDouble(double value)
{
    add(std::make_unique<Double>(mCurrentParent, value));
}


void BinaryDocument::addString(std::string value)
{
    add(std::make_unique<String>(mCurrentParent, std::move(value)));
}


void BinaryDocument::addKey(std::string key)
{
    Assert(! mCurrentKey.has_value());
    Assert(mCurrentParent && mCurrentParent->type == Type::Object);
    mCurrentKey = std::move(key);
}


void BinaryDocument::startArray()
{
    ValuePtr valuePtr = std::make_unique<Array>(mCurrentParent);
    Value* newParent = valuePtr.get();
    add(std::move(valuePtr));
    mCurrentParent = newParent;
}


void BinaryDocument::endArray()
{
    Assert(! mCurrentKey.has_value());
    Assert(mCurrentParent && mCurrentParent->type == Type::Array);
    mCurrentParent = mCurrentParent->parent;
}


void BinaryDocument::startObject()
{
    ValuePtr valuePtr = std::make_unique<Object>(mCurrentParent);
    Value* newParent = valuePtr.get();
    add(std::move(valuePtr));
    mCurrentParent = newParent;
}


void BinaryDocument::endObject()
{
    Assert(! mCurrentKey.has_value());
    Assert(mCurrentParent && mCurrentParent->type == Type::Object);
    mCurrentParent = mCurrentParent->parent;
}


std::string BinaryDocument::encode() const
{
    std::string buffer(1, magic);
    if (mRootValue)
        mRootValue->encode(buffer);
    return buffer;
}


bool BinaryDocument::isBinaryDocument(std::string_view data)
{
    return ! data.empty() && data.front() == magic;
}


void BinaryDocument::decode(std::string_view data)
{
    Assert(isBinaryDocument(data));
    data.remove_prefix(1);
    clear();
    mRootValue = Value::decode(data);
    Assert(data.empty()) << "decoding multiple root values is not supported";
}


const BinaryDocument::Value* BinaryDocument::getRoot() const
{
    return mRootValue.get();
}


void BinaryDocument::add(ValuePtr valuePtr)
{
    Assert(valuePtr->parent == mCurrentParent);

    if (! mCurrentParent)
    {
        Assert(! mRootValue) << "Multiple root values not support";
        Assert(! mCurrentKey.has_value());
        mRootValue = std::move(valuePtr);
        return;
    }

    if (mCurrentParent->type == Type::Array)
    {
        Assert(! mCurrentKey.has_value());
        dynamic_cast<Array&>(*mCurrentParent).elements.emplace_back(std::move(valuePtr));
    }
    else
    {
        Assert(mCurrentParent->type == Type::Object);
        Assert(mCurrentKey.has_value());
        dynamic_cast<Object&>(*mCurrentParent)
            .elements.emplace(std::move(mCurrentKey.value()), std::move(valuePtr));
        mCurrentKey.reset();
    }
}


void BinaryDocument::encodeTypeAndNumber(std::string& buffer, EncType encType, std::uint64_t number)
{
    // Basic binary encoding scheme, tttt is the type, nnn.. is the number, high-byte first:
    //   tttt0nnn
    //   tttt10nn nnnnnnnn
    //   tttt110n nnnnnnnn nnnnnnnn
    //   tttt1110 nnnnnnnn nnnnnnnn nnnnnnnn nnnnnnnn
    //   tttt1111 nnnnnnnn nnnnnnnn nnnnnnnn nnnnnnnn nnnnnnnn nnnnnnnn nnnnnnnn nnnnnnnn
    // But the length of the type is not always four bits, actually the type codes are:
    //   00000000 False          |
    //   00000001 True           |
    //   00000010 Double         | Number
    //   00000011 UuidString     |     always 0
    //   00000100 UrnUuidString  |
    //   00000101 TimeString     |
    //   0000011* Reserved for future use
    //   00001    OidString
    //   00010    UrnOidString
    //   00011    Array
    //   0010     PositiveInt
    //   0011     NegativeInt
    //   010      FrequentString
    //   011      Object
    //   10       String
    //   11       CodedValueObject
    // Numbers of five-bit types can have at most 32 bits, numbers of six-bit types can have at most
    // 16 bits, and numbers of seven-bit types can have at most 8 bits.

    unsigned char data[9];
    int typeBits = 0;
    switch (encType)
    {
        case EncType::False:
            data[0] = 0b00000000;
            typeBits = 8;
            break;
        case EncType::True:
            data[0] = 0b00000001;
            typeBits = 8;
            break;
        case EncType::PositiveInt:
            data[0] = 0b00100000;
            typeBits = 4;
            break;
        case EncType::NegativeInt:
            data[0] = 0b00110000;
            typeBits = 4;
            break;
        case EncType::Double:
            data[0] = 0b00000010;
            typeBits = 8;
            break;
        case EncType::String:
            data[0] = 0b10000000;
            typeBits = 2;
            break;
        case EncType::FrequentString:
            data[0] = 0b01000000;
            typeBits = 3;
            break;
        case EncType::UuidString:
            data[0] = 0b00000011;
            typeBits = 8;
            break;
        case EncType::UrnUuidString:
            data[0] = 0b00000100;
            typeBits = 8;
            break;
        case EncType::OidString:
            data[0] = 0b00001000;
            typeBits = 5;
            break;
        case EncType::UrnOidString:
            data[0] = 0b00010000;
            typeBits = 5;
            break;
        case EncType::TimeString:
            data[0] = 0b00000101;
            typeBits = 8;
            break;
        case EncType::Array:
            data[0] = 0b00011000;
            typeBits = 5;
            break;
        case EncType::Object:
            data[0] = 0b01100000;
            typeBits = 3;
            break;
        case EncType::CodedValueObject:
            data[0] = 0b11000000;
            typeBits = 2;
            break;
    }

    size_t dataLen = 1;
    int nBits = 7 - typeBits;
    if (nBits < 0)
    {
        Assert(number == 0);
    }
    else if (number < (1u << nBits))
    {
        data[0] |= static_cast<unsigned char>(number);
    }
    else if (number < (1u << (nBits + 7)))
    {
        Assert(typeBits <= 7);
        data[0] |= (0b10000000 >> typeBits);
        data[0] |= static_cast<unsigned char>(number >> 8);
        data[1] = static_cast<unsigned char>(number);
        dataLen = 2;
    }
    else if (number < (1u << (nBits + 14)))
    {
        Assert(typeBits <= 6);
        data[0] |= (0b11000000 >> typeBits);
        data[0] |= static_cast<unsigned char>(number >> 16);
        data[1] = static_cast<unsigned char>(number >> 8);
        data[2] = static_cast<unsigned char>(number);
        dataLen = 3;
    }
    else if ((number >> 32) == 0)
    {
        Assert(typeBits <= 5);
        data[0] |= (0b11100000 >> typeBits);
        data[1] = static_cast<unsigned char>(number >> 24);
        data[2] = static_cast<unsigned char>(number >> 16);
        data[3] = static_cast<unsigned char>(number >> 8);
        data[4] = static_cast<unsigned char>(number);
        dataLen = 5;
    }
    else
    {
        Assert(typeBits <= 4);
        data[0] |= (0b11110000 >> typeBits);
        data[1] = static_cast<unsigned char>(number >> 56);
        data[2] = static_cast<unsigned char>(number >> 48);
        data[3] = static_cast<unsigned char>(number >> 40);
        data[4] = static_cast<unsigned char>(number >> 32);
        data[5] = static_cast<unsigned char>(number >> 24);
        data[6] = static_cast<unsigned char>(number >> 16);
        data[7] = static_cast<unsigned char>(number >> 8);
        data[8] = static_cast<unsigned char>(number);
        dataLen = 9;
    }

    buffer.append(reinterpret_cast<const char*>(data), dataLen);
}

//NOLINTBEGIN(readability-function-cognitive-complexity)
std::pair<BinaryDocument::EncType, std::uint64_t> BinaryDocument::decodeTypeAndNumber(
    std::string_view& buffer)
{
    Assert(! buffer.empty());
    const auto* data = reinterpret_cast<const unsigned char*>(buffer.data());
    size_t dataLen = 1;

    EncType type = EncType::False;
    int typeBits = 0;

    if (data[0] & 0b10000000)
    {
        if (data[0] & 0b01000000)
            type = EncType::CodedValueObject;
        else
            type = EncType::String;
        typeBits = 2;
    }
    else if (data[0] & 0b01000000)
    {
        if (data[0] & 0b00100000)
            type = EncType::Object;
        else
            type = EncType::FrequentString;
        typeBits = 3;
    }
    else if (data[0] & 0b00100000)
    {
        if (data[0] & 0b00010000)
            type = EncType::NegativeInt;
        else
            type = EncType::PositiveInt;
        typeBits = 4;
    }
    else if (data[0] & 0b00010000)
    {
        if (data[0] & 0b00001000)
            type = EncType::Array;
        else
            type = EncType::UrnOidString;
        typeBits = 5;
    }
    else if (data[0] & 0b00001000)
    {
        type = EncType::OidString;
        typeBits = 5;
    }
    else if (data[0] & 0b00000100)
    {
        if (data[0] & 0b00000010)
            Failure() << "Found a code that is reserved for future use";
        else if (data[0] & 0b00000001)
            type = EncType::TimeString;
        else
            type = EncType::UrnUuidString;
        typeBits = 8;
    }
    else if (data[0] & 0b00000010)
    {
        if (data[0] & 0b00000001)
            type = EncType::UuidString;
        else
            type = EncType::Double;
        typeBits = 8;
    }
    else
    {
        if (data[0] & 0b00000001)
            type = EncType::True;
        else
            type = EncType::False;
        typeBits = 8;
    }

    const int nBits = 7 - typeBits;
    std::uint64_t number = 0;
    if (nBits < 0)
    {
        number = 0;
    }
    else if ((data[0] & (1 << nBits)) == 0)
    {
        number = data[0] & ((1 << nBits) - 1);
    }
    else if (nBits < 1 || (data[0] & (1 << (nBits - 1))) == 0)
    {
        dataLen = 2;
        Assert(buffer.size() >= dataLen);
        number = nBits <= 1 ? 0u : static_cast<uint64_t>((data[0] & ((1 << (nBits - 1)) - 1)) << 8);
        number |= data[1];
    }
    else if (nBits < 2 || (data[0] & (1 << (nBits - 2))) == 0)
    {
        dataLen = 3;
        Assert(buffer.size() >= dataLen);
        number =
            nBits <= 2 ? 0u : static_cast<uint64_t>((data[0] & ((1 << (nBits - 2)) - 1)) << 16);
        number |= static_cast<uint64_t>(data[1] << 8);
        number |= data[2];
    }
    else if (nBits < 3 || (data[0] & (1 << (nBits - 3))) == 0)
    {
        dataLen = 5;
        Assert(buffer.size() >= dataLen);
        number = static_cast<std::uint64_t>(data[1]) << 24;
        number |= static_cast<std::uint64_t>(data[2]) << 16;
        number |= static_cast<std::uint64_t>(data[3]) << 8;
        number |= static_cast<std::uint64_t>(data[4]);
    }
    else
    {
        dataLen = 9;
        Assert(buffer.size() >= dataLen);
        number = static_cast<std::uint64_t>(data[1]) << 56;
        number |= static_cast<std::uint64_t>(data[2]) << 48;
        number |= static_cast<std::uint64_t>(data[3]) << 40;
        number |= static_cast<std::uint64_t>(data[4]) << 32;
        number |= static_cast<std::uint64_t>(data[5]) << 24;
        number |= static_cast<std::uint64_t>(data[6]) << 16;
        number |= static_cast<std::uint64_t>(data[7]) << 8;
        number |= static_cast<std::uint64_t>(data[8]);
    }

    buffer.remove_prefix(dataLen);
    return {type, number};
}
//NOLINTEND(readability-function-cognitive-complexity)


const std::vector<std::string> BinaryDocument::mAllKeys = {
    // After this went into production, no changes must ever be made except for adding new keys to
    // the end, because the indices are stored in the database (or COS).
    // TODO: Verify that this is complete. Missing keys lead to failures.
    // TODO: If we have more than 128 keys, have the most frequently used 128 ones at the beginning.
    "allDocuments",
    "areaCode",
    "associations",
    "authentication",
    "authenticationType",
    "authority",
    "authorizedId",
    "authorizedName",
    "authors",
    "availabilityStatus",
    "backendData_documentId",
    "backendData_associationId",
    "backendData_associationSourceObject",
    "backendData_createdByVersion",
    "backendData_deApndAssociationId",
    "backendData_deApndAssociationTargetObject",
    "backendData_folderAssociations",
    "backendData_lastWrittenByVersion",
    "backendData_relationId",
    "backendData_relationParentObjectId",
    "backendData_sourceObject",
    "backendData_submissionSetAssociationId",
    "backendData_submissionSetAssociationSourceObject",
    "blacklistedEntryUUIDs",
    "categoryAccess",
    "classCode",
    "code",
    "codeList",
    "codeSystem",
    "comments",
    "confidentialityCodes",
    "contentTypeCode",
    "countryCode",
    "creationTime",
    "datetime",
    "deviceName",
    "displayName",
    "documents",
    "emailAddress",
    "entryUUID",
    "equipmentType",
    "errorInformation",
    "eventCodes",
    "extensionCode",
    "familyName",
    "fdAssociationId",
    "fdAssociationSourceObject",
    "filename",
    "folderCodes",
    "folders",
    "formatCode",
    "givenName",
    "hash",
    "healthcareFacilityTypeCode",
    "hmAssociationId",
    "homeCommunityId",
    "identifier",
    "institution",
    "intendedRecipients",
    "languageCode",
    "lastUpdateTime",
    "legalAuthenticator",
    "limitedMetadata",
    "mimeType",
    "name",
    "nameSuffix",
    "objectId",
    "objectIdentifier",
    "objectName",
    "objectType",
    "organization",
    "otherName",
    "parameterQueryId",
    "patientId",
    "permission",
    "permissions",
    "person",
    "practiceSettingCode",
    "providerName",
    "query",
    "recordId",
    "recordName",
    "referenceIdList",
    "removedPermission",
    "repositoryUniqueId",
    "result",
    "role",
    "serviceStartTime",
    "serviceStopTime",
    "signatureServiceErrorCounter",
    "size",
    "smartCardErrorCounter",
    "sourceEntryUUID",
    "sourceId",
    "sourcePatientId",
    "sourcePatientInfo",
    "specialty",
    "state",
    "submissionSets",
    "submissionTime",
    "subscriberNumber",
    "targetEntryUUID",
    "technicalDocuments",
    "telecommunication",
    "title",
    "trailingSeparators",
    "type",
    "typeCode",
    "uniqueId",
    "unknownErrorCounter",
    "uploader",
    "useCode",
    "userCommonName",
    "userId",
    "userName",
    "whitelistedEntryUUIDs",
    "$patientId",
    "$XDSDocumentEntryClassCode",
    "$XDSDocumentEntryStatus",
    "$XDSDocumentEntryPatientId",
    "$AssociationTypes",
    "$homeCommunityId",
    "$uuid",
    "$XDSDocumentEntryAuthorInstitution",
    "$XDSDocumentEntryAuthorPerson",
    "$XDSDocumentEntryConfidentialityCode",
    "$XDSDocumentEntryCreationTimeFrom",
    "$XDSDocumentEntryCreationTimeTo",
    "$XDSDocumentEntryEntryUUID",
    "$XDSDocumentEntryEventCodeList",
    "$XDSDocumentEntryFormatCode",
    "$XDSDocumentEntryHealthcareFacilityTypeCode",
    "$XDSDocumentEntryPracticeSettingCode",
    "$XDSDocumentEntryReferenceIdList",
    "$XDSDocumentEntryServiceStartTimeFrom",
    "$XDSDocumentEntryServiceStartTimeTo",
    "$XDSDocumentEntryServiceStopTimeFrom",
    "$XDSDocumentEntryServiceStopTimeTo",
    "$XDSDocumentEntryTitle",
    "$XDSDocumentEntryType",
    "$XDSDocumentEntryTypeCode",
    "$XDSDocumentEntryUniqueId",
    "$XDSFolderCodeList",
    "$XDSFolderEntryUUID",
    "$XDSFolderLastUpdateTimeFrom",
    "$XDSFolderLastUpdateTimeTo",
    "$XDSFolderPatientId",
    "$XDSFolderStatus",
    "$XDSFolderUniqueId",
    "$XDSSubmissionSetAuthorPerson",
    "$XDSSubmissionSetContentType",
    "$XDSSubmissionSetEntryUUID",
    "$XDSSubmissionSetPatientId",
    "$XDSSubmissionSetSourceId",
    "$XDSSubmissionSetStatus",
    "$XDSSubmissionSetSubmissionTimeFrom",
    "$XDSSubmissionSetSubmissionTimeTo",
    "$XDSSubmissionSetUniqueId",
    "PRACTITIONER",
    "HOSPITAL",
    "LABORATORY",
    "PHYSIOTHERAPY",
    "PSYCHOTHERAPY",
    "DERMATOLOGY",
    "GYNAECOLOGY_UROLOGY",
    "DENTISTRY_OMS",
    "OTHER_MEDICAL",
    "OTHER_NON_MEDICAL",
    "EMP",
    "NFD",
    "EAB",
    "DENTALRECORD",
    "CHILDSRECORD",
    "MOTHERSRECORD",
    "VACCINATION",
    "PATIENTDOC",
    "EGA",
    "RECEIPT",
    "DIGA",
    "CARE",
    "PRESCRIPTION",
    "EAU",
    "OTHER",
    "TECHNICAL",
    // entitlements / blocked user policy:
    "at",
    "actorId",
    "data",
    "issued",
    "oid",
    "validTo",
};


const BinaryDocument::StringRefToIndexMap BinaryDocument::mAllKeyIndices = []() {
    StringRefToIndexMap initMap;
    for (size_t index = 0; index < mAllKeys.size(); index++)
        initMap.emplace(mAllKeys[index], index);
    return initMap;
}();


const std::vector<std::string> BinaryDocument::mFrequentStrings = {
    // After this went into production, no changes must ever be made except for adding new strings
    // to the end, because the indices are stored in the database (or COS).
    // TODO: This is currently just a quick selection of all strings from AvailabilityStatus,
    // EventOutcomeIndicator, MimeType.cxx, EventType, and UUIDs.hxx (partly with and without urn
    // prefix). Maybe this is too much, maybe it is not enough. The most frequently used strings
    // should at the beginning.
    "DEPRECATED",
    "APPROVED",
    "SUCCESS",
    "ERROR",
    "application/xop+xml",
    "application/soap+xml",
    "application/xml",
    "text/plain;charset=UTF-8",
    "text/plain",
    "text/rtf",
    "application/octet-stream",
    "application/json",
    "application/hl7-v3",
    "image/jpeg",
    "image/png",
    "image/tiff",
    "application/pdf",
    "application/xacml+xml",
    "multipart/related",
    "application/pkcs7-mime",
    "application/fhir+xml",
    "AUTHENTICATION_INSURANT__LOGIN_CREATE_TOKEN",
    "AUTHORIZATION__GET_AUTHORIZATION_KEY",
    "AUTHORIZATION_MANAGEMENT_INSURANT__DELETE_AUTHORIZATION_KEY",
    "AUTHORIZATION_MANAGEMENT_SERVICE__CLEANUP_PERMISSIONS",
    "AUTHORIZATION_MANAGEMENT_INSURANT__GET_AUDIT_EVENTS",
    "AUTHORIZATION_MANAGEMENT_INSURANT__GET_SIGNED_AUDIT_EVENTS",
    "AUTHORIZATION_MANAGEMENT_INSURANT__PUT_NOTIFICATION_INFO",
    "AUTHORIZATION_MANAGEMENT_PROVIDER__PUT_NOTIFICATION_INFO",
    "AUTHORIZATION_MANAGEMENT_INSURANT__GET_NOTIFICATION_INFO",
    "AUTHORIZATION_MANAGEMENT_INSURANT__DEVICE_MANAGEMENT",
    "AUTHORIZATION_MANAGEMENT_INSURANT__START_KEY_CHANGE",
    "AUTHORIZATION_MANAGEMENT_INSURANT__FINISH_KEY_CHANGE",
    "DOCUMENT_MANAGEMENT__CROSS_GATEWAY_DOCUMENT_PROVIDE",
    "DOCUMENT_MANAGEMENT__CROSS_GATEWAY_DOCUMENT_PROVIDE_ADD_POLICY_DOCUMENT",
    "DOCUMENT_MANAGEMENT__CROSS_GATEWAY_DOCUMENT_PROVIDE_CHANGE_POLICY_DOCUMENT",
    "DOCUMENT_MANAGEMENT__CROSS_GATEWAY_QUERY",
    "DOCUMENT_MANAGEMENT__REMOVE_DOCUMENTS",
    "DOCUMENT_MANAGEMENT__CROSS_GATEWAY_RETRIEVE",
    "DOCUMENT_MANAGEMENT__RESTRICTED_UPDATE_DOCUMENT_SET",
    "DOCUMENT_MANAGEMENT__REMOVE_METADATA",
    "DOCUMENT_MANAGEMENT_INSURANT__PROVIDE_AND_REGISTER_DOCUMENT_SET",
    "DOCUMENT_MANAGEMENT_INSURANT__PROVIDE_AND_REGISTER_DOCUMENT_SET_ADD_POLICY_DOCUMENT",
    "DOCUMENT_MANAGEMENT_INSURANT__PROVIDE_AND_REGISTER_DOCUMENT_SET_CHANGE_POLICY_DOCUMENT",
    "DOCUMENT_MANAGEMENT_INSURANT__REGISTRY_STORED_QUERY",
    "DOCUMENT_MANAGEMENT_INSURANT__REMOVE_DOCUMENTS",
    "DOCUMENT_MANAGEMENT_INSURANT__RETRIEVE_DOCUMENT_SET",
    "ACCOUNT_MANAGEMENT_INSURANT__SUSPEND_ACCOUNT",
    "ACCOUNT_MANAGEMENT_INSURANT__RESUME_ACCOUNT",
    "ACCOUNT_MANAGEMENT_INSURANT__GET_AUDIT_EVENTS",
    "ACCOUNT_MANAGEMENT_INSURANT__GET_SIGNED_AUDIT_EVENTS",
    "DOCUMENT_MANAGEMENT_INSURANT__REMOVE_METADATA",
    "DOCUMENT_MANAGEMENT_INSURANT__REMOVE_METADATA_POLICY_DOCUMENT",
    "DOCUMENT_MANAGEMENT_INSURANT__RESTRICTED_UPDATE_DOCUMENT_SET",
    "DOCUMENT_MANAGEMENT_INSURANCE__PROVIDE_AND_REGISTER_DOCUMENT_SET",
    "AUTHORIZATION__ROLL_BACK_KEY_CHANGE",
    "a54d6aa5-d40d-43f9-88c5-b4633d873bdd",
    "a7058bb9-b4e4-4307-ba5b-e3f0ab85e12d",
    "aa543740-bdda-424e-8c96-df4873be8500",
    "6b5aea1a-874d-4603-a4bc-96a0a7b38446",
    "554ac39e-e3fe-47fe-b233-965d2a147832",
    "96fdda7c-d067-4183-912e-bf5ee74998a8",
    "5003a9db-8d8d-49e6-bf0c-990e34ac7707",
    "7edca82f-054d-47f2-a032-9b2a5b5186c1",
    "34268e47-fdf5-41a6-ba33-82133c465248",
    "93606bcf-9494-43ec-9b4e-a7748d1a838d",
    "41a5887f-8865-4c09-adf7-e362475b143a",
    "f4f85eac-e6cb-4883-b524-f2705394840f",
    "2c6b8cb7-8b2a-4051-b291-b1ae6a575ef4",
    "a09d5840-386c-46f2-b5ad-9c3699a4309d",
    "f33fb8ac-18af-42cc-ae0e-ed0b0bdb91e1",
    "58a6f841-87b3-4a3e-92fd-a8ffeff98427",
    "cccf5598-8b07-4b77-a05e-ae952c785ead",
    "f0306f51-975f-434e-a61c-c59651d33983",
    "2e82c1f6-a085-4c72-9da3-8640a32e42ab",
    "bbbe4487-7a96-3a66-f94d-fd841b685ffc",
    "ab9b591b-83ab-4d03-8f5d-f93b1fb92e85",
    "d9d542f3-6cc4-48b6-8870-ea235fbc94c2",
    "1ba97051-7806-41a8-a48b-8fce7af683c5",
    "f64ffdf0-4b97-4e06-b79f-a52b38ec2f8a",
    "75df8f67-9973-4fbe-a900-df66cefecc5a",
    "2c144a76-29a9-4b7c-af54-b25409fe7d03",
    "abd807a3-4432-4053-87b4-fd82c643d1f3",
    "urn:uuid:a54d6aa5-d40d-43f9-88c5-b4633d873bdd",
    "urn:uuid:a7058bb9-b4e4-4307-ba5b-e3f0ab85e12d",
    "urn:uuid:aa543740-bdda-424e-8c96-df4873be8500",
    "urn:uuid:6b5aea1a-874d-4603-a4bc-96a0a7b38446",
    "urn:uuid:554ac39e-e3fe-47fe-b233-965d2a147832",
    "urn:uuid:96fdda7c-d067-4183-912e-bf5ee74998a8",
    "urn:uuid:5003a9db-8d8d-49e6-bf0c-990e34ac7707",
    "urn:uuid:7edca82f-054d-47f2-a032-9b2a5b5186c1",
    "urn:uuid:34268e47-fdf5-41a6-ba33-82133c465248",
    "urn:uuid:93606bcf-9494-43ec-9b4e-a7748d1a838d",
    "urn:uuid:41a5887f-8865-4c09-adf7-e362475b143a",
    "urn:uuid:f4f85eac-e6cb-4883-b524-f2705394840f",
    "urn:uuid:2c6b8cb7-8b2a-4051-b291-b1ae6a575ef4",
    "urn:uuid:a09d5840-386c-46f2-b5ad-9c3699a4309d",
    "urn:uuid:f33fb8ac-18af-42cc-ae0e-ed0b0bdb91e1",
    "urn:uuid:58a6f841-87b3-4a3e-92fd-a8ffeff98427",
    "urn:uuid:cccf5598-8b07-4b77-a05e-ae952c785ead",
    "urn:uuid:f0306f51-975f-434e-a61c-c59651d33983",
    "urn:uuid:2e82c1f6-a085-4c72-9da3-8640a32e42ab",
    "urn:uuid:bbbe4487-7a96-3a66-f94d-fd841b685ffc",
    "urn:uuid:ab9b591b-83ab-4d03-8f5d-f93b1fb92e85",
    "urn:uuid:d9d542f3-6cc4-48b6-8870-ea235fbc94c2",
    "urn:uuid:1ba97051-7806-41a8-a48b-8fce7af683c5",
    "urn:uuid:f64ffdf0-4b97-4e06-b79f-a52b38ec2f8a",
    "urn:uuid:75df8f67-9973-4fbe-a900-df66cefecc5a",
    "urn:uuid:2c144a76-29a9-4b7c-af54-b25409fe7d03",
    "urn:uuid:abd807a3-4432-4053-87b4-fd82c643d1f3",
    "urn:ihe:iti:xds:2013:referenceIdList",
    "urn:oasis:names:tc:ebxml-regrep:ObjectType:RegistryObject:RegistryPackage",
    "urn:oasis:names:tc:ebxml-regrep:ObjectType:RegistryObject:Classification",
    "urn:oasis:names:tc:ebxml-regrep:ObjectType:RegistryObject:ExternalIdentifier",
    "urn:oasis:names:tc:ebxml-regrep:ObjectType:RegistryObject:Association",
    "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success",
    "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Failure",
    "urn:ihe:iti:2007:ResponseStatusType:PartialSuccess",
    "urn:oasis:names:tc:ebxml-regrep:AssociationType:HasMember",
    "urn:ihe:iti:2007:AssociationType:RPLC",
    "urn:ihe:iti:2007:AssociationType:XFRM",
    "urn:ihe:iti:2007:AssociationType:APND",
    "urn:ihe:iti:2007:AssociationType:XFRM_RPLC",
    "urn:ihe:iti:2007:AssociationType:signs",
    "urn:ihe:iti:2010:AssociationType:IsSnapshotOf",
    "1.2.276.0.76.4.8",
    "1.2.276.0.76.4.16",
    "1.2.276.0.76.4.296",
    "1.2.276.0.76.4.188",
};


const BinaryDocument::StringRefToIndexMap BinaryDocument::mFrequentStringIndices = []() {
    StringRefToIndexMap initMap;
    for (size_t index = 0; index < mFrequentStrings.size(); index++)
        initMap.emplace(mFrequentStrings[index], index);
    return initMap;
}();


const std::vector<BinaryDocument::CodedValueEntry> BinaryDocument::mCodedValues = {
    // After this went into production, no changes must ever be made except for adding new entries
    // to the end, because the indices are stored in the database (or COS).
    // CONFIDENTIALITY_CODES
    {"LEI", "1.2.276.0.76.5.491", "Dokument einer Leistungserbringerinstitution"},
    {"KTR", "1.2.276.0.76.5.491", "Dokument eines Kostenträgers"},
    {"PAT", "1.2.276.0.76.5.491", "Dokument eines Versicherten"},
    {"LE\u00c4",
     "1.2.276.0.76.5.491",
     "Leistungserbringeräquivalentes Dokument eines Versicherten oder Kostenträgers"},
    {"N", "2.16.840.1.113883.5.25", "normal"},
    {"R", "2.16.840.1.113883.5.25", "restricted"},
    {"V", "2.16.840.1.113883.5.25", "very restricted"},
    {"PV", "1.3.6.1.4.1.19376.3.276.1.5.10", "gesperrt"},
    {"PR", "1.3.6.1.4.1.19376.3.276.1.5.10", "erhöhte Vertraulichkeit"},
    {"PN", "1.3.6.1.4.1.19376.3.276.1.5.10", "übliche Vertraulichkeit"},
    // CLASS_CODES
    {"57016-8", "2.16.840.1.113883.6.1", "Patienteneinverständniserklärung"},
    {"ADM", "1.3.6.1.4.1.19376.3.276.1.5.8", "Administratives Dokument"},
    {"ANF", "1.3.6.1.4.1.19376.3.276.1.5.8", "Anforderung"},
    {"ASM", "1.3.6.1.4.1.19376.3.276.1.5.8", "Assessment"},
    {"BEF", "1.3.6.1.4.1.19376.3.276.1.5.8", "Befundbericht"},
    {"BIL", "1.3.6.1.4.1.19376.3.276.1.5.8", "Bilddaten"},
    {"BRI", "1.3.6.1.4.1.19376.3.276.1.5.8", "Brief"},
    {"DOK", "1.3.6.1.4.1.19376.3.276.1.5.8", "Dokumente ohne besondere Form (Notizen)"},
    {"DUR", "1.3.6.1.4.1.19376.3.276.1.5.8", "Durchführungsprotokoll"},
    {"FOR", "1.3.6.1.4.1.19376.3.276.1.5.8", "Forschung"},
    {"GUT", "1.3.6.1.4.1.19376.3.276.1.5.8", "Gutachten und Qualitätsmanagement"},
    {"LAB", "1.3.6.1.4.1.19376.3.276.1.5.8", "Laborergebnisse"},
    {"AUS", "1.3.6.1.4.1.19376.3.276.1.5.8", "Medizinischer Ausweis"},
    {"PLA", "1.3.6.1.4.1.19376.3.276.1.5.8", "Planungsdokument"},
    {"VER", "1.3.6.1.4.1.19376.3.276.1.5.8", "Verordnung"},
    {"VID", "1.3.6.1.4.1.19376.3.276.1.5.8", "Videodaten"},
    {"MED", "1.3.6.1.4.1.19376.3.276.1.5.8", "Medikation"},
    // FOLDER_CODES
    {"practitioner", "1.2.276.0.76.5.511", "Hausarzt/Hausärztin"},
    {"hospital", "1.2.276.0.76.5.511", "Krankenhaus"},
    {"laboratory", "1.2.276.0.76.5.511", "Labor und Humangenetik"},
    {"physiotherapy", "1.2.276.0.76.5.511", "Physiotherapeuten"},
    {"psychotherapy", "1.2.276.0.76.5.511", "Psychotherapeuten"},
    {"dermatology", "1.2.276.0.76.5.511", "Dermatologie"},
    {"gynaecology_urology", "1.2.276.0.76.5.511", "Urologie/Gynäkologie"},
    {"dentistry_oms", "1.2.276.0.76.5.511", "Zahnheilkunde und Mund-Kiefer-Gesichtschirurgie"},
    {"other_medical", "1.2.276.0.76.5.511", "Weitere Fachärzte/Fachärztinnen"},
    {"other_non_medical", "1.2.276.0.76.5.511", "Weitere nicht-ärztliche Berufe"},
    {"emp", "1.2.276.0.76.5.512", "Elektronischer Medikationsplan"},
    {"nfd", "1.2.276.0.76.5.512", "Notfalldaten"},
    {"eab", "1.2.276.0.76.5.512", "eArztbrief"},
    {"dentalrecord", "1.2.276.0.76.5.512", "Zahnbonusheft"},
    {"childsrecord", "1.2.276.0.76.5.512", "Kinderuntersuchungsheft"},
    {"mothersrecord", "1.2.276.0.76.5.512", "Schwangerschaft und Geburt"},
    {"vaccination", "1.2.276.0.76.5.512", "Impfpass"},
    {"patientdoc", "1.2.276.0.76.5.512", "vom Versicherten eingestellte Dokumente"},
    {"ega", "1.2.276.0.76.5.512", "Elektronische Gesundheitsakte (eGA)"},
    {"receipt", "1.2.276.0.76.5.512", "Quittungen"},
    {"diga", "1.2.276.0.76.5.512", "DiGA"},
    {"care", "1.2.276.0.76.5.512", "Pflegedokumente"},
    {"prescription", "1.2.276.0.76.5.512", "Verordnungsdaten und Dispensierinformationen"},
    {"eau", "1.2.276.0.76.5.512", "Elektronische Arbeitsunfähigkeitsbescheinigungen"},
    {"other",
     "1.2.276.0.76.5.512",
     "in andere Kategorien nicht einzuordnende Dokumente, eDMPs sowie Telemedizinisches "
     "Monitoring"},
    {"technical", "1.2.276.0.76.5.512", "technische Dokumente"},
    // HEALTHCARE_FACILITY_TYPE_CODES
    {"APD", "1.3.6.1.4.1.19376.3.276.1.5.2", "Ambulanter Pflegedienst"},
    {"APO", "1.3.6.1.4.1.19376.3.276.1.5.2", "Apotheke"},
    {"BER", "1.3.6.1.4.1.19376.3.276.1.5.2", "Ärztlicher Bereitschaftsdienst"},
    {"PRA", "1.3.6.1.4.1.19376.3.276.1.5.2", "Arztpraxis"},
    {"BAA", "1.3.6.1.4.1.19376.3.276.1.5.2", "Betriebsärztliche Abteilung"},
    {"BHR", "1.3.6.1.4.1.19376.3.276.1.5.2", "Gesundheitsbehörde"},
    {"HEB", "1.3.6.1.4.1.19376.3.276.1.5.2", "Hebamme/Geburtshaus"},
    {"HOS", "1.3.6.1.4.1.19376.3.276.1.5.2", "Hospiz"},
    {"KHS", "1.3.6.1.4.1.19376.3.276.1.5.2", "Krankenhaus"},
    {"MVZ", "1.3.6.1.4.1.19376.3.276.1.5.2", "Medizinisches Versorgungszentrum"},
    {"HAN", "1.3.6.1.4.1.19376.3.276.1.5.2", "Medizinisch-technisches Handwerk"},
    {"REH", "1.3.6.1.4.1.19376.3.276.1.5.2", "Medizinische Rehabilitation"},
    {"HEI", "1.3.6.1.4.1.19376.3.276.1.5.2", "Nicht-ärztliche Heilberufs-Praxis"},
    {"PFL", "1.3.6.1.4.1.19376.3.276.1.5.2", "Pflegeheim"},
    {"RTN", "1.3.6.1.4.1.19376.3.276.1.5.2", "Rettungsdienst"},
    {"SEL", "1.3.6.1.4.1.19376.3.276.1.5.2", "Selbsthilfe"},
    {"TMZ", "1.3.6.1.4.1.19376.3.276.1.5.2", "Telemedizinisches Zentrum"},
    {"BIL", "1.3.6.1.4.1.19376.3.276.1.5.3", "Bildungseinrichtung"},
    {"FOR", "1.3.6.1.4.1.19376.3.276.1.5.3", "Forschungseinrichtung"},
    {"GEN", "1.3.6.1.4.1.19376.3.276.1.5.3", "Gen-Analysedienste"},
    {"MDK", "1.3.6.1.4.1.19376.3.276.1.5.3", "Medizinischer Dienst der Krankenversicherung"},
    {"PAT", "1.3.6.1.4.1.19376.3.276.1.5.3", "Patient außerhalb der Betreuung"},
    {"SPE", "1.3.6.1.4.1.19376.3.276.1.5.3", "Spendedienste"},
    {"VER", "1.3.6.1.4.1.19376.3.276.1.5.3", "Versicherungsträger"},
    // CONTENT_TYPE_CODES
    {"1", "1.3.6.1.4.1.19376.3.276.1.5.12", "Patientenkontakt"},
    {"2", "1.3.6.1.4.1.19376.3.276.1.5.12", "Verlegung"},
    {"3", "1.3.6.1.4.1.19376.3.276.1.5.12", "Entlassung"},
    {"4", "1.3.6.1.4.1.19376.3.276.1.5.12", "Überweisung/Einweisung"},
    {"5", "1.3.6.1.4.1.19376.3.276.1.5.12", "Aufnahme"},
    {"6", "1.3.6.1.4.1.19376.3.276.1.5.12", "Anforderung"},
    {"7", "1.3.6.1.4.1.19376.3.276.1.5.12", "Auf Anfrage"},
    {"8", "1.3.6.1.4.1.19376.3.276.1.5.12", "Veranlassung durch Patient"},
    {"9", "1.3.6.1.4.1.19376.3.276.1.5.12", "Konsil/Zweitmeinung"},
    {"10", "1.3.6.1.4.1.19376.3.276.1.5.12", "Systemwechsel/Archivierung"},
    {"11", "1.3.6.1.4.1.19376.3.276.1.5.12", "Monitoring"},
    // AUTHOR_ROLES
    {"1", "1.3.6.1.4.1.19376.3.276.1.5.13", "Einweiser"},
    {"2", "1.3.6.1.4.1.19376.3.276.1.5.13", "Entlassender"},
    {"3", "1.3.6.1.4.1.19376.3.276.1.5.13", "Überweiser"},
    {"4", "1.3.6.1.4.1.19376.3.276.1.5.13", "Durchführender"},
    {"5", "1.3.6.1.4.1.19376.3.276.1.5.13", "durchführendes Gerät"},
    {"6", "1.3.6.1.4.1.19376.3.276.1.5.13", "Betreuer"},
    {"7", "1.3.6.1.4.1.19376.3.276.1.5.13", "Pflegender"},
    {"17", "1.3.6.1.4.1.19376.3.276.1.5.13", "Begutachtender"},
    {"8", "1.3.6.1.4.1.19376.3.276.1.5.13", "Behandler"},
    {"9", "1.3.6.1.4.1.19376.3.276.1.5.13", "Erstbehandler außerhalb einer Einrichtung"},
    {"10", "1.3.6.1.4.1.19376.3.276.1.5.13", "Bereitstellender"},
    {"11", "1.3.6.1.4.1.19376.3.276.1.5.13", "Dokumentierender"},
    {"12", "1.3.6.1.4.1.19376.3.276.1.5.13", "dokumentierendes Gerät"},
    {"13", "1.3.6.1.4.1.19376.3.276.1.5.13", "Validierer"},
    {"14", "1.3.6.1.4.1.19376.3.276.1.5.13", "Gesetzlich Verantwortlicher"},
    {"15", "1.3.6.1.4.1.19376.3.276.1.5.13", "Beratender"},
    {"16", "1.3.6.1.4.1.19376.3.276.1.5.13", "Informierender"},
    {"101", "1.3.6.1.4.1.19376.3.276.1.5.14", "Hausarzt"},
    {"102", "1.3.6.1.4.1.19376.3.276.1.5.14", "Patient"},
    {"103", "1.3.6.1.4.1.19376.3.276.1.5.14", "Arbeitgebervertreter"},
    {"104", "1.3.6.1.4.1.19376.3.276.1.5.14", "Primärbetreuer (langfristig)"},
    {"105", "1.3.6.1.4.1.19376.3.276.1.5.14", "Kostenträgerverteter"},
    // TYPE_CODES
    {"ABRE", "1.3.6.1.4.1.19376.3.276.1.5.9", "Abrechnungsdokumente"},
    {"ADCH", "1.3.6.1.4.1.19376.3.276.1.5.9", "Administrative Checklisten"},
    {"ANTR", "1.3.6.1.4.1.19376.3.276.1.5.9", "Anträge und deren Bescheide"},
    {"ANAE", "1.3.6.1.4.1.19376.3.276.1.5.9", "Anästhesiedokumente"},
    {"BERI", "1.3.6.1.4.1.19376.3.276.1.5.9", "Arztberichte"},
    {"BESC", "1.3.6.1.4.1.19376.3.276.1.5.9", "Ärztliche Bescheinigungen"},
    {"BEFU", "1.3.6.1.4.1.19376.3.276.1.5.9", "Ergebnisse Diagnostik"},
    {"BSTR", "1.3.6.1.4.1.19376.3.276.1.5.9", "Bestrahlungsdokumentation"},
    {"AUFN", "1.3.6.1.4.1.19376.3.276.1.5.9", "Einweisungs- und Aufnahmedokumente"},
    {"EINW", "1.3.6.1.4.1.19376.3.276.1.5.9", "Einwilligungen/Aufklärungen"},
    {"FUNK", "1.3.6.1.4.1.19376.3.276.1.5.9", "Ergebnisse Funktionsdiagnostik"},
    {"BILD", "1.3.6.1.4.1.19376.3.276.1.5.9", "Ergebnisse bildgebender Diagnostik"},
    {"FALL", "1.3.6.1.4.1.19376.3.276.1.5.9", "Fallbesprechungen"},
    {"FOTO", "1.3.6.1.4.1.19376.3.276.1.5.9", "Fotodokumentation"},
    {"FPRO", "1.3.6.1.4.1.19376.3.276.1.5.9", "Therapiedokumentation"},
    {"IMMU", "1.3.6.1.4.1.19376.3.276.1.5.9", "Ergebnisse Immunologie"},
    {"INTS", "1.3.6.1.4.1.19376.3.276.1.5.9", "Intensivmedizinische Dokumente"},
    {"KOMP", "1.3.6.1.4.1.19376.3.276.1.5.9", "Komplexbehandlungsbögen"},
    {"MEDI", "1.3.6.1.4.1.19376.3.276.1.5.9", "Medikamentöse Therapien"},
    {"MKRO", "1.3.6.1.4.1.19376.3.276.1.5.9", "Ergebnisse Mikrobiologie"},
    {"OPDK", "1.3.6.1.4.1.19376.3.276.1.5.9", "OP-Dokumente"},
    {"ONKO", "1.3.6.1.4.1.19376.3.276.1.5.9", "Onkologische Dokumente"},
    {"PATH", "1.3.6.1.4.1.19376.3.276.1.5.9", "Pathologiebefundberichte"},
    {"PATD", "1.3.6.1.4.1.19376.3.276.1.5.9", "Patienteneigene Dokumente"},
    {"PATI", "1.3.6.1.4.1.19376.3.276.1.5.9", "Patienteninformationen"},
    {"PFLG", "1.3.6.1.4.1.19376.3.276.1.5.9", "Pflegedokumentation"},
    {"QUAL", "1.3.6.1.4.1.19376.3.276.1.5.9", "Qualitätssicherung"},
    {"RETT", "1.3.6.1.4.1.19376.3.276.1.5.9", "Rettungsdienstliche Dokumente"},
    {"SCHR", "1.3.6.1.4.1.19376.3.276.1.5.9", "Schriftwechsel (administrativ)"},
    {"GEBU", "1.3.6.1.4.1.19376.3.276.1.5.9", "Schwangerschafts- und Geburtsdokumentation"},
    {"SOZI", "1.3.6.1.4.1.19376.3.276.1.5.9", "Sozialdienstdokumente"},
    {"STUD", "1.3.6.1.4.1.19376.3.276.1.5.9", "Studiendokumente"},
    {"TRFU", "1.3.6.1.4.1.19376.3.276.1.5.9", "Transfusionsdokumente"},
    {"TRPL", "1.3.6.1.4.1.19376.3.276.1.5.9", "Transplantationsdokumente"},
    {"VERO", "1.3.6.1.4.1.19376.3.276.1.5.9", "Verordnungen"},
    {"VERT", "1.3.6.1.4.1.19376.3.276.1.5.9", "Verträge"},
    {"VIRO", "1.3.6.1.4.1.19376.3.276.1.5.9", "Ergebnisse Virologie"},
    {"WUND", "1.3.6.1.4.1.19376.3.276.1.5.9", "Wunddokumentation"},
    {"57016-8", "2.16.840.1.113883.6.1", "Patienteneinverständniserklärung"},
    // FORMAT_CODES
    {"urn:ihe:pcc:xds-ms:2007", "1.3.6.1.4.1.19376.1.2.3", "Medical Summaries (XDS-MS)"},
    {"urn:ihe:pcc:xphr:2007",
     "1.3.6.1.4.1.19376.1.2.3",
     "Exchange of Personal Health Records (XPHR)"},
    {"urn:ihe:pcc:edr:2007", "1.3.6.1.4.1.19376.1.2.3", "Emergency Department Referral (EDR)"},
    {"urn:ihe:pcc:aps:2007", "1.3.6.1.4.1.19376.1.2.3", "Antepartum Summary (APS)"},
    {"urn:ihe:pcc:edes:2007",
     "1.3.6.1.4.1.19376.1.2.3",
     "Emergency Department Encounter Summary (EDES)"},
    {"urn:ihe:pcc:aphp:2008", "1.3.6.1.4.1.19376.1.2.3", "Antepartum History and Physical (APHP)"},
    {"urn:ihe:pcc:apl:2008", "1.3.6.1.4.1.19376.1.2.3", "Antepartum Laboratory (APL)"},
    {"urn:ihe:pcc:ape:2008", "1.3.6.1.4.1.19376.1.2.3", "Antepartum Education (APE)"},
    {"urn:ihe:pcc:ic:2009", "1.3.6.1.4.1.19376.1.2.3", "Immunization Content (IC)"},
    {"urn:ihe:pcc:cm:2008", "1.3.6.1.4.1.19376.1.2.3", "Care Management (CM)"},
    {"urn:ihe:pcc:tn:2007", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:tn:2007)"},
    {"urn:ihe:pcc:nn:2007", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:nn:2007)"},
    {"urn:ihe:pcc:ctn:2007", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:ctn:2007)"},
    {"urn:ihe:pcc:edpn:2007", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:edpn:2007)"},
    {"urn:ihe:pcc:hp:2008", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:hp:2008)"},
    {"urn:ihe:pcc:ldhp:2009", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:ldhp:2009)"},
    {"urn:ihe:pcc:lds:2009", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:lds:2009)"},
    {"urn:ihe:pcc:mds:2009", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:mds:2009)"},
    {"urn:ihe:pcc:nds:2010", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:nds:2010)"},
    {"urn:ihe:pcc:ppvs:2010", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:ppvs:2010)"},
    {"urn:ihe:pcc:trs:2011", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:trs:2011)"},
    {"urn:ihe:pcc:ets:2011", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:ets:2011)"},
    {"urn:ihe:pcc:its:2011", "1.3.6.1.4.1.19376.1.2.3", "??? (urn:ihe:pcc:its:2011)"},
    {"urn:ihe:iti:xds-sd:pdf:2008", "1.3.6.1.4.1.19376.1.2.3", "Scanned Documents (PDF)"},
    {"urn:ihe:iti:xds-sd:text:2008", "1.3.6.1.4.1.19376.1.2.3", "Scanned Documents (text)"},
    {"urn:ihe:iti:bppc:2007", "1.3.6.1.4.1.19376.1.2.3", "Basic Patient Privacy Consents"},
    {"urn:ihe:iti:bppc-sd:2007",
     "1.3.6.1.4.1.19376.1.2.3",
     "Basic Patient Privacy Consents with Scanned Document"},
    {"urn:ihe:iti:appc:2016:consent", "1.3.6.1.4.1.19376.1.2.3", "APPC Privacy Consent Document"},
    {"urn:ihe:iti:xdw:2011:workflowDoc", "1.3.6.1.4.1.19376.1.2.3", "XDW Workflow Document"},
    {"urn:ihe:iti:dsg:detached:2014", "1.3.6.1.4.1.19376.1.2.3", "DSG Detached Document"},
    {"urn:ihe:iti:dsg:enveloping:2014", "1.3.6.1.4.1.19376.1.2.3", "DSG Enveloping Document"},
    {"urn:ihe:lab:xd-lab:2008", "1.3.6.1.4.1.19376.1.2.3", "CDA Laboratory Report"},
    {"urn:ihe:rad:TEXT", "1.3.6.1.4.1.19376.1.2.3", "XDS-I CDA Wrapped Text Report (XDS-I)"},
    {"urn:ihe:rad:PDF", "1.3.6.1.4.1.19376.1.2.3", "XDS-I PDF (XDS-I)"},
    {"urn:ihe:rad:CDA:ImagingReportStructuredHeadings:2013",
     "1.3.6.1.4.1.19376.1.2.3",
     "XDS-I Imaging Report with Structured Headings (XDS-I)"},
    {"urn:ihe:card:CRC:2012", "1.3.6.1.4.1.19376.1.2.3", "Cardiology ??? (CRC)"},
    {"urn:ihe:card:EPRC-IE:2014", "1.3.6.1.4.1.19376.1.2.3", "Cardiology ??? (EPRC-IE)"},
    {"urn:ihe:card:imaging:2011", "1.3.6.1.4.1.19376.1.2.3", "Cardiac Imaging Report"},
    {"urn:ihe:dent:TEXT", "1.3.6.1.4.1.19376.1.2.3", "Dental CDA Wrapped Text Report (DENT)"},
    {"urn:ihe:dent:PDF", "1.3.6.1.4.1.19376.1.2.3", "Dental PDF (DENT)"},
    {"urn:ihe:dent:CDA:ImagingReportStructuredHeadings:2013",
     "1.3.6.1.4.1.19376.1.2.3",
     "Dental Imaging Report with Structured Headings (DENT)"},
    {"urn:ihe:palm:apsr:2016",
     "1.3.6.1.4.1.19376.1.2.3",
     "Anatomic Pathology Structured Report (APSR)"},
    {"urn:ihe:pharm:pre:2010", "1.3.6.1.4.1.19376.1.2.3", "Pharmacy ??? (urn:ihe:pharm:pre:2010)"},
    {"urn:ihe:pharm:padv:2010",
     "1.3.6.1.4.1.19376.1.2.3",
     "Pharmacy ??? (urn:ihe:pharm:padv:2010)"},
    {"urn:ihe:pharm:dis:2010", "1.3.6.1.4.1.19376.1.2.3", "Pharmacy ??? (urn:ihe:pharm:dis:2010)"},
    {"urn:ihe:pharm:pml:2013", "1.3.6.1.4.1.19376.1.2.3", "Pharmacy ??? (urn:ihe:pharm:pml:2013)"},
    {"urn:ihe:iti:xds:2017:mimeTypeSufficient",
     "1.3.6.1.4.1.19376.1.2.3",
     "Format aus MIME Type ableitbar"},
    {"urn:ihe-d:ig:Entlassmanagementbrief:2018",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Entlassmanagementbrief"},
    {"urn:ihe-d:ig:NotaufnahmeregisterTraumaModul:2017",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "NotaufnahmeregisterTraumamodul"},
    {"urn:ihe-d:ig:NotaufnahmeregisterBasisModul:2017",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "NotaufnahmeregisterBasismodul"},
    {"urn:ihe-d:ig:MeldepflichtigeKrankheitenLabor:2014",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Meldepflichtige Krankheiten: Labormeldung"},
    {"urn:ihe-d:ig:MeldepflichtigeKrankheitenArzt:2013",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Übermittlung meldepflichtiger Krankheiten – Arztmeldung"},
    {"urn:ihe-d:ig:RehaEntlassbrief:2009",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Ärztlicher Reha-Entlassungsbericht"},
    {"urn:ihe-d:ig:KurzberichtUeberleitungKrankenhaus:2016",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Überleitungsmanagement Ärztlicher Kurzbericht über den Krankenhausaufenthalt"},
    {"urn:ihe-d:ig:KurzberichtUeberleitungNiedergelassenerArzt:2016",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Überleitungsmanagement Ärztlicher Kurzbericht des niedergelassenen Arztes"},
    {"urn:ihe-d:ig:Medikationsplan:2015", "1.3.6.1.4.1.19376.3.276.1.5.6", "Medikationsplan"},
    {"urn:ihe-d:ig:Arztbriefplus:2017", "1.3.6.1.4.1.19376.3.276.1.5.6", "Arztbrief plus"},
    {"urn:ihe-d:ig:arztbrief:2014:nonXmlBody", "1.3.6.1.4.1.19376.3.276.1.5.6", "Arztbrief 2014"},
    {"urn:ihe-d:ig:eppc-g:2015",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Enhanced Patient Privacy Consent - Germany"},
    {"urn:ihe-d:ig:eppc-g-sd:2015",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Enhanced Patient Privacy Consent - Germany - Scanned Document Option"},
    {"urn:ihe-d:spec:PDF_A1:2005", "1.3.6.1.4.1.19376.3.276.1.5.6", "PDF/A-1"},
    {"urn:ihe-d:spec:PDF_A2:2011", "1.3.6.1.4.1.19376.3.276.1.5.6", "PDF/A-2"},
    {"urn:ihe-d:spec:PDF_A3:2012", "1.3.6.1.4.1.19376.3.276.1.5.6", "PDF/A-3"},
    {"urn:ihe-d:spec:PDF_UA:2008", "1.3.6.1.4.1.19376.3.276.1.5.6", "PDF/UA"},
    {"urn:ihe-d:mime", "1.3.6.1.4.1.19376.3.276.1.5.6", "durch MIME Type beschrieben"},
    {"urn:gematik:ig:Arztbrief:r3.1", "1.3.6.1.4.1.19376.3.276.1.5.6", "Arztbrief § 291f SGB V"},
    {"urn:gematik:ig:Medikationsplan:r3.1",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Medikationsplan (gematik)"},
    {"urn:gematik:ig:Notfalldatensatz:r3.1", "1.3.6.1.4.1.19376.3.276.1.5.6", "Notfalldatensatz"},
    {"urn:gematik:ig:DatensatzPersoenlicheErklaerungen:r3.1",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Datensatz für persönliche Erklärungen (gematik)"},
    {"urn:gematik:ig:Impfausweis:r4.0", "1.3.6.1.4.1.19376.3.276.1.5.6", "Impfausweis (gematik)"},
    {"urn:gematik:ig:Impfausweis:v1.1.0", "1.3.6.1.4.1.19376.3.276.1.5.6", "Impfausweis (gematik)"},
    {"urn:gematik:ig:Mutterpass:r4.0", "1.3.6.1.4.1.19376.3.276.1.5.6", "Mutterpass (gematik)"},
    {"urn:gematik:ig:Mutterpass:v1.0.0", "1.3.6.1.4.1.19376.3.276.1.5.6", "Mutterpass (gematik)"},
    {"urn:gematik:ig:Mutterpass:v1.1.0", "1.3.6.1.4.1.19376.3.276.1.5.6", "Mutterpass (gematik)"},
    {"urn:gematik:ig:Kinderuntersuchungsheft:r4.0",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Kinderuntersuchungsheft (gematik)"},
    {"urn:gematik:ig:Kinderuntersuchungsheft:v1.0.0",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Kinderuntersuchungsheft (gematik)"},
    {"urn:gematik:ig:Arbeitsunfaehigkeitsbescheinigung:r4.0",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Arbeitsunfähigkeitsbescheinigung (gematik)"},
    {"urn:gematik:ig:Arbeitsunfaehigkeitsbescheinigung:v1.1",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Arbeitsunfähigkeitsbescheinigung (gematik) v1.1"},
    {"urn:gematik:ig:VerordnungsdatensatzMedikation:r4.0",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Verordnungsdatensatz Medikation (gematik)"},
    {"urn:gematik:ig:VerordnungsdatensatzMedikation:v1.0.2",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Verordnungsdatensatz Medikation (gematik)"},
    {"urn:gematik:ig:VerordnungsdatensatzMedikation:v1.1",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Verordnungsdatensatz Medikation (gematik) v1.1"},
    {"urn:gematik:ig:KinderuntersuchungsheftUntersuchungen:v1.0.0",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Untersuchungen Kinderuntersuchungsheft"},
    {"urn:gematik:ig:KinderuntersuchungsheftUntersuchungen:v1.0.1",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Untersuchungen Kinderuntersuchungsheft"},
    {"urn:gematik:ig:KinderuntersuchungsheftTeilnahmekarte:v1.0.0",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Teilnahmekarte Kinderuntersuchungsheft"},
    {"urn:gematik:ig:KinderuntersuchungsheftTeilnahmekarte:v1.0.1",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Teilnahmekarte Kinderuntersuchungsheft"},
    {"urn:gematik:ig:KinderuntersuchungsheftNotizen:v1.0.0",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Notizen Kinderuntersuchungsheft"},
    {"urn:gematik:ig:KinderuntersuchungsheftNotizen:v1.0.1",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Notizen Kinderuntersuchungsheft"},
    {"urn:hl7-de:DGUV-StatEntlassbrief:2020",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "DGUV Stationärer Entlassbrief"},
    {"urn:gematik:ig:Zahnbonusheft:r4.0",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Zahnbonusheft (gematik)"},
    {"urn:gematik:ig:Zahnbonusheft:v1.0.0",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Zahnbonusheft (gematik)"},
    {"urn:gematik:ig:Zahnbonusheft:v1.1.0",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Zahnbonusheft (gematik)"},
    {"urn:gematik:ig:diga:v1.0", "1.3.6.1.4.1.19376.3.276.1.5.6", "DiGA (gematik)"},
    {"urn:gematik:ig:diga:v1.1", "1.3.6.1.4.1.19376.3.276.1.5.6", "DiGA (gematik) v1.1"},
    {"urn:gematik:ig:DMP-Asthma:v4", "1.3.6.1.4.1.19376.3.276.1.5.6", "eDMP Asthma (gematik)"},
    {"urn:gematik:ig:DMP-BRK:v4", "1.3.6.1.4.1.19376.3.276.1.5.6", "eDMP Brustkrebs (gematik)"},
    {"urn:gematik:ig:DMP-COPD:v4",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "eDMP Chronic Obstrusive Pulmonary Disease (gematik)"},
    {"urn:gematik:ig:DMP-Rueckenschmerz:v1",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "eDMP Rückenschmerz (gematik)"},
    {"urn:gematik:ig:DMP-Depression:v1",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "eDMP Depression (gematik)"},
    {"urn:gematik:ig:DMP-DM1:v5",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "eDMP Diabetes mellitus Typ 1 (gematik)"},
    {"urn:gematik:ig:DMP-DM2:v6",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "eDMP Diabetes mellitus Typ 2 (gematik)"},
    {"urn:gematik:ig:DMP-HI:v1",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "eDMP Herzinsuffizienz (gematik)"},
    {"urn:gematik:ig:DMP-KHK:v4",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "eDMP Koronare Herzkrankheit (gematik)"},
    {"urn:gematik:ig:DMP-OST:v1", "1.3.6.1.4.1.19376.3.276.1.5.6", "eDMP Osteoporose (gematik)"},
    {"urn:gematik:ig:Telemedizinisches-Monitoring:v1.0",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Telemedizinisches Monitoring (gematik)"},
    {"urn:gematik:ig:Pflegeueberleitungsbogen:v1.0",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "Pflegeüberleitungsbogen (gematik)"},
    {"urn:gematik:ig:DMP-Rheuma:v1",
     "1.3.6.1.4.1.19376.3.276.1.5.6",
     "eDMP Rheumatoide Arthritis (gematik)"},
    {"1.2.840.10008.5.1.4.1.1.88.59",
     "1.2.840.10008.2.6.1",
     "DICOM Manifest (DICOM KOS SOP Class UID)"},
    {"urn:gematik:ig:researchDataSubmissionTracking:v1.0",
     "2.25.154081344090540725127779452347992051720",
     "Aufzeichnungsliste Forschungsdatenfreigabe"},
    // PRACTICE_SETTING_CODES
    {"ALLG", "1.3.6.1.4.1.19376.3.276.1.5.4", "Allgemeinmedizin"},
    {"ANAE", "1.3.6.1.4.1.19376.3.276.1.5.4", "Anästhesiologie"},
    {"ARBE", "1.3.6.1.4.1.19376.3.276.1.5.4", "Arbeitsmedizin"},
    {"AUGE", "1.3.6.1.4.1.19376.3.276.1.5.4", "Augenheilkunde"},
    {"CHIR", "1.3.6.1.4.1.19376.3.276.1.5.4", "Chirurgie"},
    {"ALCH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Allgemeinchirurgie"},
    {"GFCH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Gefäßchirurgie"},
    {"HZCH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Herzchirurgie"},
    {"KDCH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Kinderchirurgie"},
    {"ORTH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Orthopädie"},
    {"PLCH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Plastische und Ästhetische Chirurgie"},
    {"THCH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Thoraxchirurgie"},
    {"UNFC", "1.3.6.1.4.1.19376.3.276.1.5.4", "Unfallchirurgie"},
    {"VICH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Viszeralchirurgie"},
    {"FRAU", "1.3.6.1.4.1.19376.3.276.1.5.4", "Frauenheilkunde und Geburtshilfe"},
    {"GEND",
     "1.3.6.1.4.1.19376.3.276.1.5.4",
     "Gynäkologische Endokrinologie und Reproduktionsmedizin"},
    {"GONK", "1.3.6.1.4.1.19376.3.276.1.5.4", "Gynäkologische Onkologie"},
    {"PERI", "1.3.6.1.4.1.19376.3.276.1.5.4", "Perinatalmedizin"},
    {"GERI", "1.3.6.1.4.1.19376.3.276.1.5.4", "Geriatrie"},
    {"HNOH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Hals-Nasen-Ohrenheilkunde"},
    {"HRST", "1.3.6.1.4.1.19376.3.276.1.5.4", "Sprach-, Stimm- und kindliche Hörstörungen"},
    {"HAUT", "1.3.6.1.4.1.19376.3.276.1.5.4", "Haut- und Geschlechtskrankheiten"},
    {"HIST", "1.3.6.1.4.1.19376.3.276.1.5.4", "Histologie / Zytologie"},
    {"HUMA", "1.3.6.1.4.1.19376.3.276.1.5.4", "Humangenetik"},
    {"HYGI", "1.3.6.1.4.1.19376.3.276.1.5.4", "Hygiene und Umweltmedizin"},
    {"INNE", "1.3.6.1.4.1.19376.3.276.1.5.4", "Innere Medizin"},
    {"ANGI", "1.3.6.1.4.1.19376.3.276.1.5.4", "Angiologie"},
    {"ENDO", "1.3.6.1.4.1.19376.3.276.1.5.4", "Endokrinologie und Diabetologie"},
    {"GAST", "1.3.6.1.4.1.19376.3.276.1.5.4", "Gastroenterologie"},
    {"HAEM", "1.3.6.1.4.1.19376.3.276.1.5.4", "Hämatologie und internistische Onkologie"},
    {"KARD", "1.3.6.1.4.1.19376.3.276.1.5.4", "Kardiologie"},
    {"NEPH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Nephrologie"},
    {"PNEU", "1.3.6.1.4.1.19376.3.276.1.5.4", "Pneumologie"},
    {"RHEU", "1.3.6.1.4.1.19376.3.276.1.5.4", "Rheumatologie"},
    {"INTM", "1.3.6.1.4.1.19376.3.276.1.5.4", "Intensivmedizin"},
    {"INTZ", "1.3.6.1.4.1.19376.3.276.1.5.4", "Interdisziplinäre Zusammenarbeit"},
    {"INTO", "1.3.6.1.4.1.19376.3.276.1.5.4", "Interdisziplinäre Onkologie"},
    {"INTS", "1.3.6.1.4.1.19376.3.276.1.5.4", "Interdisziplinäre Schmerzmedizin"},
    {"TRPL", "1.3.6.1.4.1.19376.3.276.1.5.4", "Transplantationsmedizin"},
    {"SELT", "1.3.6.1.4.1.19376.3.276.1.5.4", "seltene Erkrankungen"},
    {"KIJU", "1.3.6.1.4.1.19376.3.276.1.5.4", "Kinder- und Jugendmedizin"},
    {"KONK", "1.3.6.1.4.1.19376.3.276.1.5.4", "Kinder-Hämatologie und -Onkologie"},
    {"KKAR", "1.3.6.1.4.1.19376.3.276.1.5.4", "Kinder-Kardiologie"},
    {"NNAT", "1.3.6.1.4.1.19376.3.276.1.5.4", "Neonatologie"},
    {"NPAE", "1.3.6.1.4.1.19376.3.276.1.5.4", "Neuropädiatrie"},
    {"KPSY", "1.3.6.1.4.1.19376.3.276.1.5.4", "Kinder- und Jugendpsychiatrie und -psychotherapie"},
    {"LABO", "1.3.6.1.4.1.19376.3.276.1.5.4", "Laboratoriumsmedizin"},
    {"MIKR",
     "1.3.6.1.4.1.19376.3.276.1.5.4",
     "Mikrobiologie, Virologie und Infektionsepidemiologie"},
    {"MKGC", "1.3.6.1.4.1.19376.3.276.1.5.4", "Mund-Kiefer-Gesichtschirurgie"},
    {"NATU", "1.3.6.1.4.1.19376.3.276.1.5.4", "Naturheilverfahren und alternative Heilmethoden"},
    {"NOTF", "1.3.6.1.4.1.19376.3.276.1.5.4", "Notfallmedizin"},
    {"NRCH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Neurochirurgie"},
    {"NEUR", "1.3.6.1.4.1.19376.3.276.1.5.4", "Neurologie"},
    {"NUKL", "1.3.6.1.4.1.19376.3.276.1.5.4", "Nuklearmedizin"},
    {"GESU", "1.3.6.1.4.1.19376.3.276.1.5.4", "Öffentliches Gesundheitswesen"},
    {"PALL", "1.3.6.1.4.1.19376.3.276.1.5.4", "Palliativmedizin"},
    {"PATH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Pathologie"},
    {"NPAT", "1.3.6.1.4.1.19376.3.276.1.5.4", "Neuropathologie"},
    {"PHAR", "1.3.6.1.4.1.19376.3.276.1.5.4", "Pharmakologie"},
    {"TOXI", "1.3.6.1.4.1.19376.3.276.1.5.4", "Toxikologie"},
    {"REHA", "1.3.6.1.4.1.19376.3.276.1.5.4", "Physikalische und Rehabilitative Medizin"},
    {"PSYC", "1.3.6.1.4.1.19376.3.276.1.5.4", "Psychiatrie und Psychotherapie"},
    {"FPSY", "1.3.6.1.4.1.19376.3.276.1.5.4", "Forensische Psychiatrie"},
    {"PSYM", "1.3.6.1.4.1.19376.3.276.1.5.4", "Psychosomatische Medizin und Psychotherapie"},
    {"RADI", "1.3.6.1.4.1.19376.3.276.1.5.4", "Radiologie"},
    {"KRAD", "1.3.6.1.4.1.19376.3.276.1.5.4", "Kinderradiologie"},
    {"NRAD", "1.3.6.1.4.1.19376.3.276.1.5.4", "Neuroradiologie"},
    {"RECH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Rechtsmedizin"},
    {"SCHL", "1.3.6.1.4.1.19376.3.276.1.5.4", "Schlafmedizin"},
    {"SPOR", "1.3.6.1.4.1.19376.3.276.1.5.4", "Sport- und Bewegungsmedizin"},
    {"STRA", "1.3.6.1.4.1.19376.3.276.1.5.4", "Strahlentherapie"},
    {"TRAN", "1.3.6.1.4.1.19376.3.276.1.5.4", "Transfusionsmedizin"},
    {"TROP", "1.3.6.1.4.1.19376.3.276.1.5.4", "Tropen-/Reisemedizin"},
    {"UROL", "1.3.6.1.4.1.19376.3.276.1.5.4", "Urologie"},
    {"MZKH", "1.3.6.1.4.1.19376.3.276.1.5.4", "Zahnmedizin"},
    {"ORAL", "1.3.6.1.4.1.19376.3.276.1.5.4", "Oralchirurgie"},
    {"KIEF", "1.3.6.1.4.1.19376.3.276.1.5.4", "Kieferorthopädie"},
    {"PARO", "1.3.6.1.4.1.19376.3.276.1.5.4", "Parodontologie"},
    {"MZAH", "1.2.276.0.76.5.494", "Allgemeine Zahnheilkunde"},
    {"ZGES", "1.2.276.0.76.5.494", "Öffentliches Gesundheitswesen (Zahnheilkunde)"},
    {"ERG", "1.3.6.1.4.1.19376.3.276.1.5.5", "Ergotherapie"},
    {"ERN", "1.3.6.1.4.1.19376.3.276.1.5.5", "Ernährung und Diätetik"},
    {"FOR", "1.3.6.1.4.1.19376.3.276.1.5.5", "Forschung"},
    {"PFL", "1.3.6.1.4.1.19376.3.276.1.5.5", "Pflege und Betreuung"},
    {"ALT", "1.3.6.1.4.1.19376.3.276.1.5.5", "Altenpflege"},
    {"KIN", "1.3.6.1.4.1.19376.3.276.1.5.5", "Kinderpflege"},
    {"PAT", "1.3.6.1.4.1.19376.3.276.1.5.5", "Patient außerhalb der Betreuung"},
    {"PHZ", "1.3.6.1.4.1.19376.3.276.1.5.5", "Pharmazeutik"},
    {"POD", "1.3.6.1.4.1.19376.3.276.1.5.5", "Podologie"},
    {"PRV", "1.3.6.1.4.1.19376.3.276.1.5.5", "Prävention"},
    {"SOZ", "1.3.6.1.4.1.19376.3.276.1.5.5", "Sozialwesen"},
    {"SPR", "1.3.6.1.4.1.19376.3.276.1.5.5", "Sprachtherapie"},
    {"VKO", "1.3.6.1.4.1.19376.3.276.1.5.5", "Versorgungskoordination"},
    {"VER", "1.3.6.1.4.1.19376.3.276.1.5.5", "Verwaltung"},
    {"PST", "1.3.6.1.4.1.19376.3.276.1.5.5", "Psychotherapie"},
    // AUTHOR_SPECIALTIES
    {"010", "1.2.276.0.76.5.114", "FA Allgemeinmedizin"},
    {"020", "1.2.276.0.76.5.114", "FA Anästhesiologie"},
    {"030", "1.2.276.0.76.5.114", "FA Augenheilkunde"},
    {"050", "1.2.276.0.76.5.114", "FA Frauenheilkunde und Geburtshilfe"},
    {"060", "1.2.276.0.76.5.114", "FA Hals-, Nasen-, Ohrenheilkunde"},
    {"070", "1.2.276.0.76.5.114", "FA Haut- und Geschlechtskrankheiten"},
    {"080", "1.2.276.0.76.5.114", "FA Innere Medizin"},
    {"091", "1.2.276.0.76.5.114", "SP Kinderkardiologie"},
    {"093", "1.2.276.0.76.5.114", "SP Neonatologie"},
    {"102", "1.2.276.0.76.5.114", "FA Kinder- und Jugendpsychiatrie und -psychotherapie"},
    {"110", "1.2.276.0.76.5.114", "FA Laboratoriumsmedizin"},
    {"130", "1.2.276.0.76.5.114", "FA Mund-Kiefer-Gesichts-Chirurgie"},
    {"142", "1.2.276.0.76.5.114", "FA Neurologie"},
    {"147", "1.2.276.0.76.5.114", "FA Psychiatrie und Psychotherapie"},
    {"150", "1.2.276.0.76.5.114", "FA Neurochirurgie"},
    {"170", "1.2.276.0.76.5.114", "FA Pathologie"},
    {"180", "1.2.276.0.76.5.114", "FA Pharmakologie und Toxikologie"},
    {"196", "1.2.276.0.76.5.114", "SP Kinderradiologie"},
    {"197", "1.2.276.0.76.5.114", "SP Neuroradiologie"},
    {"200", "1.2.276.0.76.5.114", "FA Urologie"},
    {"210", "1.2.276.0.76.5.114", "FA Arbeitsmedizin"},
    {"220", "1.2.276.0.76.5.114", "FA Nuklearmedizin"},
    {"230", "1.2.276.0.76.5.114", "FA Öffentliches Gesundheitswesen"},
    {"240", "1.2.276.0.76.5.114", "FA Rechtsmedizin"},
    {"250", "1.2.276.0.76.5.114", "FA Hygiene und Umweltmedizin"},
    {"271", "1.2.276.0.76.5.114", "FA Neuropathologie"},
    {"281", "1.2.276.0.76.5.114", "FA Klinische Pharmakologie"},
    {"291", "1.2.276.0.76.5.114", "FA Strahlentherapie"},
    {"301", "1.2.276.0.76.5.114", "FA Anatomie"},
    {"302", "1.2.276.0.76.5.114", "FA Biochemie"},
    {"303", "1.2.276.0.76.5.114", "FA Transfusionsmedizin"},
    {"304", "1.2.276.0.76.5.114", "FA Kinderchirurgie"},
    {"308", "1.2.276.0.76.5.114", "FA Physiologie"},
    {"313", "1.2.276.0.76.5.114", "FA Herzchirurgie"},
    {"314", "1.2.276.0.76.5.114", "FA Humangenetik"},
    {"330", "1.2.276.0.76.5.114", "FA Physikalische und Rehabilitative Medizin"},
    {"341", "1.2.276.0.76.5.114", "FA Kinder- und Jugendmedizin"},
    {"359", "1.2.276.0.76.5.114", "Fachzahnarzt für Mikrobiologie"},
    {"360", "1.2.276.0.76.5.114", "Fachzahnarzt für Kieferchirurgie (§ 6 Abs. 1 BMV)"},
    {"361", "1.2.276.0.76.5.114", "Fachzahnarzt für theoretisch-experimentelle Medizin"},
    {"511", "1.2.276.0.76.5.114", "FA Gefäßchirurgie"},
    {"512", "1.2.276.0.76.5.114", "FA Orthopädie und Unfallchirurgie"},
    {"513", "1.2.276.0.76.5.114", "FA Thoraxchirurgie"},
    {"514", "1.2.276.0.76.5.114", "FA Visceralchirurgie"},
    {"515", "1.2.276.0.76.5.114", "SP Gynäkologische Onkologie"},
    {"516", "1.2.276.0.76.5.114", "SP Gynäkologische Endokrinologie und Reproduktionsmedizin"},
    {"517", "1.2.276.0.76.5.114", "SP Spezielle Geburtshilfe und Perinatalmedizin"},
    {"518", "1.2.276.0.76.5.114", "FA Sprach-, Stimm- und kindliche Hörstörungen"},
    {"521", "1.2.276.0.76.5.114", "FA Innere Medizin und (SP) Angiologie"},
    {"522", "1.2.276.0.76.5.114", "FA Innere Medizin und (SP) Endokrinologie und Diabetologie"},
    {"523", "1.2.276.0.76.5.114", "FA Innere Medizin und (SP) Gastroenterologie"},
    {"524", "1.2.276.0.76.5.114", "FA Innere Medizin und (SP) Hämatologie und Onkologie"},
    {"525", "1.2.276.0.76.5.114", "FA Innere Medizin und (SP) Kardiologie"},
    {"526", "1.2.276.0.76.5.114", "FA Innere Medizin und (SP) Nephrologie"},
    {"527", "1.2.276.0.76.5.114", "FA Innere Medizin und (SP) Pneumologie"},
    {"528", "1.2.276.0.76.5.114", "FA Innere Medizin und (SP) Rheumatologie"},
    {"530", "1.2.276.0.76.5.114", "SP Kinder-Hämatologie und -Onkologie"},
    {"531", "1.2.276.0.76.5.114", "SP Neuropädiatrie"},
    {"532", "1.2.276.0.76.5.114", "FA Mikrobiologie, Virologie und Infektionsepidemiologie"},
    {"533", "1.2.276.0.76.5.114", "SP Forensische Psychiatrie"},
    {"534", "1.2.276.0.76.5.114", "FA Psychosomatische Medizin und Psychotherapie"},
    {"535", "1.2.276.0.76.5.114", "FA Radiologie (neue (M-)WBO)"},
    {"542", "1.2.276.0.76.5.114", "FA Plastische und Ästhetische Chirurgie"},
    {"544", "1.2.276.0.76.5.114", "FA Allgemeinchirurgie"},
    {"011001", "1.2.276.0.76.5.514", "FA Allgemeinmedizin"},
    {"012901", "1.2.276.0.76.5.514", "SP Geriatrie"},
    {"021001", "1.2.276.0.76.5.514", "FA Anästhesiologie"},
    {"021002", "1.2.276.0.76.5.514", "FA Anästhesiologie und Intensivtherapie"},
    {"031001", "1.2.276.0.76.5.514", "FA Anatomie"},
    {"041001", "1.2.276.0.76.5.514", "FA Arbeitshygiene"},
    {"041002", "1.2.276.0.76.5.514", "FA Arbeitsmedizin"},
    {"051001", "1.2.276.0.76.5.514", "FA Augenheilkunde"},
    {"061001", "1.2.276.0.76.5.514", "FA Biochemie"},
    {"071107", "1.2.276.0.76.5.514", "FA Allgemeinchirurgie"},
    {"071101", "1.2.276.0.76.5.514", "FA Allgemeine Chirurgie"},
    {"071001", "1.2.276.0.76.5.514", "FA Chirurgie"},
    {"071102", "1.2.276.0.76.5.514", "FA Gefäßchirurgie"},
    {"071002", "1.2.276.0.76.5.514", "FA Herzchirurgie"},
    {"071202", "1.2.276.0.76.5.514", "FA Kinder- und Jugendchirurgie"},
    {"071003", "1.2.276.0.76.5.514", "FA Kinderchirurgie"},
    {"071004", "1.2.276.0.76.5.514", "FA Orthopädie"},
    {"071103", "1.2.276.0.76.5.514", "FA Orthopädie und Unfallchirurgie"},
    {"071005", "1.2.276.0.76.5.514", "FA Plastische Chirurgie"},
    {"071106", "1.2.276.0.76.5.514", "FA Plastische und Ästhetische Chirurgie"},
    {"071201", "1.2.276.0.76.5.514", "FA Plastische; Rekonstruktive und Ästhetische Chirurgie"},
    {"071104", "1.2.276.0.76.5.514", "FA Thoraxchirurgie"},
    {"071105", "1.2.276.0.76.5.514", "FA Visceralchirurgie"},
    {"071108", "1.2.276.0.76.5.514", "FA Viszeralchirurgie"},
    {"072001", "1.2.276.0.76.5.514", "SP Gefäßchirurgie"},
    {"072002", "1.2.276.0.76.5.514", "SP Rheumatologie (Orthopädie)"},
    {"072003", "1.2.276.0.76.5.514", "SP Thoraxchirurgie in der Chirurgie"},
    {"072004", "1.2.276.0.76.5.514", "SP Thoraxchirurgie in der Herzchirurgie"},
    {"072005", "1.2.276.0.76.5.514", "SP Unfallchirurgie"},
    {"072006", "1.2.276.0.76.5.514", "SP Visceralchirurgie"},
    {"073001", "1.2.276.0.76.5.514", "TG Echokardiologie herznaher Gefäße"},
    {"073002", "1.2.276.0.76.5.514", "TG Gefäßchirurgie"},
    {"073003", "1.2.276.0.76.5.514", "TG Herz- und Gefäßchirurgie"},
    {"073004", "1.2.276.0.76.5.514", "TG Kinderchirurgie"},
    {"073005", "1.2.276.0.76.5.514", "TG Plastische Chirurgie"},
    {"073006", "1.2.276.0.76.5.514", "TG Rheumatologie (Orthopädie)"},
    {"073007", "1.2.276.0.76.5.514", "TG Thorax- und Kardiovaskularchirurgie"},
    {"073008", "1.2.276.0.76.5.514", "TG Thoraxchirurgie"},
    {"073009", "1.2.276.0.76.5.514", "TG Unfallchirurgie"},
    {"081001", "1.2.276.0.76.5.514", "FA Frauenheilkunde"},
    {"081002", "1.2.276.0.76.5.514", "FA Frauenheilkunde und Geburtshilfe"},
    {"081003", "1.2.276.0.76.5.514", "FA Gynäkologie und Geburtshilfe"},
    {"082101", "1.2.276.0.76.5.514", "SP Gynäkologische Endokrinologie und Reproduktionsmedizin"},
    {"082102", "1.2.276.0.76.5.514", "SP Gynäkologische Onkologie"},
    {"082103", "1.2.276.0.76.5.514", "SP Spezielle Geburtshilfe und Perinatalmedizin"},
    {"091001", "1.2.276.0.76.5.514", "FA Hals-Nasen-Ohrenheilkunde"},
    {"091002", "1.2.276.0.76.5.514", "FA Phoniatrie und Pädaudiologie"},
    {"091101", "1.2.276.0.76.5.514", "FA Sprach-; Stimm- und kindliche Hörstörungen"},
    {"093001", "1.2.276.0.76.5.514", "TG Audiologie"},
    {"093002", "1.2.276.0.76.5.514", "TG Phoniatrie"},
    {"093003", "1.2.276.0.76.5.514", "TG Phoniatrie und Pädaudiologie"},
    {"101001", "1.2.276.0.76.5.514", "FA Dermatologie und Venerologie"},
    {"101002", "1.2.276.0.76.5.514", "FA Haut- und Geschlechtskrankheiten"},
    {"111001", "1.2.276.0.76.5.514", "FA Humangenetik"},
    {"121001", "1.2.276.0.76.5.514", "FA Hygiene"},
    {"121002", "1.2.276.0.76.5.514", "FA Hygiene und Umweltmedizin"},
    {"131001", "1.2.276.0.76.5.514", "FA Immunologie"},
    {"141002", "1.2.276.0.76.5.514", "FA Innere Medizin"},
    {"141110", "1.2.276.0.76.5.514", "FA Innere Medizin und Angiologie"},
    {"141111", "1.2.276.0.76.5.514", "FA Innere Medizin und Endokrinologie und Diabetologie"},
    {"141112", "1.2.276.0.76.5.514", "FA Innere Medizin und Gastroenterologie"},
    {"141903", "1.2.276.0.76.5.514", "FA Innere Medizin und Geriatrie"},
    {"141113", "1.2.276.0.76.5.514", "FA Innere Medizin und Hämatologie und Onkologie"},
    {"141904", "1.2.276.0.76.5.514", "FA Innere Medizin und Infektiologie"},
    {"141114", "1.2.276.0.76.5.514", "FA Innere Medizin und Kardiologie"},
    {"141115", "1.2.276.0.76.5.514", "FA Innere Medizin und Nephrologie"},
    {"141116", "1.2.276.0.76.5.514", "FA Innere Medizin und Pneumologie"},
    {"141117", "1.2.276.0.76.5.514", "FA Innere Medizin und Rheumatologie"},
    {"141102", "1.2.276.0.76.5.514", "FA Innere Medizin und Schwerpunkt Angiologie"},
    {"141103",
     "1.2.276.0.76.5.514",
     "FA Innere Medizin und Schwerpunkt Endokrinologie und Diabetologie"},
    {"141104", "1.2.276.0.76.5.514", "FA Innere Medizin und Schwerpunkt Gastroenterologie"},
    {"141901", "1.2.276.0.76.5.514", "FA Innere Medizin und Schwerpunkt Geriatrie"},
    {"141902", "1.2.276.0.76.5.514", "FA Innere Medizin und Schwerpunkt gesamte Innere Medizin"},
    {"141105", "1.2.276.0.76.5.514", "FA Innere Medizin und Schwerpunkt Hämatologie und Onkologie"},
    {"141106", "1.2.276.0.76.5.514", "FA Innere Medizin und Schwerpunkt Kardiologie"},
    {"141107", "1.2.276.0.76.5.514", "FA Innere Medizin und Schwerpunkt Nephrologie"},
    {"141108", "1.2.276.0.76.5.514", "FA Innere Medizin und Schwerpunkt Pneumologie"},
    {"141109", "1.2.276.0.76.5.514", "FA Innere Medizin und Schwerpunkt Rheumatologie"},
    {"141003", "1.2.276.0.76.5.514", "FA Internist/Lungen- und Bronchialheilkunde"},
    {"141005", "1.2.276.0.76.5.514", "FA Lungen- und Bronchialheilkunde"},
    {"141004", "1.2.276.0.76.5.514", "FA Lungenheilkunde"},
    {"142001", "1.2.276.0.76.5.514", "SP Angiologie"},
    {"142002", "1.2.276.0.76.5.514", "SP Endokrinologie"},
    {"142901", "1.2.276.0.76.5.514", "SP Endokrinologie und Diabetologie"},
    {"142003", "1.2.276.0.76.5.514", "SP Gastroenterologie"},
    {"142004", "1.2.276.0.76.5.514", "SP Geriatrie"},
    {"142005", "1.2.276.0.76.5.514", "SP Hämatologie und Internistische Onkologie"},
    {"142006", "1.2.276.0.76.5.514", "SP Infektiologie"},
    {"142007", "1.2.276.0.76.5.514", "SP Kardiologie"},
    {"142008", "1.2.276.0.76.5.514", "SP Nephrologie"},
    {"142009", "1.2.276.0.76.5.514", "SP Pneumologie"},
    {"142010", "1.2.276.0.76.5.514", "SP Rheumatologie"},
    {"143001", "1.2.276.0.76.5.514", "TG Diabetologie"},
    {"143002", "1.2.276.0.76.5.514", "TG Endokrinologie"},
    {"143003", "1.2.276.0.76.5.514", "TG Gastroenterologie"},
    {"143004", "1.2.276.0.76.5.514", "TG Hämatologie"},
    {"143005", "1.2.276.0.76.5.514", "TG Infektions- und Tropenmedizin"},
    {"143006", "1.2.276.0.76.5.514", "TG Kardiologie"},
    {"143901", "1.2.276.0.76.5.514", "TG Kardiologie und Angiologie"},
    {"143007", "1.2.276.0.76.5.514", "TG Lungen- und Bronchialheilkunde"},
    {"143008", "1.2.276.0.76.5.514", "TG Nephrologie"},
    {"143009", "1.2.276.0.76.5.514", "TG Rheumatologie"},
    {"151002", "1.2.276.0.76.5.514", "FA Kinder- und Jugendmedizin"},
    {"151001", "1.2.276.0.76.5.514", "FA Kinderheilkunde"},
    {"152901",
     "1.2.276.0.76.5.514",
     "SP Endokrinologie und Diabetologie in der Kinder- und Jugendmedizin"},
    {"152902", "1.2.276.0.76.5.514", "SP Gastroenterologie in der Kinder- und Jugendmedizin"},
    {"152001", "1.2.276.0.76.5.514", "SP Infektiologie"},
    {"152201", "1.2.276.0.76.5.514", "SP Kinder- und Jugend-Hämatologie und -Onkologie"},
    {"152202", "1.2.276.0.76.5.514", "SP Kinder- und Jugend-Kardiologie"},
    {"152101", "1.2.276.0.76.5.514", "SP Kinder-Hämatologie und -Onkologie"},
    {"152002", "1.2.276.0.76.5.514", "SP Kinder-Kardiologie"},
    {"152906", "1.2.276.0.76.5.514", "SP Kinderpneumologie"},
    {"152003", "1.2.276.0.76.5.514", "SP Neonatologie"},
    {"152903", "1.2.276.0.76.5.514", "SP Nephrologie"},
    {"152102", "1.2.276.0.76.5.514", "SP Neuropädiatrie"},
    {"152904", "1.2.276.0.76.5.514", "SP Pädiatrische Rheumatologie"},
    {"152905", "1.2.276.0.76.5.514", "SP Pulmologie in der Kinder- und Jugendmedizin"},
    {"153001", "1.2.276.0.76.5.514", "TG Kinderdiabetologie"},
    {"153002", "1.2.276.0.76.5.514", "TG Kindergastroenterologie"},
    {"153003", "1.2.276.0.76.5.514", "TG Kinderhämatologie"},
    {"153004", "1.2.276.0.76.5.514", "TG Kinderkardiologie"},
    {"153005", "1.2.276.0.76.5.514", "TG Kinderlungen- und -bronchialheilkunde"},
    {"153006", "1.2.276.0.76.5.514", "TG Kinderneonatologie"},
    {"153007", "1.2.276.0.76.5.514", "TG Kindernephrologie"},
    {"153008", "1.2.276.0.76.5.514", "TG Kinderneuropsychiatrie"},
    {"161001", "1.2.276.0.76.5.514", "FA Kinder- und Jugendpsychiatrie"},
    {"161002", "1.2.276.0.76.5.514", "FA Kinder- und Jugendpsychiatrie und -psychotherapie"},
    {"171001", "1.2.276.0.76.5.514", "FA Laboratoriumsmedizin"},
    {"173001", "1.2.276.0.76.5.514", "TG Medizinische Mikrobiologie"},
    {"181001", "1.2.276.0.76.5.514", "FA Mikrobiologie"},
    {"181002", "1.2.276.0.76.5.514", "FA Mikrobiologie und Infektionsepidemiologie"},
    {"181101", "1.2.276.0.76.5.514", "FA Mikrobiologie; Virologie und Infektionsepidemiologie"},
    {"191001", "1.2.276.0.76.5.514", "FA Kieferchirurgie"},
    {"191002", "1.2.276.0.76.5.514", "FA Mund-Kiefer-Gesichtschirurgie"},
    {"191901", "1.2.276.0.76.5.514", "FA Oralchirurgie"},
    {"201001", "1.2.276.0.76.5.514", "FA Nervenheilkunde"},
    {"201002", "1.2.276.0.76.5.514", "FA Nervenheilkunde (Neurologie und Psychiatrie)"},
    {"201003", "1.2.276.0.76.5.514", "FA Neurologie und Psychiatrie (Nervenarzt)"},
    {"203001", "1.2.276.0.76.5.514", "TG Kinderneuropsychiatrie"},
    {"211001", "1.2.276.0.76.5.514", "FA Neurochirurgie"},
    {"221001", "1.2.276.0.76.5.514", "FA Neurologie"},
    {"222901", "1.2.276.0.76.5.514", "SP Geriatrie"},
    {"231001", "1.2.276.0.76.5.514", "FA Nuklearmedizin"},
    {"241001", "1.2.276.0.76.5.514", "FA Öffentliches Gesundheitswesen"},
    {"251001", "1.2.276.0.76.5.514", "FA Neuropathologie"},
    {"251002", "1.2.276.0.76.5.514", "FA Pathobiochemie und Labordiagnostik"},
    {"251003", "1.2.276.0.76.5.514", "FA Pathologie"},
    {"251004", "1.2.276.0.76.5.514", "FA Pathologische Anatomie"},
    {"251005", "1.2.276.0.76.5.514", "FA Pathologische Physiologie"},
    {"253001", "1.2.276.0.76.5.514", "TG Neuropathologie"},
    {"261001", "1.2.276.0.76.5.514", "FA Klinische Pharmakologie"},
    {"261002", "1.2.276.0.76.5.514", "FA Pharmakologie"},
    {"261003", "1.2.276.0.76.5.514", "FA Pharmakologie und Toxikologie"},
    {"263001", "1.2.276.0.76.5.514", "TG Klinische Pharmakologie"},
    {"381201", "1.2.276.0.76.5.514", "Phoniatrie und Pädaudiologie"},
    {"271001", "1.2.276.0.76.5.514", "FA Physikalische und Rehabilitative Medizin"},
    {"271002", "1.2.276.0.76.5.514", "FA Physiotherapie"},
    {"281001", "1.2.276.0.76.5.514", "FA Physiologie"},
    {"291001", "1.2.276.0.76.5.514", "FA Psychiatrie"},
    {"291002", "1.2.276.0.76.5.514", "FA Psychiatrie und Psychotherapie"},
    {"292101", "1.2.276.0.76.5.514", "SP Forensische Psychiatrie"},
    {"292901", "1.2.276.0.76.5.514", "SP Geriatrie"},
    {"301101", "1.2.276.0.76.5.514", "FA Psychosomatische Medizin und Psychotherapie"},
    {"301001", "1.2.276.0.76.5.514", "FA Psychotherapeutische Medizin"},
    {"301002", "1.2.276.0.76.5.514", "FA Psychotherapie"},
    {"311001", "1.2.276.0.76.5.514", "FA Diagnostische Radiologie"},
    {"311002", "1.2.276.0.76.5.514", "FA Radiologie"},
    {"311003", "1.2.276.0.76.5.514", "FA Radiologische Diagnostik"},
    {"312201", "1.2.276.0.76.5.514", "SP Kinder- und Jugendradiologie"},
    {"312001", "1.2.276.0.76.5.514", "SP Kinderradiologie"},
    {"312002", "1.2.276.0.76.5.514", "SP Neuroradiologie"},
    {"313001", "1.2.276.0.76.5.514", "TG Kinderradiologie"},
    {"313002", "1.2.276.0.76.5.514", "TG Neuroradiologie"},
    {"313003", "1.2.276.0.76.5.514", "TG Strahlentherapie"},
    {"321001", "1.2.276.0.76.5.514", "FA Rechtsmedizin"},
    {"351001", "1.2.276.0.76.5.514", "FA Strahlentherapie"},
    {"361001", "1.2.276.0.76.5.514", "FA Blutspende- und Transfusionswesen"},
    {"361002", "1.2.276.0.76.5.514", "FA Transfusionsmedizin"},
    {"371001", "1.2.276.0.76.5.514", "FA Urologie"},
    {"011002", "1.2.276.0.76.5.514", "FA Praktischer Arzt"},
    {"011101", "1.2.276.0.76.5.514", "FA Innere und Allgemeinmedizin (Hausarzt)"},
    {"142902", "1.2.276.0.76.5.514", "SP Geriatrie"},
    {"331001", "1.2.276.0.76.5.514", "FA Sozialhygiene"},
    {"341001", "1.2.276.0.76.5.514", "FA Sportmedizin"},
    {"590001", "1.2.276.0.76.5.514", "Biomathematik"},
    {"590002", "1.2.276.0.76.5.514", "Biophysik"},
    {"590003", "1.2.276.0.76.5.514", "Geschichte der Medizin"},
    {"590004", "1.2.276.0.76.5.514", "Industrietoxikologie"},
    {"590005", "1.2.276.0.76.5.514", "Kieferchirurgie"},
    {"590006", "1.2.276.0.76.5.514", "Klinische Strahlenphysik"},
    {"590007", "1.2.276.0.76.5.514", "Medizinische Genetik"},
    {"590008", "1.2.276.0.76.5.514", "Medizinische Informatik"},
    {"590009", "1.2.276.0.76.5.514", "Medizinische Physik und Biophysik"},
    {"590010", "1.2.276.0.76.5.514", "Medizinische Wissenschaftsinformation"},
    {"590011", "1.2.276.0.76.5.514", "Pathologische Biochemie"},
    {"000001", "1.2.276.0.76.5.514", "Ärztin/Arzt"},
    {"000011",
     "1.2.276.0.76.5.514",
     "Praktische Ärztin/Praktischer Arzt (EWG-Recht ab 86/457/EWG)"},
    {"1", "1.2.276.0.76.5.492", "Zahnärztin/Zahnarzt"},
    {"2", "1.2.276.0.76.5.492", "FZA Allgemeine Zahnheilkunde"},
    {"3", "1.2.276.0.76.5.492", "FZA Parodontologie"},
    {"4", "1.2.276.0.76.5.492", "FZA Oralchirurgie"},
    {"5", "1.2.276.0.76.5.492", "FZA Kieferorthopädie"},
    {"6", "1.2.276.0.76.5.492", "FZA öffentliches Gesundheitswesen"},
    {"1", "1.3.6.1.4.1.19376.3.276.1.5.11", "Gesundheits- Sozial-, Sportmanagement"},
    {"2", "1.3.6.1.4.1.19376.3.276.1.5.11", "Arzthilfe, Praxisorganisation, -verwaltung"},
    {"179", "1.3.6.1.4.1.19376.3.276.1.5.11", "Physician Assistant"},
    {"3", "1.3.6.1.4.1.19376.3.276.1.5.11", "Kaufmann/-frau - Gesundheitswesen"},
    {"4", "1.3.6.1.4.1.19376.3.276.1.5.11", "Medizinischer Fachangestellter"},
    {"5", "1.3.6.1.4.1.19376.3.276.1.5.11", "Tiermedizinischer Fachangestellter"},
    {"6", "1.3.6.1.4.1.19376.3.276.1.5.11", "Zahnmedizinischer Fachangestellter"},
    {"7", "1.3.6.1.4.1.19376.3.276.1.5.11", "Arztsekretär"},
    {"8", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sozial-, Gesundheitsmanagement"},
    {"9", "1.3.6.1.4.1.19376.3.276.1.5.11", "Gesundheitsaufseher / Hygienekontrolleur"},
    {"10", "1.3.6.1.4.1.19376.3.276.1.5.11", "Assistent Gesundheits- und Sozialwesen"},
    {"11", "1.3.6.1.4.1.19376.3.276.1.5.11", "Beamte Sozialversicherung"},
    {"12", "1.3.6.1.4.1.19376.3.276.1.5.11", "Beamte Sozialverwaltung"},
    {"13", "1.3.6.1.4.1.19376.3.276.1.5.11", "Betriebswirt Sozialversicherung"},
    {"14", "1.3.6.1.4.1.19376.3.276.1.5.11", "Gesundheitsmanager"},
    {"15", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sozialökonom, -wirt"},
    {"16", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sozialversicherungsfachangestellte"},
    {"17", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sportmanagement"},
    {"18", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sportassistent"},
    {"19", "1.3.6.1.4.1.19376.3.276.1.5.11", "Fachwirt Fitness"},
    {"20", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sport- und Fitnesskaufmann"},
    {"21", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sportmanager, Sportökonom"},
    {"22",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "nichtärztliche medizinische Analyse, Beratung, Pflege, Therapie"},
    {"23", "1.3.6.1.4.1.19376.3.276.1.5.11", "Gesundheitsberatung, -förderung"},
    {"24", "1.3.6.1.4.1.19376.3.276.1.5.11", "Assistenten für Gesundheitstourismus, -prophylaxe"},
    {"25", "1.3.6.1.4.1.19376.3.276.1.5.11", "Diätassistent"},
    {"26", "1.3.6.1.4.1.19376.3.276.1.5.11", "Gesundheitsförderer, -pädagoge"},
    {"27", "1.3.6.1.4.1.19376.3.276.1.5.11", "Gesundheitswissenschaftler"},
    {"28", "1.3.6.1.4.1.19376.3.276.1.5.11", "Oekotrophologe"},
    {"29", "1.3.6.1.4.1.19376.3.276.1.5.11", "Tai-Chi-Chuan- und Qigong-Lehrer"},
    {"30", "1.3.6.1.4.1.19376.3.276.1.5.11", "Yogalehrer"},
    {"31", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sportfachmann"},
    {"32", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sportwissenschaftler"},
    {"33", "1.3.6.1.4.1.19376.3.276.1.5.11", "Kranken-, Altenpflege, Geburtshilfe"},
    {"34", "1.3.6.1.4.1.19376.3.276.1.5.11", "Altenpflegehelfer"},
    {"35", "1.3.6.1.4.1.19376.3.276.1.5.11", "Altenpfleger"},
    {"36", "1.3.6.1.4.1.19376.3.276.1.5.11", "Fachkraft Pflegeassistenz"},
    {"37", "1.3.6.1.4.1.19376.3.276.1.5.11", "Gesundheits- und Kinderkrankenpfleger"},
    {"38", "1.3.6.1.4.1.19376.3.276.1.5.11", "Gesundheits- und Krankenpflegehelfer"},
    {"39", "1.3.6.1.4.1.19376.3.276.1.5.11", "Gesundheits- und Krankenpfleger"},
    {"40", "1.3.6.1.4.1.19376.3.276.1.5.11", "Haus- und Familienpfleger"},
    {"41", "1.3.6.1.4.1.19376.3.276.1.5.11", "Hebamme / Entbindungspfleger"},
    {"42", "1.3.6.1.4.1.19376.3.276.1.5.11", "Heilerziehungspfleger"},
    {"43", "1.3.6.1.4.1.19376.3.276.1.5.11", "Helfer Altenpflege"},
    {"44", "1.3.6.1.4.1.19376.3.276.1.5.11", "Helfer stationäre Krankenpflege"},
    {"45", "1.3.6.1.4.1.19376.3.276.1.5.11", "Heilerziehungspflegehelfer"},
    {"46", "1.3.6.1.4.1.19376.3.276.1.5.11", "Pflegewissenschaftler"},
    {"175",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Fachgesundheits- und krankenpfleger für Intensivpflege und Anästhesie"},
    {"176",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Fachgesundheits- und krankenpfleger im Operations-/Endoskopiedienst"},
    {"177", "1.3.6.1.4.1.19376.3.276.1.5.11", "Fachgesundheits- und krankenpfleger für Hygiene"},
    {"178",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Fachgesundheits- und krankenpfleger für Palliativ- und Hospizpflege"},
    {"47",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Nichtärztliche Behandlung, Therapie (außer Psychotherapie)"},
    {"48", "1.3.6.1.4.1.19376.3.276.1.5.11", "Akademischer Sprachtherapeut"},
    {"49", "1.3.6.1.4.1.19376.3.276.1.5.11", "Atem-, Sprech- und Stimmlehrer"},
    {"50", "1.3.6.1.4.1.19376.3.276.1.5.11", "Ergotherapeut"},
    {"51", "1.3.6.1.4.1.19376.3.276.1.5.11", "Fachangestellter für Bäderbetriebe"},
    {"52", "1.3.6.1.4.1.19376.3.276.1.5.11", "Heilpraktiker"},
    {"53", "1.3.6.1.4.1.19376.3.276.1.5.11", "Klinischer Linguist"},
    {"54", "1.3.6.1.4.1.19376.3.276.1.5.11", "Kunsttherapeut"},
    {"55", "1.3.6.1.4.1.19376.3.276.1.5.11", "Logopäde"},
    {"56", "1.3.6.1.4.1.19376.3.276.1.5.11", "Masseur und medizinische Bademeister"},
    {"57", "1.3.6.1.4.1.19376.3.276.1.5.11", "Motologe"},
    {"58", "1.3.6.1.4.1.19376.3.276.1.5.11", "Musiktherapeut"},
    {"59", "1.3.6.1.4.1.19376.3.276.1.5.11", "Orthoptist"},
    {"60", "1.3.6.1.4.1.19376.3.276.1.5.11", "Physiotherapeut"},
    {"61", "1.3.6.1.4.1.19376.3.276.1.5.11", "Podologe"},
    {"62", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sporttherapeut"},
    {"63", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sprechwissenschaftler"},
    {"64", "1.3.6.1.4.1.19376.3.276.1.5.11", "Staatlich anerkannter Sprachtherapeut"},
    {"65", "1.3.6.1.4.1.19376.3.276.1.5.11", "Stomatherapeut"},
    {"66", "1.3.6.1.4.1.19376.3.276.1.5.11", "Tanz- und Bewegungstherapeut"},
    {"67", "1.3.6.1.4.1.19376.3.276.1.5.11", "Tierheilpraktiker"},
    {"68", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sozialtherapeut"},
    {"69", "1.3.6.1.4.1.19376.3.276.1.5.11", "Pharmazeutische Beratung, Pharmavertrieb"},
    {"70", "1.3.6.1.4.1.19376.3.276.1.5.11", "Apotheker / Fachapotheker"},
    {"71", "1.3.6.1.4.1.19376.3.276.1.5.11", "Pharmazeut"},
    {"72", "1.3.6.1.4.1.19376.3.276.1.5.11", "Pharmazeutisch-technischer Assistent – PTA"},
    {"73", "1.3.6.1.4.1.19376.3.276.1.5.11", "Pharmazeutisch-kaufmännischer Angestellter"},
    {"180", "1.3.6.1.4.1.19376.3.276.1.5.11", "Pharmazieingenieur"},
    {"181", "1.3.6.1.4.1.19376.3.276.1.5.11", "Apothekenassistent"},
    {"182", "1.3.6.1.4.1.19376.3.276.1.5.11", "Apothekerassistent"},
    {"74", "1.3.6.1.4.1.19376.3.276.1.5.11", "Psychologische Analyse, Beratung, Therapie"},
    {"75", "1.3.6.1.4.1.19376.3.276.1.5.11", "Gesundheits- und Rehabilitationspsychologe"},
    {"76", "1.3.6.1.4.1.19376.3.276.1.5.11", "Kinder- und Jugendpsychotherapeut"},
    {"77", "1.3.6.1.4.1.19376.3.276.1.5.11", "Klinischer Psychologe"},
    {"78", "1.3.6.1.4.1.19376.3.276.1.5.11", "Kommunikationspsychologe"},
    {"79", "1.3.6.1.4.1.19376.3.276.1.5.11", "Pädagogischer Psychologe"},
    {"80", "1.3.6.1.4.1.19376.3.276.1.5.11", "Psychoanalytiker"},
    {"81", "1.3.6.1.4.1.19376.3.276.1.5.11", "Psychologe"},
    {"82", "1.3.6.1.4.1.19376.3.276.1.5.11", "Psychologischer Psychotherapeut"},
    {"183", "1.3.6.1.4.1.19376.3.276.1.5.11", "Psychotherapeut"},
    {"184", "1.3.6.1.4.1.19376.3.276.1.5.11", "Fachpsychotherapeut für Kinder und Jugendliche"},
    {"185", "1.3.6.1.4.1.19376.3.276.1.5.11", "Fachpsychotherapeut für Erwachsene"},
    {"83", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sportpsychologe"},
    {"84", "1.3.6.1.4.1.19376.3.276.1.5.11", "Verkehrspsychologe"},
    {"85", "1.3.6.1.4.1.19376.3.276.1.5.11", "Wirtschaftspsychologe"},
    {"86", "1.3.6.1.4.1.19376.3.276.1.5.11", "Rettungsdienst"},
    {"87", "1.3.6.1.4.1.19376.3.276.1.5.11", "Ingenieur Rettungswesen"},
    {"88", "1.3.6.1.4.1.19376.3.276.1.5.11", "Notfallsanitäter"},
    {"89", "1.3.6.1.4.1.19376.3.276.1.5.11", "Rettungsassistent"},
    {"90", "1.3.6.1.4.1.19376.3.276.1.5.11", "Rettungshelfer"},
    {"91", "1.3.6.1.4.1.19376.3.276.1.5.11", "Rettungssanitäter"},
    {"92", "1.3.6.1.4.1.19376.3.276.1.5.11", "med. Datenverarbeitung"},
    {"93", "1.3.6.1.4.1.19376.3.276.1.5.11", "Medizinische Datenerhebung"},
    {"94", "1.3.6.1.4.1.19376.3.276.1.5.11", "Medizinischer Dokumentar"},
    {"95", "1.3.6.1.4.1.19376.3.276.1.5.11", "Medizinischer Dokumentationsassistent"},
    {"173",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Fachangestellter f. Medien- und Informationsdienste - Medizinische Dokumentation"},
    {"174", "1.3.6.1.4.1.19376.3.276.1.5.11", "Medizinischer Informationsmanager"},
    {"96", "1.3.6.1.4.1.19376.3.276.1.5.11", "Soziales, Pädagogik"},
    {"97", "1.3.6.1.4.1.19376.3.276.1.5.11", "Kinderbetreuung, -erziehung"},
    {"98", "1.3.6.1.4.1.19376.3.276.1.5.11", "Pädagoge"},
    {"99", "1.3.6.1.4.1.19376.3.276.1.5.11", "Kinderdorfmutter, -vater"},
    {"100", "1.3.6.1.4.1.19376.3.276.1.5.11", "Kinderpfleger"},
    {"101", "1.3.6.1.4.1.19376.3.276.1.5.11", "Erzieher"},
    {"102", "1.3.6.1.4.1.19376.3.276.1.5.11", "Erzieher Jugend- und Heimerziehung"},
    {"103", "1.3.6.1.4.1.19376.3.276.1.5.11", "Lehrer"},
    {"104", "1.3.6.1.4.1.19376.3.276.1.5.11", "Orientierungs- und Mobilitätslehrer"},
    {"105", "1.3.6.1.4.1.19376.3.276.1.5.11", "Medien-, Kulturpädagogik"},
    {"106", "1.3.6.1.4.1.19376.3.276.1.5.11", "Musikpädagoge"},
    {"107", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sozialberatung, -arbeit"},
    {"108", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sozialarbeiter / Sozialpädagoge"},
    {"109", "1.3.6.1.4.1.19376.3.276.1.5.11", "Betreuungskraft / Alltagsbegleiter"},
    {"110", "1.3.6.1.4.1.19376.3.276.1.5.11", "Gerontologe"},
    {"111", "1.3.6.1.4.1.19376.3.276.1.5.11", "Psychosozialer Prozessbegleiter"},
    {"112", "1.3.6.1.4.1.19376.3.276.1.5.11", "Rehabilitationspädagoge"},
    {"113", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sozialassistent"},
    {"114", "1.3.6.1.4.1.19376.3.276.1.5.11", "Seelsorge"},
    {"115", "1.3.6.1.4.1.19376.3.276.1.5.11", "Religionspädagoge"},
    {"116", "1.3.6.1.4.1.19376.3.276.1.5.11", "Gemeindehelfer, Gemeindediakon"},
    {"117", "1.3.6.1.4.1.19376.3.276.1.5.11", "Theologe"},
    {"118", "1.3.6.1.4.1.19376.3.276.1.5.11", "Medizintechnik, Laboranalyse"},
    {"119", "1.3.6.1.4.1.19376.3.276.1.5.11", "Medizin-, Orthopädie- und Rehatechnik"},
    {"120", "1.3.6.1.4.1.19376.3.276.1.5.11", "Assistent Medizinische Gerätetechnik"},
    {"121", "1.3.6.1.4.1.19376.3.276.1.5.11", "Augenoptiker"},
    {"122", "1.3.6.1.4.1.19376.3.276.1.5.11", "Hörakustiker / Hörgeräteakustiker"},
    {"123", "1.3.6.1.4.1.19376.3.276.1.5.11", "Hörgeräteakustikermeister"},
    {"124", "1.3.6.1.4.1.19376.3.276.1.5.11", "Ingenieur Augenoptik"},
    {"125", "1.3.6.1.4.1.19376.3.276.1.5.11", "Ingenieur - Hörtechnik und Audiologie"},
    {"126", "1.3.6.1.4.1.19376.3.276.1.5.11", "Ingenieur - Medizintechnik"},
    {"127", "1.3.6.1.4.1.19376.3.276.1.5.11", "Ingenieur - Orthopädie- und Rehatechnik"},
    {"128", "1.3.6.1.4.1.19376.3.276.1.5.11", "Medizinphysiker (z.B. in Strahlenmedizin)"},
    {"129", "1.3.6.1.4.1.19376.3.276.1.5.11", "Orthopädieschuhmacher"},
    {"130", "1.3.6.1.4.1.19376.3.276.1.5.11", "Orthopädietechnik-Mechaniker"},
    {"131", "1.3.6.1.4.1.19376.3.276.1.5.11", "Zahntechniker"},
    {"132", "1.3.6.1.4.1.19376.3.276.1.5.11", "Glasbläser (Fachrichtung Kunstaugen)"},
    {"133",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "staatlich geprüfter Techniker der Fachrichtung Medizintechnik"},
    {"134", "1.3.6.1.4.1.19376.3.276.1.5.11", "Medizinisch-technische Assistenz"},
    {"135", "1.3.6.1.4.1.19376.3.276.1.5.11", "Anästhesietechnischer Assistent"},
    {"136", "1.3.6.1.4.1.19376.3.276.1.5.11", "HNO Audiologieassistent"},
    {"137",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Medizinisch-Technischer Assistent Funktionsdiagnostik – MTA-F"},
    {"138",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Medizinisch-Technischer Laboratoriumsassistent – MTA-L"},
    {"139",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Medizinisch-Technischer Radiologieassistent – MTA-R"},
    {"140", "1.3.6.1.4.1.19376.3.276.1.5.11", "Operationstechnischer Angestellter"},
    {"141", "1.3.6.1.4.1.19376.3.276.1.5.11", "Operationstechnischer Assistent"},
    {"142", "1.3.6.1.4.1.19376.3.276.1.5.11", "Veterinärmedizinischer-technischer Assistent"},
    {"143", "1.3.6.1.4.1.19376.3.276.1.5.11", "Zytologieassistent"},
    {"144",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Chemie, naturwissenschaftliche Laboranalyse (außer MTA)"},
    {"145", "1.3.6.1.4.1.19376.3.276.1.5.11", "Biochemiker (z.B. klinische Chemie)"},
    {"146", "1.3.6.1.4.1.19376.3.276.1.5.11", "Chemiker (z.B. klinische Chemie)"},
    {"147", "1.3.6.1.4.1.19376.3.276.1.5.11", "Humangenetiker"},
    {"148", "1.3.6.1.4.1.19376.3.276.1.5.11", "Mikrobiologe"},
    {"149", "1.3.6.1.4.1.19376.3.276.1.5.11", "Dienstleistungen am Menschen (außer medizinische)"},
    {"150", "1.3.6.1.4.1.19376.3.276.1.5.11", "Körperpflege"},
    {"151", "1.3.6.1.4.1.19376.3.276.1.5.11", "Fachkraft Beauty und Wellness"},
    {"152", "1.3.6.1.4.1.19376.3.276.1.5.11", "Friseur"},
    {"153", "1.3.6.1.4.1.19376.3.276.1.5.11", "Kosmetiker"},
    {"154", "1.3.6.1.4.1.19376.3.276.1.5.11", "Bestattungswesen"},
    {"155", "1.3.6.1.4.1.19376.3.276.1.5.11", "Bestattungsfachkraft"},
    {"156", "1.3.6.1.4.1.19376.3.276.1.5.11", "Berufe aus sonstigen Berufsfeldern"},
    {"157", "1.3.6.1.4.1.19376.3.276.1.5.11", "Umwelt"},
    {"158",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Medien, Kultur, Gestaltung, Kunst (außer Pädagogen)"},
    {"159", "1.3.6.1.4.1.19376.3.276.1.5.11", "Schutz und Sicherheit"},
    {"162",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Verfahrens- und Produktentwicklung, technisches Zeichnen, Konstruktion"},
    {"163", "1.3.6.1.4.1.19376.3.276.1.5.11", "Sprachen"},
    {"164",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Produktion, Produktionsplanung, Produktionssteuerung, Instandhaltung"},
    {"165", "1.3.6.1.4.1.19376.3.276.1.5.11", "Jurist"},
    {"166", "1.3.6.1.4.1.19376.3.276.1.5.11", "Reinigung"},
    {"167", "1.3.6.1.4.1.19376.3.276.1.5.11", "Bau, Architektur, Rohstoffe"},
    {"168",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Verwaltungsberufe (außer medizinische), kaufmännische Berufe, Verkehr"},
    {"169", "1.3.6.1.4.1.19376.3.276.1.5.11", "Taxifahrer bei Krankentransport"},
    {"170", "1.3.6.1.4.1.19376.3.276.1.5.11", "Elektro, Energie, Ver- und Entsorgung"},
    {"171",
     "1.3.6.1.4.1.19376.3.276.1.5.11",
     "Gastgewerbe und Tourismus, Veranstaltungsmanagement und Hauswirtschaft"},
    {"172", "1.3.6.1.4.1.19376.3.276.1.5.11", "IT"},
    {"1", "1.2.276.0.76.5.493", "Arzt in Facharztausbildung"},
    {"2", "1.2.276.0.76.5.493", "Hausarzt"},
    {"3", "1.2.276.0.76.5.493", "Praktischer Arzt"},
    // EVENT_CODES
    {"AD", "1.2.276.0.76.5.533", "Arztdokumentation"},
    {"AD0101", "1.2.276.0.76.5.533", "Arztberichte"},
    {"AD010101", "1.2.276.0.76.5.533", "Ärztliche Stellungnahme"},
    {"AD010102", "1.2.276.0.76.5.533", "Durchgangsarztbericht"},
    {"AD010103", "1.2.276.0.76.5.533", "Entlassungsbericht intern"},
    {"AD010104", "1.2.276.0.76.5.533", "Entlassungsbericht extern"},
    {"AD010105", "1.2.276.0.76.5.533", "Reha-Bericht"},
    {"AD010106", "1.2.276.0.76.5.533", "Verlegungsbericht intern"},
    {"AD010107", "1.2.276.0.76.5.533", "Verlegungsbericht extern"},
    {"AD010108", "1.2.276.0.76.5.533", "Vorläufiger Arztbericht"},
    {"AD010109", "1.2.276.0.76.5.533", "Ärztlicher Befundbericht"},
    {"AD010110", "1.2.276.0.76.5.533", "Ärztlicher Verlaufsbericht"},
    {"AD010111", "1.2.276.0.76.5.533", "Ambulanzbrief"},
    {"AD010112", "1.2.276.0.76.5.533", "Kurzarztbrief"},
    {"AD010113", "1.2.276.0.76.5.533", "Nachschaubericht"},
    {"AD010114", "1.2.276.0.76.5.533", "Interventionsbericht"},
    {"AD010199", "1.2.276.0.76.5.533", "Sonstiger Arztbericht"},
    {"AD0201", "1.2.276.0.76.5.533", "Bescheinigungen"},
    {"AD020101", "1.2.276.0.76.5.533", "Arbeitsunfähigkeitsbescheinigung"},
    {"AD020102", "1.2.276.0.76.5.533", "Beurlaubung"},
    {"AD020103", "1.2.276.0.76.5.533", "Todesbescheinigung"},
    {"AD020104", "1.2.276.0.76.5.533", "Ärztliche Bescheinigung"},
    {"AD020105", "1.2.276.0.76.5.533", "Notfall-/Vertretungsschein"},
    {"AD020106", "1.2.276.0.76.5.533", "Wiedereingliederungsplan"},
    {"AD020107", "1.2.276.0.76.5.533", "Aufenthaltsbescheinigung"},
    {"AD020108", "1.2.276.0.76.5.533", "Geburtsanzeige"},
    {"AD020199", "1.2.276.0.76.5.533", "Sonstige Bescheinigung"},
    {"AD0202", "1.2.276.0.76.5.533", "Befunderhebungen"},
    {"AD020201", "1.2.276.0.76.5.533", "Anatomische Skizze"},
    {"AD020202", "1.2.276.0.76.5.533", "Befundbogen"},
    {"AD020203", "1.2.276.0.76.5.533", "Bericht Gesundheitsuntersuchung"},
    {"AD020204", "1.2.276.0.76.5.533", "Krebsfrüherkennung"},
    {"AD020205", "1.2.276.0.76.5.533", "Messblatt"},
    {"AD020206", "1.2.276.0.76.5.533", "Belastungserprobung"},
    {"AD020207", "1.2.276.0.76.5.533", "Ärztlicher Fragebogen"},
    {"AD020208", "1.2.276.0.76.5.533", "Befund extern"},
    {"AD020299", "1.2.276.0.76.5.533", "Sonstige ärztliche Befunderhebung"},
    {"AD0601", "1.2.276.0.76.5.533", "Fallbesprechungen"},
    {"AD060101", "1.2.276.0.76.5.533", "Konsilanforderung"},
    {"AD060102", "1.2.276.0.76.5.533", "Konsilanmeldung"},
    {"AD060103", "1.2.276.0.76.5.533", "Konsilbericht intern"},
    {"AD060104", "1.2.276.0.76.5.533", "Konsilbericht extern"},
    {"AD060105", "1.2.276.0.76.5.533", "Visitenprotokoll"},
    {"AD060106", "1.2.276.0.76.5.533", "Tumorkonferenzprotokoll"},
    {"AD060107", "1.2.276.0.76.5.533", "Teambesprechungsprotokoll"},
    {"AD060108", "1.2.276.0.76.5.533", "Anordnung/Verordnung"},
    {"AD060109", "1.2.276.0.76.5.533", "Verordnung"},
    {"AD060199", "1.2.276.0.76.5.533", "Sonstige Fallbesprechung"},
    {"AM", "1.2.276.0.76.5.533", "Administration"},
    {"AM0101", "1.2.276.0.76.5.533", "Abrechnungsdokumente"},
    {"AM010101", "1.2.276.0.76.5.533", "Bogen abrechnungsrelevanter Diagnosen / Prozeduren"},
    {"AM010102", "1.2.276.0.76.5.533", "G-AEP Kriterien"},
    {"AM010103", "1.2.276.0.76.5.533", "Kostenübernahmeverlängerung"},
    {"AM010104", "1.2.276.0.76.5.533", "Schriftverkehr MD Kasse"},
    {"AM010105", "1.2.276.0.76.5.533", "Abrechnungsschein"},
    {"AM010106", "1.2.276.0.76.5.533", "Rechnung ambulante/stationäre Behandlung"},
    {"AM010107", "1.2.276.0.76.5.533", "MD Prüfauftrag"},
    {"AM010108", "1.2.276.0.76.5.533", "MD Gutachten"},
    {"AM010199", "1.2.276.0.76.5.533", "Sonstige Abrechnungsdokumentation"},
    {"AM0102", "1.2.276.0.76.5.533", "Anträge"},
    {"AM010201", "1.2.276.0.76.5.533", "Antrag auf Rehabilitation"},
    {"AM010202", "1.2.276.0.76.5.533", "Antrag auf Betreuung"},
    {"AM010203", "1.2.276.0.76.5.533", "Antrag auf gesetzliche Unterbringung"},
    {"AM010204", "1.2.276.0.76.5.533", "Verlängerungsantrag"},
    {"AM010205", "1.2.276.0.76.5.533", "Antrag auf Psychotherapie"},
    {"AM010206", "1.2.276.0.76.5.533", "Antrag auf Pflegeeinstufung"},
    {"AM010207", "1.2.276.0.76.5.533", "Kostenübernahmeantrag"},
    {"AM010208", "1.2.276.0.76.5.533", "Antrag auf Leistungen der Pflegeversicherung"},
    {"AM010209", "1.2.276.0.76.5.533", "Antrag auf Kurzzeitpflege"},
    {"AM010210", "1.2.276.0.76.5.533", "Antrag auf Fixierung/Isolierung beim Amtsgericht"},
    {"AM010299", "1.2.276.0.76.5.533", "Sonstiger Antrag"},
    {"AM0103", "1.2.276.0.76.5.533", "Aufklärungen"},
    {"AM010301", "1.2.276.0.76.5.533", "Anästhesieaufklärungsbogen"},
    {"AM010302", "1.2.276.0.76.5.533", "Diagnostischer Aufklärungsbogen"},
    {"AM010303", "1.2.276.0.76.5.533", "Operationsaufklärungsbogen"},
    {"AM010304", "1.2.276.0.76.5.533", "Aufklärungsbogen Therapie"},
    {"AM010399", "1.2.276.0.76.5.533", "Sonstiger Aufklärungsbogen"},
    {"AM0301", "1.2.276.0.76.5.533", "Checklisten Administration"},
    {"AM030101", "1.2.276.0.76.5.533", "Aktenlaufzettel"},
    {"AM030102", "1.2.276.0.76.5.533", "Checkliste Entlassung"},
    {"AM030103", "1.2.276.0.76.5.533", "Entlassungsplan"},
    {"AM030104", "1.2.276.0.76.5.533", "Patientenlaufzettel"},
    {"AM030199", "1.2.276.0.76.5.533", "Sonstige Checkliste Administration"},
    {"AM0501", "1.2.276.0.76.5.533", "Einwilligungen/Erklärungen"},
    {"AM050101", "1.2.276.0.76.5.533", "Datenschutzerklärung"},
    {"AM050102", "1.2.276.0.76.5.533", "Einverständniserklärung"},
    {"AM050103", "1.2.276.0.76.5.533", "Erklärung Nichtansprechbarkeit Patienten"},
    {"AM050104", "1.2.276.0.76.5.533", "Einverständniserklärung Abrechnung"},
    {"AM050105", "1.2.276.0.76.5.533", "Einverständniserklärung Behandlung"},
    {"AM050106",
     "1.2.276.0.76.5.533",
     "Einwilligung und Datenschutzerklärung Entlassungsmanagement"},
    {"AM050107", "1.2.276.0.76.5.533", "Schweigepflichtentbindung"},
    {"AM050108", "1.2.276.0.76.5.533", "Entlassung gegen ärztlichen Rat"},
    {"AM050109",
     "1.2.276.0.76.5.533",
     "Aufforderung zur Herausgabe der medizinischen Dokumentation"},
    {"AM050110", "1.2.276.0.76.5.533", "Aufforderung zur Löschung der medizinischen Dokumentation"},
    {"AM050111",
     "1.2.276.0.76.5.533",
     "Aufforderung zur Berichtigung der medizinischen Dokumentation"},
    {"AM050199", "1.2.276.0.76.5.533", "Sonstige Einwilligung/Erklärung"},
    {"AM1601", "1.2.276.0.76.5.533", "Patienteneigene Dokumente"},
    {"AM160101", "1.2.276.0.76.5.533", "Blutgruppenausweis"},
    {"AM160102", "1.2.276.0.76.5.533", "Impfausweis"},
    {"AM160103", "1.2.276.0.76.5.533", "Vorsorgevollmacht"},
    {"AM160104", "1.2.276.0.76.5.533", "Patientenverfügung"},
    {"AM160105", "1.2.276.0.76.5.533", "Wertgegenständeverwaltung"},
    {"AM160106", "1.2.276.0.76.5.533", "Allergiepass"},
    {"AM160107", "1.2.276.0.76.5.533", "Herzschrittmacherausweis"},
    {"AM160108", "1.2.276.0.76.5.533", "Nachlassprotokoll"},
    {"AM160109", "1.2.276.0.76.5.533", "Mutterpass (Kopie)"},
    {"AM160110", "1.2.276.0.76.5.533", "Ausweiskopie"},
    {"AM160111", "1.2.276.0.76.5.533", "Implantat-Ausweis"},
    {"AM160112", "1.2.276.0.76.5.533", "Betreuerausweis"},
    {"AM160113", "1.2.276.0.76.5.533", "Patientenbild"},
    {"AM160199", "1.2.276.0.76.5.533", "Sonstiges patienteneigenes Dokument"},
    {"AM1602", "1.2.276.0.76.5.533", "Patienteninformationen"},
    {"AM160201", "1.2.276.0.76.5.533", "Belehrung"},
    {"AM160202", "1.2.276.0.76.5.533", "Informationsblatt"},
    {"AM160203", "1.2.276.0.76.5.533", "Informationsblatt Entlassungsmanagement"},
    {"AM160299", "1.2.276.0.76.5.533", "Sonstiges Patienteninformationsblatt"},
    {"AM1603", "1.2.276.0.76.5.533", "Poststationäre Verordnungen"},
    {"AM160301", "1.2.276.0.76.5.533", "Heil- / Hilfsmittelverordnung"},
    {"AM160302", "1.2.276.0.76.5.533", "Krankentransportschein"},
    {"AM160303", "1.2.276.0.76.5.533", "Verordnung häusliche Krankenpflege"},
    {"AM160399", "1.2.276.0.76.5.533", "Sonstige poststationäre Verordnung"},
    {"AM1701", "1.2.276.0.76.5.533", "Qualitätssicherungen"},
    {"AM170101", "1.2.276.0.76.5.533", "Dokumentationsbogen Meldepflicht"},
    {"AM170102", "1.2.276.0.76.5.533", "Hygienestandard"},
    {"AM170103", "1.2.276.0.76.5.533", "Patientenfragebogen"},
    {"AM170104", "1.2.276.0.76.5.533", "Pflegestandard"},
    {"AM170105", "1.2.276.0.76.5.533", "Qualitätssicherungsbogen"},
    {"AM170199", "1.2.276.0.76.5.533", "Sonstiges Qualitätssicherungsdokument"},
    {"AM1901", "1.2.276.0.76.5.533", "Schriftverkehr"},
    {"AM190101", "1.2.276.0.76.5.533", "Anforderung Unterlagen"},
    {"AM190102", "1.2.276.0.76.5.533", "Schriftverkehr Amtsgericht"},
    {"AM190103", "1.2.276.0.76.5.533", "Schriftverkehr MD Arzt"},
    {"AM190104", "1.2.276.0.76.5.533", "Schriftverkehr Krankenkasse"},
    {"AM190105", "1.2.276.0.76.5.533", "Schriftverkehr Deutsche Rentenversicherung"},
    {"AM190106", "1.2.276.0.76.5.533", "Sendebericht"},
    {"AM190107", "1.2.276.0.76.5.533", "Empfangsbestätigung"},
    {"AM190108", "1.2.276.0.76.5.533", "Handschriftliche Notiz"},
    {"AM190109", "1.2.276.0.76.5.533", "Lieferschein"},
    {"AM190110", "1.2.276.0.76.5.533", "Schriftverkehr Amt/Behörde/Anwalt"},
    {"AM190111", "1.2.276.0.76.5.533", "Schriftverkehr Strafverfolgung und Schadensersatz"},
    {"AM190112", "1.2.276.0.76.5.533", "Anforderung Unterlagen MD"},
    {"AM190113", "1.2.276.0.76.5.533", "Widerspruchsbegründung"},
    {"AM190199", "1.2.276.0.76.5.533", "Sonstiger Schriftverkehr"},
    {"AM1902", "1.2.276.0.76.5.533", "Sozialdienst"},
    {"AM190201", "1.2.276.0.76.5.533", "Beratungsbogen Sozialer Dienst"},
    {"AM190202", "1.2.276.0.76.5.533", "Soziotherapeutischer Betreuungsplan"},
    {"AM190203", "1.2.276.0.76.5.533", "Einschätzung Sozialdienst"},
    {"AM190204", "1.2.276.0.76.5.533", "Abschlussbericht Sozialdienst"},
    {"AM190299", "1.2.276.0.76.5.533", "Sonstiges Dokument Sozialdienst"},
    {"AM2201", "1.2.276.0.76.5.533", "Verträge"},
    {"AM220101", "1.2.276.0.76.5.533", "Behandlungsvertrag"},
    {"AM220102", "1.2.276.0.76.5.533", "Wahlleistungsvertrag"},
    {"AM220103", "1.2.276.0.76.5.533", "Heimvertrag"},
    {"AM220104", "1.2.276.0.76.5.533", "Angaben zur Vergütung von Mitarbeitenden"},
    {"AM220199", "1.2.276.0.76.5.533", "Sonstiger Vertrag"},
    {"AU", "1.2.276.0.76.5.533", "Aufnahme"},
    {"AU0101", "1.2.276.0.76.5.533", "Aufnahmedokumente"},
    {"AU010101", "1.2.276.0.76.5.533", "Anamnesebogen"},
    {"AU010102", "1.2.276.0.76.5.533", "Anmeldung Aufnahme"},
    {"AU010103", "1.2.276.0.76.5.533", "Aufnahmebogen"},
    {"AU010104", "1.2.276.0.76.5.533", "Checkliste Aufnahme"},
    {"AU010105", "1.2.276.0.76.5.533", "Stammblatt"},
    {"AU010199", "1.2.276.0.76.5.533", "Sonstige Aufnahmedokumentation"},
    {"AU0501", "1.2.276.0.76.5.533", "Einweisungs-/ Überweisungsdokumente"},
    {"AU050101", "1.2.276.0.76.5.533", "Verordnung von Krankenhausbehandlung"},
    {"AU050102", "1.2.276.0.76.5.533", "Überweisungsschein"},
    {"AU050103", "1.2.276.0.76.5.533", "Überweisungsschein Entlassung"},
    {"AU050104", "1.2.276.0.76.5.533", "Verlegungsschein Intern"},
    {"AU050199", "1.2.276.0.76.5.533", "Sonstiges Einweisungs-/Überweisungsdokument"},
    {"AU1901", "1.2.276.0.76.5.533", "Rettungsstelle"},
    {"AU190101", "1.2.276.0.76.5.533", "Einsatzprotokoll"},
    {"AU190102", "1.2.276.0.76.5.533", "Notaufnahmebericht"},
    {"AU190103", "1.2.276.0.76.5.533", "Notaufnahmebogen"},
    {"AU190104", "1.2.276.0.76.5.533", "Notfalldatensatz"},
    {"AU190105", "1.2.276.0.76.5.533", "ISAR Screening"},
    {"AU190199", "1.2.276.0.76.5.533", "Sonstige Dokumentation Rettungsstelle"},
    {"DG", "1.2.276.0.76.5.533", "Diagnostik"},
    {"DG0201", "1.2.276.0.76.5.533", "Bildgebende Diagnostiken"},
    {"DG020101", "1.2.276.0.76.5.533", "Anforderung bildgebende Diagnostik"},
    {"DG020102", "1.2.276.0.76.5.533", "Angiographiebefund"},
    {"DG020103", "1.2.276.0.76.5.533", "CT-Befund"},
    {"DG020104", "1.2.276.0.76.5.533", "Echokardiographiebefund"},
    {"DG020105", "1.2.276.0.76.5.533", "Endoskopiebefund"},
    {"DG020106", "1.2.276.0.76.5.533", "Herzkatheterprotokoll"},
    {"DG020107", "1.2.276.0.76.5.533", "MRT-Befund"},
    {"DG020108", "1.2.276.0.76.5.533", "OCT-Befund"},
    {"DG020109", "1.2.276.0.76.5.533", "PET-Befund"},
    {"DG020110", "1.2.276.0.76.5.533", "Röntgenbefund"},
    {"DG020111", "1.2.276.0.76.5.533", "Sonographiebefund"},
    {"DG020112", "1.2.276.0.76.5.533", "SPECT-Befund"},
    {"DG020113", "1.2.276.0.76.5.533", "Szintigraphiebefund"},
    {"DG020114", "1.2.276.0.76.5.533", "Mammographiebefund"},
    {"DG020115", "1.2.276.0.76.5.533", "Checkliste bildgebende Diagnostik"},
    {"DG020199", "1.2.276.0.76.5.533", "Sonstige Dokumentation bildgebende Diagnostik"},
    {"DG0601", "1.2.276.0.76.5.533", "Funktionsdiagnostiken"},
    {"DG060101", "1.2.276.0.76.5.533", "Anforderung Funktionsdiagnostik"},
    {"DG060102", "1.2.276.0.76.5.533", "Audiometriebefund"},
    {"DG060103", "1.2.276.0.76.5.533", "Befund evozierter Potentiale"},
    {"DG060104", "1.2.276.0.76.5.533", "Blutdruckprotokoll"},
    {"DG060105", "1.2.276.0.76.5.533", "CTG-Ausdruck"},
    {"DG060106", "1.2.276.0.76.5.533", "Dokumentationsbogen Feststellung Hirntod"},
    {"DG060107", "1.2.276.0.76.5.533", "Dokumentationsbogen Herzschrittmacherkontrolle"},
    {"DG060108", "1.2.276.0.76.5.533", "Dokumentationsbogen Lungenfunktionsprüfung"},
    {"DG060109", "1.2.276.0.76.5.533", "EEG-Auswertung"},
    {"DG060110", "1.2.276.0.76.5.533", "EMG-Befund"},
    {"DG060111", "1.2.276.0.76.5.533", "EKG-Auswertung"},
    {"DG060112", "1.2.276.0.76.5.533", "Manometriebefund"},
    {"DG060113", "1.2.276.0.76.5.533", "Messungsprotokoll Augeninnendruck"},
    {"DG060114", "1.2.276.0.76.5.533", "Neurographiebefund"},
    {"DG060115", "1.2.276.0.76.5.533", "Rhinometriebefund"},
    {"DG060116", "1.2.276.0.76.5.533", "Schlaflabordokumentationsbogen"},
    {"DG060117", "1.2.276.0.76.5.533", "Schluckuntersuchung"},
    {"DG060118", "1.2.276.0.76.5.533", "Checkliste Funktionsdiagnostik"},
    {"DG060119", "1.2.276.0.76.5.533", "Ergometriebefund"},
    {"DG060120", "1.2.276.0.76.5.533", "Kipptischuntersuchung"},
    {"DG060121", "1.2.276.0.76.5.533", "Augenuntersuchung"},
    {"DG060122", "1.2.276.0.76.5.533", "ICD-Kontrolle"},
    {"DG060123", "1.2.276.0.76.5.533", "Zystometrie"},
    {"DG060124", "1.2.276.0.76.5.533", "Uroflowmetrie"},
    {"DG060199", "1.2.276.0.76.5.533", "Sonstige Dokumentation Funktionsdiagnostik"},
    {"DG0602", "1.2.276.0.76.5.533", "Funktionstests"},
    {"DG060201", "1.2.276.0.76.5.533", "Schellong Test"},
    {"DG060202", "1.2.276.0.76.5.533", "H2 Atemtest"},
    {"DG060203", "1.2.276.0.76.5.533", "Allergietest"},
    {"DG060204", "1.2.276.0.76.5.533", "Zahlenverbindungstest"},
    {"DG060205", "1.2.276.0.76.5.533", "6-Minuten-Gehtest"},
    {"DG060209", "1.2.276.0.76.5.533", "Sonstige Funktionstests"},
    {"DG060299", "1.2.276.0.76.5.533", "Sonstige Funktionstests"},
    {"ED", "1.2.276.0.76.5.533", "Elektronische Dokumentation"},
    {"ED0101", "1.2.276.0.76.5.533", "Audiodokumentation"},
    {"ED010199", "1.2.276.0.76.5.533", "Sonstige Audiodokumentation"},
    {"ED0201", "1.2.276.0.76.5.533", "Bilddokumentation"},
    {"ED020101", "1.2.276.0.76.5.533", "Fotodokumentation Operation"},
    {"ED020102", "1.2.276.0.76.5.533", "Fotodokumentation Dermatologie"},
    {"ED020103", "1.2.276.0.76.5.533", "Fotodokumentation Diagnostik"},
    {"ED020104", "1.2.276.0.76.5.533", "Videodokumentation Operation"},
    {"ED020199", "1.2.276.0.76.5.533", "Foto-/Videodokumentation Sonstige"},
    {"ED1101", "1.2.276.0.76.5.533", "KIS"},
    {"ED110101", "1.2.276.0.76.5.533", "Behandlungspfad"},
    {"ED110102", "1.2.276.0.76.5.533", "Notfalldatenmanagement (NFDM)"},
    {"ED110103", "1.2.276.0.76.5.533", "Medikationsplan elektronisch (eMP)"},
    {"ED110104", "1.2.276.0.76.5.533", "eArztbrief"},
    {"ED110105", "1.2.276.0.76.5.533", "eImpfpass"},
    {"ED110106", "1.2.276.0.76.5.533", "eZahnärztliches Bonusheft"},
    {"ED110107", "1.2.276.0.76.5.533", "eArbeitsunfähigkeitsbescheinigung"},
    {"ED110108", "1.2.276.0.76.5.533", "eRezept"},
    {"ED110109", "1.2.276.0.76.5.533", "Pflegebericht"},
    {"ED110110", "1.2.276.0.76.5.533", "eDMP"},
    {"ED110111", "1.2.276.0.76.5.533", "eMutterpass"},
    {"ED110112", "1.2.276.0.76.5.533", "KH-Entlassbrief"},
    {"ED110113", "1.2.276.0.76.5.533", "U-Heft Untersuchungen"},
    {"ED110114", "1.2.276.0.76.5.533", "U-Heft Teilnahmekarte"},
    {"ED110115", "1.2.276.0.76.5.533", "U-Heft Elternnotiz"},
    {"ED110116", "1.2.276.0.76.5.533", "Überleitungsbogen"},
    {"ED110199", "1.2.276.0.76.5.533", "Sonstige Dokumentation KIS"},
    {"ED1901", "1.2.276.0.76.5.533", "Schriftverkehr elektronisch"},
    {"ED190101", "1.2.276.0.76.5.533", "E-Mail Befundauskunft"},
    {"ED190102", "1.2.276.0.76.5.533", "E-Mail Juristische Beweissicherung"},
    {"ED190103", "1.2.276.0.76.5.533", "E-Mail Arztauskunft"},
    {"ED190104", "1.2.276.0.76.5.533", "E-Mail Sonstige"},
    {"ED190105", "1.2.276.0.76.5.533", "Fax Befundauskunft"},
    {"ED190106", "1.2.276.0.76.5.533", "Fax Juristische Beweissicherung"},
    {"ED190107", "1.2.276.0.76.5.533", "Fax Arztauskunft"},
    {"ED190108", "1.2.276.0.76.5.533", "Fax Sonstige"},
    {"ED190199", "1.2.276.0.76.5.533", "Sonstiger elektronischer Schriftverkehr"},
    {"LB", "1.2.276.0.76.5.533", "Labor"},
    {"LB0201", "1.2.276.0.76.5.533", "Blut"},
    {"LB020101", "1.2.276.0.76.5.533", "Blutgasanalyse"},
    {"LB020102", "1.2.276.0.76.5.533", "Blutkulturenbefund"},
    {"LB020103",
     "1.2.276.0.76.5.533",
     "Herstellungs- und Prüfprotokoll von Blut und Blutprodukten"},
    {"LB020104", "1.2.276.0.76.5.533", "Serologischer Befund"},
    {"LB020199", "1.2.276.0.76.5.533", "Sonstige Dokumentation Blut"},
    {"LB1201", "1.2.276.0.76.5.533", "Laborbefunde"},
    {"LB120101", "1.2.276.0.76.5.533", "Glukosetoleranztestprotokoll"},
    {"LB120102", "1.2.276.0.76.5.533", "Laborbefund extern"},
    {"LB120103", "1.2.276.0.76.5.533", "Laborbefund intern"},
    {"LB120104", "1.2.276.0.76.5.533", "Anforderung Labor"},
    {"LB120105", "1.2.276.0.76.5.533", "Überweisungsschein Labor"},
    {"LB120199", "1.2.276.0.76.5.533", "Sonstiger Laborbefund"},
    {"LB1301", "1.2.276.0.76.5.533", "Mikrobiologie"},
    {"LB130101", "1.2.276.0.76.5.533", "Mikrobiologiebefund"},
    {"LB130102", "1.2.276.0.76.5.533", "Urinbefund"},
    {"LB2201", "1.2.276.0.76.5.533", "Virologie"},
    {"LB220101", "1.2.276.0.76.5.533", "Befund über positive Infektionsmarker"},
    {"LB220102", "1.2.276.0.76.5.533", "Virologiebefund"},
    {"OP", "1.2.276.0.76.5.533", "Operation"},
    {"OP0101", "1.2.276.0.76.5.533", "Anästhesie"},
    {"OP010101", "1.2.276.0.76.5.533", "Anästhesieprotokoll intraoperativ"},
    {"OP010102", "1.2.276.0.76.5.533", "Aufwachraumprotokoll"},
    {"OP010103", "1.2.276.0.76.5.533", "Checkliste Anästhesie"},
    {"OP010199", "1.2.276.0.76.5.533", "Sonstige Anästhesiedokumentation"},
    {"OP1501", "1.2.276.0.76.5.533", "OP-Dokumente"},
    {"OP150101", "1.2.276.0.76.5.533", "Chargendokumentation"},
    {"OP150102", "1.2.276.0.76.5.533", "OP-Anmeldungsbogen"},
    {"OP150103", "1.2.276.0.76.5.533", "OP-Bericht"},
    {"OP150104", "1.2.276.0.76.5.533", "OP-Bilddokumentation"},
    {"OP150105", "1.2.276.0.76.5.533", "OP-Checkliste"},
    {"OP150106", "1.2.276.0.76.5.533", "OP-Protokoll"},
    {"OP150107", "1.2.276.0.76.5.533", "Postoperative Verordnung"},
    {"OP150108", "1.2.276.0.76.5.533", "OP-Zählprotokoll"},
    {"OP150109", "1.2.276.0.76.5.533", "Dokumentation ambulantes Operieren"},
    {"OP150199", "1.2.276.0.76.5.533", "Sonstige OP-Dokumentation"},
    {"OP2001", "1.2.276.0.76.5.533", "Transplantationsdokumente"},
    {"OP200101", "1.2.276.0.76.5.533", "Transplantationsprotokoll"},
    {"OP200102", "1.2.276.0.76.5.533", "Spenderdokument"},
    {"OP200199", "1.2.276.0.76.5.533", "Sonstige Transplantationsdokumentation"},
    {"PT", "1.2.276.0.76.5.533", "Pathologie"},
    {"PT0801", "1.2.276.0.76.5.533", "Histopathologie"},
    {"PT080101", "1.2.276.0.76.5.533", "Histologieanforderung"},
    {"PT080102", "1.2.276.0.76.5.533", "Histologiebefund"},
    {"PT1301", "1.2.276.0.76.5.533", "Molekularpathologie"},
    {"PT130101", "1.2.276.0.76.5.533", "Molekularpathologieanforderung"},
    {"PT130102", "1.2.276.0.76.5.533", "Molekularpathologiebefund"},
    {"PT2601", "1.2.276.0.76.5.533", "Zytopathologie"},
    {"PT260101", "1.2.276.0.76.5.533", "Zytologieanforderung"},
    {"PT260102", "1.2.276.0.76.5.533", "Zytologiebefund"},
    {"SD", "1.2.276.0.76.5.533", "Spezielle Dokumentation"},
    {"SD0701", "1.2.276.0.76.5.533", "Geburtendokumente"},
    {"SD070101", "1.2.276.0.76.5.533", "Geburtenbericht"},
    {"SD070102", "1.2.276.0.76.5.533", "Geburtenprotokoll"},
    {"SD070103", "1.2.276.0.76.5.533", "Geburtenverlaufskurve"},
    {"SD070104", "1.2.276.0.76.5.533", "Neugeborenenscreening"},
    {"SD070105", "1.2.276.0.76.5.533", "Partogramm"},
    {"SD070106", "1.2.276.0.76.5.533", "Wiegekarte"},
    {"SD070107", "1.2.276.0.76.5.533", "Neugeborenendokumentationsbogen"},
    {"SD070108", "1.2.276.0.76.5.533", "Säuglingskurve"},
    {"SD070109", "1.2.276.0.76.5.533", "Geburtenbogen"},
    {"SD070110", "1.2.276.0.76.5.533", "Perzentilkurve"},
    {"SD070111", "1.2.276.0.76.5.533", "Entnahme Nabelschnurblut"},
    {"SD070112", "1.2.276.0.76.5.533", "Datenblatt für den Pädiater"},
    {"SD070199", "1.2.276.0.76.5.533", "Sonstige Geburtendokumentation"},
    {"SD0702", "1.2.276.0.76.5.533", "Geriatrische Dokumente"},
    {"SD070201", "1.2.276.0.76.5.533", "Barthel Index"},
    {"SD070202", "1.2.276.0.76.5.533", "Dem Tect"},
    {"SD070203", "1.2.276.0.76.5.533", "ISAR Screening"},
    {"SD070204", "1.2.276.0.76.5.533", "Sturzrisikoerfassungsbogen"},
    {"SD070205", "1.2.276.0.76.5.533", "Geriatrische Depressionsskala"},
    {"SD070206", "1.2.276.0.76.5.533", "Geriatrische Assessmentdokumentation"},
    {"SD070207", "1.2.276.0.76.5.533", "Mobilitätstest nach Tinetti"},
    {"SD070208", "1.2.276.0.76.5.533", "Timed Up and Go Test"},
    {"SD070299", "1.2.276.0.76.5.533", "Sonstige geriatrische Dokumente"},
    {"SD1101", "1.2.276.0.76.5.533", "Komplexbehandlungen"},
    {"SD110101", "1.2.276.0.76.5.533", "Geriatrische Komplexbehandlungsdokumentation"},
    {"SD110102", "1.2.276.0.76.5.533", "Intensivmedizinische Komplexbehandlungsdokumentation"},
    {"SD110103", "1.2.276.0.76.5.533", "MRE/Nicht-MRE Komplexbehandlung"},
    {"SD110104", "1.2.276.0.76.5.533", "Neurologische Komplexbehandlungsdokumentation"},
    {"SD110105", "1.2.276.0.76.5.533", "Palliativmedizinische Komplexbehandlungsdokumentation"},
    {"SD110106", "1.2.276.0.76.5.533", "PKMS-Dokumentation"},
    {"SD110107", "1.2.276.0.76.5.533", "Dokumentation COVID"},
    {"SD110199", "1.2.276.0.76.5.533", "Sonstige Komplexbehandlungsdokumentation"},
    {"SD1301", "1.2.276.0.76.5.533", "Maßregelvollzug"},
    {"SD130101", "1.2.276.0.76.5.533", "Vertrag Maßregelvollzug"},
    {"SD130102", "1.2.276.0.76.5.533", "Antrag Maßregelvollzug"},
    {"SD130103", "1.2.276.0.76.5.533", "Schriftverkehr Maßregelvollzug"},
    {"SD130104", "1.2.276.0.76.5.533", "Einwilligung/Einverständniserklärung Maßregelvollzug"},
    {"SD130199", "1.2.276.0.76.5.533", "Sonstiges Maßregelvollzugdokument"},
    {"SD1501", "1.2.276.0.76.5.533", "Onkologische Dokumente"},
    {"SD150101", "1.2.276.0.76.5.533", "Follow up-Bogen"},
    {"SD150102", "1.2.276.0.76.5.533", "Meldebogen Krebsregister"},
    {"SD150103", "1.2.276.0.76.5.533", "Tumorkonferenzprotokoll"},
    {"SD150104", "1.2.276.0.76.5.533", "Tumorlokalisationsbogen"},
    {"SD150199", "1.2.276.0.76.5.533", "Sonstiger onkologischer Dokumentationsbogen"},
    {"SD1601", "1.2.276.0.76.5.533", "Psychische Dokumente"},
    {"SD160101", "1.2.276.0.76.5.533", "Handschriftliches Patiententagebuch"},
    {"SD160102", "1.2.276.0.76.5.533", "(Neuro-) Psychologischer Erhebungsbogen"},
    {"SD160103", "1.2.276.0.76.5.533", "Psychologische Therapieanordnung"},
    {"SD160104", "1.2.276.0.76.5.533", "Psychologisches Therapiegesprächsprotokoll"},
    {"SD160105", "1.2.276.0.76.5.533", "Psychologischer Verlaufsbogen"},
    {"SD160106", "1.2.276.0.76.5.533", "Spezialtherapeutische Verlaufsdokumentation"},
    {"SD160107", "1.2.276.0.76.5.533", "Therapieeinheiten/Ärzte/Psychologen/Spezialtherapeuten"},
    {"SD160108",
     "1.2.276.0.76.5.533",
     "1:1 Betreuung/Einzelbetreuung/Psychiatrische Intensivbehandlung"},
    {"SD160109", "1.2.276.0.76.5.533", "Checkliste für die Unterbringung psychisch Kranker"},
    {"SD160110", "1.2.276.0.76.5.533", "Dokumentation Verhaltensanalyse"},
    {"SD160111", "1.2.276.0.76.5.533", "Dokumentation Depression"},
    {"SD160112", "1.2.276.0.76.5.533", "Dokumentation Stationsäquivalente Behandlung (StäB)"},
    {"SD160199", "1.2.276.0.76.5.533", "Sonstige psychologische Dokumente"},
    {"SF", "1.2.276.0.76.5.533", "Studien/Forschung"},
    {"SF0601", "1.2.276.0.76.5.533", "Forschungsdokumente"},
    {"SF060101", "1.2.276.0.76.5.533", "Forschungsbericht"},
    {"SF060199", "1.2.276.0.76.5.533", "Sonstige Forschungsdokumentation"},
    {"SF1901", "1.2.276.0.76.5.533", "Studiendokumente"},
    {"SF190101", "1.2.276.0.76.5.533", "CRF-Bogen"},
    {"SF190102", "1.2.276.0.76.5.533", "Einwilligung Studie"},
    {"SF190103", "1.2.276.0.76.5.533", "Protokoll Ein- und Ausschlusskriterien"},
    {"SF190104", "1.2.276.0.76.5.533", "Prüfplan"},
    {"SF190105", "1.2.276.0.76.5.533", "SOP-Bogen"},
    {"SF190106", "1.2.276.0.76.5.533", "Studienbericht"},
    {"SF190199", "1.2.276.0.76.5.533", "Sonstige Studiendokumentation"},
    {"TH", "1.2.276.0.76.5.533", "Therapie"},
    {"TH0201", "1.2.276.0.76.5.533", "Bestrahlungstherapien"},
    {"TH020101", "1.2.276.0.76.5.533", "Bestrahlungsplan"},
    {"TH020102", "1.2.276.0.76.5.533", "Bestrahlungsprotokoll"},
    {"TH020103", "1.2.276.0.76.5.533", "Bestrahlungsverordnung"},
    {"TH020104", "1.2.276.0.76.5.533", "Radiojodtherapieprotokoll"},
    {"TH020105", "1.2.276.0.76.5.533", "Therapieprotokoll mit Radionukliden"},
    {"TH020199", "1.2.276.0.76.5.533", "Sonstiges Bestrahlungstherapieprotokoll"},
    {"TH0601", "1.2.276.0.76.5.533", "Funktionstherapien"},
    {"TH060101", "1.2.276.0.76.5.533", "Ergotherapieprotokoll"},
    {"TH060102", "1.2.276.0.76.5.533", "Logopädieprotokoll"},
    {"TH060103", "1.2.276.0.76.5.533", "Physiotherapieprotokoll"},
    {"TH060104", "1.2.276.0.76.5.533", "Anforderung Funktionstherapie"},
    {"TH060105", "1.2.276.0.76.5.533", "Elektrokonvulsionstherapie"},
    {"TH060106", "1.2.276.0.76.5.533", "Transkranielle Magnetstimulation"},
    {"TH060199", "1.2.276.0.76.5.533", "Sonstiges Funktionstherapieprotokoll"},
    {"TH1301", "1.2.276.0.76.5.533", "Medikamentöse Therapien"},
    {"TH130101", "1.2.276.0.76.5.533", "Anforderung Medikation"},
    {"TH130102", "1.2.276.0.76.5.533", "Apothekenbuch"},
    {"TH130103", "1.2.276.0.76.5.533", "Chemotherapieprotokoll"},
    {"TH130104", "1.2.276.0.76.5.533", "Hormontherapieprotokoll"},
    {"TH130105", "1.2.276.0.76.5.533", "Medikamentenplan extern"},
    {"TH130106", "1.2.276.0.76.5.533", "Medikamentenplan intern/extern (mit BTM)"},
    {"TH130107", "1.2.276.0.76.5.533", "Medikamentenplan intern/extern"},
    {"TH130108", "1.2.276.0.76.5.533", "Rezept"},
    {"TH130109", "1.2.276.0.76.5.533", "Schmerztherapieprotokoll"},
    {"TH130110", "1.2.276.0.76.5.533", "Prämedikationsprotokoll"},
    {"TH130111", "1.2.276.0.76.5.533", "Lyse Dokument"},
    {"TH130199", "1.2.276.0.76.5.533", "Sonstiges Dokument medikamentöser Therapie"},
    {"TH1601", "1.2.276.0.76.5.533", "Patientenschulungen"},
    {"TH160101", "1.2.276.0.76.5.533", "Protokoll Ernährungsberatung"},
    {"TH160199", "1.2.276.0.76.5.533", "Sonstiges Protokoll Patientenschulung"},
    {"TH2001", "1.2.276.0.76.5.533", "Transfusionsdokumente"},
    {"TH200101", "1.2.276.0.76.5.533", "Anforderung Blutkonserven"},
    {"TH200102", "1.2.276.0.76.5.533", "Blutspendeprotokoll"},
    {"TH200103", "1.2.276.0.76.5.533", "Bluttransfusionsprotokoll"},
    {"TH200104", "1.2.276.0.76.5.533", "Konservenbegleitschein"},
    {"TH200199", "1.2.276.0.76.5.533", "Sonstiges Transfusionsdokument"},
    {"VL", "1.2.276.0.76.5.533", "Verlauf"},
    {"UB9999", "1.2.276.0.76.5.533", "Sonstige Dokumentation"},
    {"UB999996", "1.2.276.0.76.5.533", "Nachweise (Zusatz-) Entgelte"},
    {"VL0101", "1.2.276.0.76.5.533", "Assessmentbögen"},
    {"VL010101", "1.2.276.0.76.5.533", "Dekubitusrisikoeinschätzung"},
    {"VL010102", "1.2.276.0.76.5.533", "Mini Mental Status Test inkl. Uhrentest"},
    {"VL010103", "1.2.276.0.76.5.533", "Schmerzerhebungsbogen"},
    {"VL010104", "1.2.276.0.76.5.533", "Ernährungsscreening"},
    {"VL010105", "1.2.276.0.76.5.533", "Aachener Aphasie Test"},
    {"VL010106", "1.2.276.0.76.5.533", "Glasgow Coma Scale"},
    {"VL010107", "1.2.276.0.76.5.533", "NIH Stroke Scale"},
    {"VL010108", "1.2.276.0.76.5.533", "IPSS (Internationaler Prostata Symptom Score)"},
    {"VL010199", "1.2.276.0.76.5.533", "Sonstiger Assessmentbogen"},
    {"VL0401", "1.2.276.0.76.5.533", "Diabetesdokumente"},
    {"VL040101", "1.2.276.0.76.5.533", "Diabetiker Kurve"},
    {"VL040102", "1.2.276.0.76.5.533", "Insulinplan"},
    {"VL040199", "1.2.276.0.76.5.533", "Sonstige Diabetesdokumentation"},
    {"VL0402", "1.2.276.0.76.5.533", "Dialysedokumente"},
    {"VL040201", "1.2.276.0.76.5.533", "Dialyseanforderung"},
    {"VL040202", "1.2.276.0.76.5.533", "Dialyseprotokoll"},
    {"VL040299", "1.2.276.0.76.5.533", "Sonstige Dialysedokumentation"},
    {"VL0403", "1.2.276.0.76.5.533", "Durchführungsnachweise"},
    {"VL040301", "1.2.276.0.76.5.533", "Ein- und Ausfuhrprotokoll"},
    {"VL040302", "1.2.276.0.76.5.533", "Fixierungsprotokoll"},
    {"VL040303", "1.2.276.0.76.5.533", "Isolierungsprotokoll"},
    {"VL040304", "1.2.276.0.76.5.533", "Lagerungsplan"},
    {"VL040305", "1.2.276.0.76.5.533", "Punktionsprotokoll"},
    {"VL040306", "1.2.276.0.76.5.533", "Punktionsprotokoll therapeutisch"},
    {"VL040307", "1.2.276.0.76.5.533", "Reanimationsprotokoll"},
    {"VL040308", "1.2.276.0.76.5.533", "Sondenplan"},
    {"VL040309", "1.2.276.0.76.5.533", "Behandlungsplan"},
    {"VL040310", "1.2.276.0.76.5.533", "Infektionsdokumentationsbogen"},
    {"VL040311", "1.2.276.0.76.5.533", "Nosokomialdokumentation"},
    {"VL040312", "1.2.276.0.76.5.533", "Stomadokumentation"},
    {"VL040313", "1.2.276.0.76.5.533", "Katheterdokument"},
    {"VL040314", "1.2.276.0.76.5.533", "Kardioversion"},
    {"VL040399", "1.2.276.0.76.5.533", "Sonstiger Durchführungsnachweis"},
    {"VL0901", "1.2.276.0.76.5.533", "Intensivmedizinische Dokumente"},
    {"VL090101", "1.2.276.0.76.5.533", "Beatmungsprotokoll"},
    {"VL090102", "1.2.276.0.76.5.533", "Intensivkurve"},
    {"VL090103", "1.2.276.0.76.5.533", "Intensivpflegebericht"},
    {"VL090104", "1.2.276.0.76.5.533", "Monitoringausdruck"},
    {"VL090105", "1.2.276.0.76.5.533", "Intensivdokumentationsbogen"},
    {"VL090199", "1.2.276.0.76.5.533", "Sonstiger Intensivdokumentationsbogen"},
    {"VL1601", "1.2.276.0.76.5.533", "Pflegedokumente"},
    {"VL160101", "1.2.276.0.76.5.533", "Auszug aus den medizinischen Daten"},
    {"VL160102", "1.2.276.0.76.5.533", "Ernährungsplan"},
    {"VL160103", "1.2.276.0.76.5.533", "Meldebogen Krebsregister"},
    {"VL160104", "1.2.276.0.76.5.533", "Pflegeanamnesebogen"},
    {"VL160105", "1.2.276.0.76.5.533", "Pflegebericht"},
    {"VL160106", "1.2.276.0.76.5.533", "Pflegekurve"},
    {"VL160107", "1.2.276.0.76.5.533", "Pflegeplanung"},
    {"VL160108", "1.2.276.0.76.5.533", "Pflegeüberleitungsbogen"},
    {"VL160109", "1.2.276.0.76.5.533", "Sturzprotokoll"},
    {"VL160110", "1.2.276.0.76.5.533", "Überwachungsprotokoll"},
    {"VL160111", "1.2.276.0.76.5.533", "Verlaufsdokumentationsbogen"},
    {"VL160112", "1.2.276.0.76.5.533", "Pflegevisite"},
    {"VL160113", "1.2.276.0.76.5.533", "Fallbesprechung Bezugspflegekraft"},
    {"VL160114", "1.2.276.0.76.5.533", "Pflegenachweis"},
    {"VL160199", "1.2.276.0.76.5.533", "Sonstiger Pflegedokumentationsbogen"},
    {"VL2301", "1.2.276.0.76.5.533", "Wunddokumente"},
    {"VL230101", "1.2.276.0.76.5.533", "Wunddokumentationsbogen"},
    {"VL230102", "1.2.276.0.76.5.533", "Bewegungs- und Lagerungsplan"},
    {"VL230103", "1.2.276.0.76.5.533", "Fotodokumentation Wunden"},
    {"VL230199", "1.2.276.0.76.5.533", "Sonstige Wunddokumentation"},
    {"UB", "1.2.276.0.76.5.533", "Sonstiges"},
    {"UB999997", "1.2.276.0.76.5.533", "Gesamtdokumentation stationäre Versorgung"},
    {"UB999998", "1.2.276.0.76.5.533", "Gesamtdokumentation ambulante Versorgung"},
    {"UB999999", "1.2.276.0.76.5.533", "Sonstige medizinische Dokumentation"},
    {"113681", "1.2.840.10008.6.1.2", "Phantom"},
    {"133945003", "1.2.840.10008.6.1.2", "Left hypochondriac region"},
    {"9454009", "1.2.840.10008.6.1.2", "Subclavian vein"},
    {"113269004", "1.2.840.10008.6.1.2", "External iliac artery"},
    {"72001000", "1.2.840.10008.6.1.2", "Bone of lower limb"},
    {"57850000", "1.2.840.10008.6.1.2", "Truncus coeliacus"},
    {"59011009", "1.2.840.10008.6.1.2", "Basilar artery"},
    {"2095001", "1.2.840.10008.6.1.2", "Paranasal sinus"},
    {"128565007", "1.2.840.10008.6.1.2", "Apex of right ventricle"},
    {"74670003", "1.2.840.10008.6.1.2", "Wrist joint"},
    {"79361005", "1.2.840.10008.6.1.2", "Fontanel of skull"},
    {"123851003", "1.2.840.10008.6.1.2", "Mouth"},
    {"128559007", "1.2.840.10008.6.1.2", "Genicular artery"},
    {"70238003", "1.2.840.10008.6.1.2", "Left ventricle inflow"},
    {"60184004", "1.2.840.10008.6.1.2", "Sigmoid colon"},
    {"312535008", "1.2.840.10008.6.1.2", "Pharynx and larynx"},
    {"69105007", "1.2.840.10008.6.1.2", "Carotid Artery"},
    {"60819002", "1.2.840.10008.6.1.2", "Cheek"},
    {"50536004", "1.2.840.10008.6.1.2", "Umbilical artery"},
    {"14944004", "1.2.840.10008.6.1.2", "Primitive aorta"},
    {"13418002", "1.2.840.10008.6.1.2", "Left ventricle outflow tract"},
    {"45631007", "1.2.840.10008.6.1.2", "Radial artery"},
    {"1101003", "1.2.840.10008.6.1.2", "Intracranial"},
    {"69833005", "1.2.840.10008.6.1.2", "Right femoral artery"},
    {"661005", "1.2.840.10008.6.1.2", "Jaw region"},
    {"113270003", "1.2.840.10008.6.1.2", "Left femoral artery"},
    {"91747007", "1.2.840.10008.6.1.2", "Lumen of blood vessel"},
    {"80144004", "1.2.840.10008.6.1.2", "Calcaneus"},
    {"32849002", "1.2.840.10008.6.1.2", "Endo-esophageal"},
    {"128553008", "1.2.840.10008.6.1.2", "Antecubital vein"},
    {"91609006", "1.2.840.10008.6.1.2", "Mandible"},
    {"91691001", "1.2.840.10008.6.1.2", "Parasternal"},
    {"34411009", "1.2.840.10008.6.1.2", "Broad ligament"},
    {"83330001", "1.2.840.10008.6.1.2", "Patent ductus arteriosus"},
    {"128583004", "1.2.840.10008.6.1.2", "Mesenteric vein"},
    {"43799004", "1.2.840.10008.6.1.2", "Intra-thoracic"},
    {"816092008", "1.2.840.10008.6.1.2", "Pelvis"},
    {"122972007", "1.2.840.10008.6.1.2", "Pulmonary vein"},
    {"41801008", "1.2.840.10008.6.1.2", "Coronary artery"},
    {"53120007", "1.2.840.10008.6.1.2", "Upper limb"},
    {"63507001", "1.2.840.10008.6.1.2", "External iliac vein"},
    {"111289009", "1.2.840.10008.6.1.2", "Pulmonary arteriovenous fistula"},
    {"32672002", "1.2.840.10008.6.1.2", "Descending aorta"},
    {"36765005", "1.2.840.10008.6.1.2", "Subclavian artery"},
    {"58602004", "1.2.840.10008.6.1.2", "Flank"},
    {"70925003", "1.2.840.10008.6.1.2", "Maxilla"},
    {"39607008", "1.2.840.10008.6.1.2", "Lung"},
    {"53549008", "1.2.840.10008.6.1.2", "Ophthalmic artery"},
    {"45206002", "1.2.840.10008.6.1.2", "Nose"},
    {"69930009", "1.2.840.10008.6.1.2", "Pancreatic duct"},
    {"85562004", "1.2.840.10008.6.1.2", "Hand"},
    {"20115005", "1.2.840.10008.6.1.2", "Brachial vein"},
    {"50519007", "1.2.840.10008.6.1.2", "Right upper quadrant of abdomen"},
    {"81745001", "1.2.840.10008.6.1.2", "Eye"},
    {"41216001", "1.2.840.10008.6.1.2", "Prostate"},
    {"16953009", "1.2.840.10008.6.1.2", "Elbow joint"},
    {"76015000", "1.2.840.10008.6.1.2", "Hepatic artery"},
    {"77568009", "1.2.840.10008.6.1.2", "Back"},
    {"91397008", "1.2.840.10008.6.1.2", "Facial bones"},
    {"56873002", "1.2.840.10008.6.1.2", "Sternum"},
    {"8821006", "1.2.840.10008.6.1.2", "Peroneal artery"},
    {"417437006", "1.2.840.10008.6.1.2", "Neck and Chest"},
    {"44627009", "1.2.840.10008.6.1.2", "Right ventricle outflow tract"},
    {"38848004", "1.2.840.10008.6.1.2", "Duodenum"},
    {"133946002", "1.2.840.10008.6.1.2", "Right hypochondriac region"},
    {"43863001", "1.2.840.10008.6.1.2", "Superior left pulmonary vein"},
    {"28273000", "1.2.840.10008.6.1.2", "Bile duct"},
    {"128589000", "1.2.840.10008.6.1.2", "Systemic collateral artery to lung"},
    {"76365002", "1.2.840.10008.6.1.2", "Upper outer quadrant of breast"},
    {"818982008", "1.2.840.10008.6.1.2", "Abdomen and Pelvis"},
    {"34635009", "1.2.840.10008.6.1.2", "Lumbar artery"},
    {"110621006", "1.2.840.10008.6.1.2", "Pancreatic duct and bile duct systems"},
    {"128564006", "1.2.840.10008.6.1.2", "Apex of left ventricle"},
    {"28726007", "1.2.840.10008.6.1.2", "Cornea"},
    {"4596009", "1.2.840.10008.6.1.2", "Larynx"},
    {"81040000", "1.2.840.10008.6.1.2", "Pulmonary artery"},
    {"7657000", "1.2.840.10008.6.1.2", "Femoral artery"},
    {"15497006", "1.2.840.10008.6.1.2", "Ovary"},
    {"244411005", "1.2.840.10008.6.1.2", "Iliac vein"},
    {"89546000", "1.2.840.10008.6.1.2", "Skull"},
    {"297172009", "1.2.840.10008.6.1.2", "Thoraco-lumbar spine"},
    {"13648007", "1.2.840.10008.6.1.2", "Endo-urethral"},
    {"69748006", "1.2.840.10008.6.1.2", "Thyroid"},
    {"69536005", "1.2.840.10008.6.1.2", "Head"},
    {"23451007", "1.2.840.10008.6.1.2", "Adrenal gland"},
    {"85856004", "1.2.840.10008.6.1.2", "Acromioclavicular joint"},
    {"90290004", "1.2.840.10008.6.1.2", "Umbilical region"},
    {"2334006", "1.2.840.10008.6.1.2", "Calyx"},
    {"128558004", "1.2.840.10008.6.1.2", "Congenital coronary artery fistula to right ventricle"},
    {"128569001", "1.2.840.10008.6.1.2", "Posterior medial tributary"},
    {"7844006", "1.2.840.10008.6.1.2", "Sternoclavicular joint"},
    {"18962004", "1.2.840.10008.6.1.2", "Endo-nasopharyngeal"},
    {"110861005", "1.2.840.10008.6.1.2", "Esophagus, stomach and duodenum"},
    {"76752008", "1.2.840.10008.6.1.2", "Breast"},
    {"45048000", "1.2.840.10008.6.1.2", "Neck"},
    {"7569003", "1.2.840.10008.6.1.2", "Finger"},
    {"170887008", "1.2.840.10008.6.1.2", "Submental"},
    {"128320002", "1.2.840.10008.6.1.2", "Cranial venous system"},
    {"32361000", "1.2.840.10008.6.1.2", "Popliteal fossa"},
    {"128584005", "1.2.840.10008.6.1.2", "Pulmonary artery conduit"},
    {"73829009", "1.2.840.10008.6.1.2", "Right atrium"},
    {"53085002", "1.2.840.10008.6.1.2", "Right ventricle"},
    {"45503006", "1.2.840.10008.6.1.2", "Common ventricle"},
    {"64688005", "1.2.840.10008.6.1.2", "Coccyx"},
    {"774007", "1.2.840.10008.6.1.2", "Head and Neck"},
    {"81502006", "1.2.840.10008.6.1.2", "Hypopharynx"},
    {"21814001", "1.2.840.10008.6.1.2", "Ventricle"},
    {"35039007", "1.2.840.10008.6.1.2", "Uterus"},
    {"53620006", "1.2.840.10008.6.1.2", "Temporomandibular joint"},
    {"110726009", "1.2.840.10008.6.1.2", "Trachea and bronchus"},
    {"8993003", "1.2.840.10008.6.1.2", "Hepatic vein"},
    {"40689003", "1.2.840.10008.6.1.2", "Testis"},
    {"416631005", "1.2.840.10008.6.1.2", "Pelvis and lower extremities"},
    {"69327007", "1.2.840.10008.6.1.2", "Internal mammary artery"},
    {"89837001", "1.2.840.10008.6.1.2", "Bladder"},
    {"45292006", "1.2.840.10008.6.1.2", "Vulva"},
    {"14742008", "1.2.840.10008.6.1.2", "Large intestine"},
    {"14975008", "1.2.840.10008.6.1.2", "Forearm"},
    {"89545001", "1.2.840.10008.6.1.2", "Face"},
    {"421060004", "1.2.840.10008.6.1.2", "Spine"},
    {"56459004", "1.2.840.10008.6.1.2", "Foot"},
    {"416152001", "1.2.840.10008.6.1.2", "Neck, Chest and Abdomen"},
    {"22286001", "1.2.840.10008.6.1.2", "External carotid artery"},
    {"42258001", "1.2.840.10008.6.1.2", "Superior mesenteric artery"},
    {"30608006", "1.2.840.10008.6.1.2", "Muscle of upper limb"},
    {"13363002", "1.2.840.10008.6.1.2", "Posterior tibial artery"},
    {"83419000", "1.2.840.10008.6.1.2", "Femoral vein"},
    {"68300000", "1.2.840.10008.6.1.2", "Right auricular appendage"},
    {"818987002", "1.2.840.10008.6.1.2", "Intra-abdominopelvic"},
    {"13881006", "1.2.840.10008.6.1.2", "Zygoma"},
    {"8887007", "1.2.840.10008.6.1.2", "Innominate vein"},
    {"818981001", "1.2.840.10008.6.1.2", "Abdomen"},
    {"181349008", "1.2.840.10008.6.1.2", "Superficial Femoral Artery"},
    {"110639002", "1.2.840.10008.6.1.2", "Uterus and fallopian tubes"},
    {"61685007", "1.2.840.10008.6.1.2", "Lower limb"},
    {"43119007", "1.2.840.10008.6.1.2", "Posterior communication artery"},
    {"128563000", "1.2.840.10008.6.1.2", "Juxtaposed atrial appendage"},
    {"57034009", "1.2.840.10008.6.1.2", "Aortic arch"},
    {"110517009", "1.2.840.10008.6.1.2", "Vertebral column and cranium"},
    {"21306003", "1.2.840.10008.6.1.2", "Jejunum"},
    {"416775004", "1.2.840.10008.6.1.2", "Chest, Abdomen and Pelvis"},
    {"7832008", "1.2.840.10008.6.1.2", "Abdominal aorta"},
    {"80891009", "1.2.840.10008.6.1.2", "Endo-cardiac"},
    {"27949001", "1.2.840.10008.6.1.2", "Tarsal joint"},
    {"41695006", "1.2.840.10008.6.1.2", "Scalp"},
    {"20699002", "1.2.840.10008.6.1.2", "Cephalic vein"},
    {"21974007", "1.2.840.10008.6.1.2", "Tongue"},
    {"113264009", "1.2.840.10008.6.1.2", "Lingual artery"},
    {"110568007", "1.2.840.10008.6.1.2", "Gastric vein"},
    {"128557009", "1.2.840.10008.6.1.2", "Congenital coronary artery fistula to right atrium"},
    {"87342007", "1.2.840.10008.6.1.2", "Fibula"},
    {"363654007", "1.2.840.10008.6.1.2", "Orbital structure"},
    {"18911002", "1.2.840.10008.6.1.2", "Penis"},
    {"195416006", "1.2.840.10008.6.1.2", "Inferior cardiac vein"},
    {"128981007", "1.2.840.10008.6.1.2", "Baffle"},
    {"59749000", "1.2.840.10008.6.1.2", "Lacrimal artery"},
    {"31145008", "1.2.840.10008.6.1.2", "Occipital artery"},
    {"59820001", "1.2.840.10008.6.1.2", "Endo-vascular"},
    {"18619003", "1.2.840.10008.6.1.2", "Sclera"},
    {"12738006", "1.2.840.10008.6.1.2", "Brain"},
    {"128568009", "1.2.840.10008.6.1.2", "Systemic venous atrium"},
    {"111002", "1.2.840.10008.6.1.2", "Parathyroid"},
    {"303270005", "1.2.840.10008.6.1.2", "Liver and biliary structure"},
    {"128585006", "1.2.840.10008.6.1.2", "Anomalous pulmonary vein"},
    {"26493002", "1.2.840.10008.6.1.2", "Suprasternal notch"},
    {"297171002", "1.2.840.10008.6.1.2", "Cervico-thoracic spine"},
    {"38864007", "1.2.840.10008.6.1.2", "Perineum"},
    {"34402009", "1.2.840.10008.6.1.2", "Endo-rectal"},
    {"71341001", "1.2.840.10008.6.1.2", "Femur"},
    {"194996006", "1.2.840.10008.6.1.2", "Anterior cardiac vein"},
    {"15776009", "1.2.840.10008.6.1.2", "Pancreas"},
    {"82471001", "1.2.840.10008.6.1.2", "Left atrium"},
    {"88556005", "1.2.840.10008.6.1.2", "Cerebral artery"},
    {"79601000", "1.2.840.10008.6.1.2", "Scapula"},
    {"33795007", "1.2.840.10008.6.1.2", "Inferior mesenteric artery"},
    {"64033007", "1.2.840.10008.6.1.2", "Endo-renal"},
    {"60734001", "1.2.840.10008.6.1.2", "Great saphenous vein"},
    {"113346000", "1.2.840.10008.6.1.2", "Omental bursa"},
    {"110837003", "1.2.840.10008.6.1.2", "Bladder and urethra"},
    {"122494005", "1.2.840.10008.6.1.2", "Cervical spine"},
    {"299716001", "1.2.840.10008.6.1.2", "Iliac and/or femoral artery"},
    {"86117002", "1.2.840.10008.6.1.2", "Internal carotid artery"},
    {"87953007", "1.2.840.10008.6.1.2", "Endo-ureteric"},
    {"54735007", "1.2.840.10008.6.1.2", "Sacrum"},
    {"30315005", "1.2.840.10008.6.1.2", "Small intestine"},
    {"361078006", "1.2.840.10008.6.1.2", "Internal Auditory Canal"},
    {"8629005", "1.2.840.10008.6.1.2", "Superior right pulmonary vein"},
    {"131183008", "1.2.840.10008.6.1.2", "Intra-articular"},
    {"25990002", "1.2.840.10008.6.1.2", "Renal pelvis"},
    {"64131007", "1.2.840.10008.6.1.2", "Inferior vena cava"},
    {"78480002", "1.2.840.10008.6.1.2", "Right pulmonary artery"},
    {"91470000", "1.2.840.10008.6.1.2", "Axilla"},
    {"82849001", "1.2.840.10008.6.1.2", "Retroperitoneum"},
    {"9875009", "1.2.840.10008.6.1.2", "Thymus"},
    {"34340008", "1.2.840.10008.6.1.2", "Venous network"},
    {"253276007", "1.2.840.10008.6.1.2", "Common atrium"},
    {"85050009", "1.2.840.10008.6.1.2", "Humerus"},
    {"22356005", "1.2.840.10008.6.1.2", "Ilium"},
    {"128551005", "1.2.840.10008.6.1.2", "Aortic fistula"},
    {"72021004", "1.2.840.10008.6.1.2", "Superior thyroid artery"},
    {"10293006", "1.2.840.10008.6.1.2", "Iliac artery"},
    {"74386004", "1.2.840.10008.6.1.2", "Nasal bone"},
    {"68053000", "1.2.840.10008.6.1.2", "Anterior tibial artery"},
    {"71252005", "1.2.840.10008.6.1.2", "Cervix"},
    {"61959006", "1.2.840.10008.6.1.2", "Truncus arteriosus communis"},
    {"51114001", "1.2.840.10008.6.1.2", "Endo-arterial"},
    {"32062004", "1.2.840.10008.6.1.2", "Common carotid artery"},
    {"39352004", "1.2.840.10008.6.1.2", "Joint"},
    {"46862004", "1.2.840.10008.6.1.2", "Buttock"},
    {"5713008", "1.2.840.10008.6.1.2", "Submandibular area"},
    {"55024004", "1.2.840.10008.6.1.2", "Optic canal"},
    {"15825003", "1.2.840.10008.6.1.2", "Aorta"},
    {"15672000", "1.2.840.10008.6.1.2", "Superficial temporal artery"},
    {"29092000", "1.2.840.10008.6.1.2", "Endo-venous"},
    {"59066005", "1.2.840.10008.6.1.2", "Mastoid bone"},
    {"128556000", "1.2.840.10008.6.1.2", "Congenital coronary artery fistula to left ventricle"},
    {"416319003", "1.2.840.10008.6.1.2", "Neck, Chest, Abdomen and Pelvis"},
    {"68367000", "1.2.840.10008.6.1.2", "Thigh"},
    {"78961009", "1.2.840.10008.6.1.2", "Spleen"},
    {"416550000", "1.2.840.10008.6.1.2", "Chest and Abdomen"},
    {"113273001", "1.2.840.10008.6.1.2", "Inferior right pulmonary vein"},
    {"44567001", "1.2.840.10008.6.1.2", "Trachea"},
    {"91830000", "1.2.840.10008.6.1.2", "Body conduit"},
    {"33564002", "1.2.840.10008.6.1.2", "Lower outer quadrant of breast"},
    {"70258002", "1.2.840.10008.6.1.2", "Ankle joint"},
    {"48367006", "1.2.840.10008.6.1.2", "Endo-vesical"},
    {"72410000", "1.2.840.10008.6.1.2", "Mediastinum"},
    {"113305005", "1.2.840.10008.6.1.2", "Cerebellum"},
    {"8017000", "1.2.840.10008.6.1.2", "Right ventricle inflow"},
    {"86570000", "1.2.840.10008.6.1.2", "Mesenteric artery"},
    {"43899006", "1.2.840.10008.6.1.2", "Popliteal artery"},
    {"68505006", "1.2.840.10008.6.1.2", "Left lower quadrant of abdomen"},
    {"431491007", "1.2.840.10008.6.1.2", "Upper urinary tract"},
    {"128567004", "1.2.840.10008.6.1.2", "Pulmonary venous atrium"},
    {"955009", "1.2.840.10008.6.1.2", "Bronchus"},
    {"58742003", "1.2.840.10008.6.1.2", "Sesamoid bones of foot"},
    {"90024005", "1.2.840.10008.6.1.2", "Internal iliac artery"},
    {"128586007", "1.2.840.10008.6.1.2", "Pulmonary chamber of cor triatriatum"},
    {"79741001", "1.2.840.10008.6.1.2", "Common bile duct"},
    {"48345005", "1.2.840.10008.6.1.2", "Superior vena cava"},
    {"44984001", "1.2.840.10008.6.1.2", "Ulnar artery"},
    {"87878005", "1.2.840.10008.6.1.2", "Left ventricle"},
    {"17388009", "1.2.840.10008.6.1.2", "Anterior spinal artery"},
    {"52612000", "1.2.840.10008.6.1.2", "Lumbar region"},
    {"27398004", "1.2.840.10008.6.1.2", "Omentum"},
    {"110612005", "1.2.840.10008.6.1.2", "Anus, rectum and sigmoid colon"},
    {"35819009", "1.2.840.10008.6.1.2", "Splenic vein"},
    {"71854001", "1.2.840.10008.6.1.2", "Colon"},
    {"54019009", "1.2.840.10008.6.1.2", "Submandibular gland"},
    {"133943005", "1.2.840.10008.6.1.2", "Left lumbar region"},
    {"122495006", "1.2.840.10008.6.1.2", "Thoracic spine"},
    {"362072009", "1.2.840.10008.6.1.2", "Saphenous vein"},
    {"28231008", "1.2.840.10008.6.1.2", "Gallbladder"},
    {"12691009", "1.2.840.10008.6.1.2", "Innominate artery"},
    {"22083002", "1.2.840.10008.6.1.2", "Splenic artery"},
    {"2841007", "1.2.840.10008.6.1.2", "Renal artery"},
    {"24136001", "1.2.840.10008.6.1.2", "Hip joint"},
    {"40983000", "1.2.840.10008.6.1.2", "Upper arm"},
    {"66019005", "1.2.840.10008.6.1.2", "Extremity"},
    {"816094009", "1.2.840.10008.6.1.2", "Thorax"},
    {"23074001", "1.2.840.10008.6.1.2", "Facial artery"},
    {"27947004", "1.2.840.10008.6.1.2", "Epigastric region"},
    {"72107004", "1.2.840.10008.6.1.2", "Azygos vein"},
    {"17137000", "1.2.840.10008.6.1.2", "Brachial artery"},
    {"10200004", "1.2.840.10008.6.1.2", "Liver"},
    {"29836001", "1.2.840.10008.6.1.2", "Hip"},
    {"113197003", "1.2.840.10008.6.1.2", "Rib"},
    {"371195002", "1.2.840.10008.6.1.2", "Bone of upper limb"},
    {"76784001", "1.2.840.10008.6.1.2", "Endo-vaginal"},
    {"19100000", "1.2.840.10008.6.1.2", "Lower inner quadrant of breast"},
    {"51299004", "1.2.840.10008.6.1.2", "Clavicle"},
    {"45289007", "1.2.840.10008.6.1.2", "Parotid gland"},
    {"181347005", "1.2.840.10008.6.1.2", "Common Femoral Artery"},
    {"816989007", "1.2.840.10008.6.1.2", "Intra-pelvic"},
    {"69695003", "1.2.840.10008.6.1.2", "Stomach"},
    {"5076001", "1.2.840.10008.6.1.2", "Subxiphoid"},
    {"77831004", "1.2.840.10008.6.1.2", "Upper inner quadrant of breast"},
    {"8012006", "1.2.840.10008.6.1.2", "Anterior communicating artery"},
    {"128555001", "1.2.840.10008.6.1.2", "Congenital coronary artery fistula to left atrium"},
    {"37117007", "1.2.840.10008.6.1.2", "Right inguinal region"},
    {"113262008", "1.2.840.10008.6.1.2", "Thoracic aorta"},
    {"35532006", "1.2.840.10008.6.1.2", "Vena cava"},
    {"90219004", "1.2.840.10008.6.1.2", "Coronary sinus"},
    {"67937003", "1.2.840.10008.6.1.2", "Axillary Artery"},
    {"50408007", "1.2.840.10008.6.1.2", "Left pulmonary artery"},
    {"34707002", "1.2.840.10008.6.1.2", "Biliary tract"},
    {"86598002", "1.2.840.10008.6.1.2", "Apex of Lung"},
    {"34516001", "1.2.840.10008.6.1.2", "Ileum"},
    {"59652004", "1.2.840.10008.6.1.2", "Atrium"},
    {"72696002", "1.2.840.10008.6.1.2", "Knee"},
    {"85234005", "1.2.840.10008.6.1.2", "Vertebral artery"},
    {"48544008", "1.2.840.10008.6.1.2", "Right lower quadrant of abdomen"},
    {"128566008", "1.2.840.10008.6.1.2", "Pulmonary vein confluence"},
    {"128587003", "1.2.840.10008.6.1.2", "Saphenofemoral junction"},
    {"12123001", "1.2.840.10008.6.1.2", "Internal jugular vein"},
    {"371398005", "1.2.840.10008.6.1.2", "Eye region"},
    {"128979005", "1.2.840.10008.6.1.2", "Lacrimal artery of right eye"},
    {"297173004", "1.2.840.10008.6.1.2", "Lumbo-sacral spine"},
    {"39723000", "1.2.840.10008.6.1.2", "Sacroiliac joint"},
    {"53505006", "1.2.840.10008.6.1.2", "Anus"},
    {"284639000", "1.2.840.10008.6.1.2", "Umbilical vein"},
    {"38266002", "1.2.840.10008.6.1.2", "Entire body"},
    {"46027005", "1.2.840.10008.6.1.2", "Common iliac vein"},
    {"26893007", "1.2.840.10008.6.1.2", "Inguinal region"},
    {"53342003", "1.2.840.10008.6.1.2", "Endo-nasal"},
    {"11708003", "1.2.840.10008.6.1.2", "Hypogastric region"},
    {"133944004", "1.2.840.10008.6.1.2", "Right lumbar region"},
    {"76505004", "1.2.840.10008.6.1.2", "Thumb"},
    {"122496007", "1.2.840.10008.6.1.2", "Lumbar spine"},
    {"5928000", "1.2.840.10008.6.1.2", "Great cardiac vein"},
    {"91707000", "1.2.840.10008.6.1.2", "Primitive pulmonary artery"},
    {"42575006", "1.2.840.10008.6.1.2", "Sella turcica"},
    {"32764006", "1.2.840.10008.6.1.2", "Portal vein"},
    {"20233005", "1.2.840.10008.6.1.2", "Scrotum"},
    {"51249003", "1.2.840.10008.6.1.2", "Inferior left pulmonary vein"},
    {"30021000", "1.2.840.10008.6.1.2", "Lower leg"},
    {"297174005", "1.2.840.10008.6.1.2", "Sacro-coccygeal Spine"},
    {"56400007", "1.2.840.10008.6.1.2", "Renal vein"},
    {"128548003", "1.2.840.10008.6.1.2", "Boyd's perforating vein"},
    {"117590005", "1.2.840.10008.6.1.2", "Ear"},
    {"77621008", "1.2.840.10008.6.1.2", "Supraclavicular region of neck"},
    {"73634005", "1.2.840.10008.6.1.2", "Common iliac artery"},
    {"29707007", "1.2.840.10008.6.1.2", "Toe"},
    {"16982005", "1.2.840.10008.6.1.2", "Shoulder"},
    {"2748008", "1.2.840.10008.6.1.2", "Spinal cord"},
    {"118375008", "1.2.840.10008.6.1.2", "Vascular graft"},
    {"33626005", "1.2.840.10008.6.1.2", "Left auricular appendage"},
    {"11279006", "1.2.840.10008.6.1.2", "Circle of Willis"},
    {"85119005", "1.2.840.10008.6.1.2", "Left inguinal region"},
    {"80243003", "1.2.840.10008.6.1.2", "Eyelid"},
    {"102292000", "1.2.840.10008.6.1.2", "Muscle of lower limb"},
    {"86367003", "1.2.840.10008.6.1.2", "Left upper quadrant of abdomen"},
    {"54247002", "1.2.840.10008.6.1.2", "Ascending aorta"},
    {"128560002", "1.2.840.10008.6.1.2", "Hunterian perforating vein"},
    {"19695001", "1.2.840.10008.6.1.2", "Subcostal"},
    {"31677005", "1.2.840.10008.6.1.2", "Profunda Femoris Artery"},
    {"54066008", "1.2.840.10008.6.1.2", "Pharynx"},
    {"84301002", "1.2.840.10008.6.1.2", "External auditory canal"},
    {"68705008", "1.2.840.10008.6.1.2", "Axillary vein"},
    {"64234005", "1.2.840.10008.6.1.2", "Patella"},
    {"360955006", "1.2.840.10008.6.1.2", "Nasopharynx"},
    {"32114007", "1.2.840.10008.6.1.2", "Occipital vein"},
    {"128554002", "1.2.840.10008.6.1.2", "Dodd's perforating vein"},
    {"1.2.840.10065.1.12.1.1", "2.16.840.1.113883.4.642.4.64", "Author's Signature"},
    {"1.2.840.10065.1.12.1.2", "2.16.840.1.113883.4.642.4.64", "Coauthor's Signature"},
    {"1.2.840.10065.1.12.1.3", "2.16.840.1.113883.4.642.4.64", "Co-participant's Signature"},
    {"1.2.840.10065.1.12.1.4",
     "2.16.840.1.113883.4.642.4.64",
     "Transcriptionist/Recorder Signature"},
    {"1.2.840.10065.1.12.1.5", "2.16.840.1.113883.4.642.4.64", "Verification Signature"},
    {"1.2.840.10065.1.12.1.6", "2.16.840.1.113883.4.642.4.64", "Validation Signature"},
    {"1.2.840.10065.1.12.1.7", "2.16.840.1.113883.4.642.4.64", "Consent Signature"},
    {"1.2.840.10065.1.12.1.8", "2.16.840.1.113883.4.642.4.64", "Signature Witness Signature"},
    {"1.2.840.10065.1.12.1.9", "2.16.840.1.113883.4.642.4.64", "Event Witness Signature"},
    {"1.2.840.10065.1.12.1.10", "2.16.840.1.113883.4.642.4.64", "Identity Witness Signature"},
    {"1.2.840.10065.1.12.1.11", "2.16.840.1.113883.4.642.4.64", "Consent Witness Signature"},
    {"1.2.840.10065.1.12.1.12", "2.16.840.1.113883.4.642.4.64", "Interpreter Signature"},
    {"1.2.840.10065.1.12.1.13", "2.16.840.1.113883.4.642.4.64", "Review Signature"},
    {"1.2.840.10065.1.12.1.14", "2.16.840.1.113883.4.642.4.64", "Source Signature"},
    {"1.2.840.10065.1.12.1.15", "2.16.840.1.113883.4.642.4.64", "Addendum Signature"},
    {"1.2.840.10065.1.12.1.16", "2.16.840.1.113883.4.642.4.64", "Modification Signature"},
    {"1.2.840.10065.1.12.1.17",
     "2.16.840.1.113883.4.642.4.64",
     "Administrative (Error/Edit) Signature"},
    {"1.2.840.10065.1.12.1.18", "2.16.840.1.113883.4.642.4.64", "Timestamp Signature"},
    {"BDUS", "1.2.840.10008.2.16.4", "Ultrasound Bone Densitometry"},
    {"BMD", "1.2.840.10008.2.16.4", "Bone Mineral Densitometry"},
    {"CR", "1.2.840.10008.2.16.4", "Computed Radiography"},
    {"CT", "1.2.840.10008.2.16.4", "Computed Tomography"},
    {"DX", "1.2.840.10008.2.16.4", "Digital Radiography"},
    {"ES", "1.2.840.10008.2.16.4", "Endoscopy"},
    {"GM", "1.2.840.10008.2.16.4", "General Microscopy"},
    {"IO", "1.2.840.10008.2.16.4", "Intra-oral Radiography"},
    {"IVOCT", "1.2.840.10008.2.16.4", "Intravascular Optical Coherence Tomography"},
    {"IVUS", "1.2.840.10008.2.16.4", "Intravascular Ultrasound"},
    {"MG", "1.2.840.10008.2.16.4", "Mammography"},
    {"MR", "1.2.840.10008.2.16.4", "Magnetic Resonance"},
    {"NM", "1.2.840.10008.2.16.4", "Nuclear Medicine"},
    {"OCT", "1.2.840.10008.2.16.4", "Optical Coherence Tomography"},
    {"OP", "1.2.840.10008.2.16.4", "Ophthalmic Photography"},
    {"OPT", "1.2.840.10008.2.16.4", "Ophthalmic Tomography"},
    {"OPTENF", "1.2.840.10008.2.16.4", "Ophthalmic Tomography En Face"},
    {"PT", "1.2.840.10008.2.16.4", "Positron emission tomography"},
    {"PX", "1.2.840.10008.2.16.4", "Panoramic X-Ray"},
    {"RF", "1.2.840.10008.2.16.4", "Radiofluoroscopy"},
    {"RG", "1.2.840.10008.2.16.4", "Radiographic imaging"},
    {"SM", "1.2.840.10008.2.16.4", "Slide Microscopy"},
    {"US", "1.2.840.10008.2.16.4", "Ultrasound"},
    {"XA", "1.2.840.10008.2.16.4", "X-Ray Angiography"},
    {"XC", "1.2.840.10008.2.16.4", "External-camera Photography"},
    {"E100", "1.3.6.1.4.1.19376.3.276.1.5.16", "ambulanter Kontakt"},
    {"E110", "1.3.6.1.4.1.19376.3.276.1.5.16", "ambulante OP"},
    {"E200", "1.3.6.1.4.1.19376.3.276.1.5.16", "stationärer Aufenthalt"},
    {"E210", "1.3.6.1.4.1.19376.3.276.1.5.16", "stationäre Aufnahme"},
    {"E211", "1.3.6.1.4.1.19376.3.276.1.5.16", "Aufnahme vollstationär"},
    {"E212", "1.3.6.1.4.1.19376.3.276.1.5.16", "Aufnahme/Wiederaufnahme teilstationär"},
    {"E213", "1.3.6.1.4.1.19376.3.276.1.5.16", "Aufnahme Entbindung stationär"},
    {"E214", "1.3.6.1.4.1.19376.3.276.1.5.16", "Aufnahme eines Neugeborenen"},
    {"E215", "1.3.6.1.4.1.19376.3.276.1.5.16", "Aufnahme des Spenders zur Organentnahme"},
    {"E230", "1.3.6.1.4.1.19376.3.276.1.5.16", "stationäre Entlassung"},
    {"E231", "1.3.6.1.4.1.19376.3.276.1.5.16", "stationäre Entlassung nach Hause"},
    {"E232",
     "1.3.6.1.4.1.19376.3.276.1.5.16",
     "stationäre Entlassung in eine Rehabilitationseinrichtung"},
    {"E233",
     "1.3.6.1.4.1.19376.3.276.1.5.16",
     "stationäre Entlassung in eine Pflegeeinrichtung/Hospiz"},
    {"E234", "1.3.6.1.4.1.19376.3.276.1.5.16", "Entlassung zur nachstationären Behandlung"},
    {"E235", "1.3.6.1.4.1.19376.3.276.1.5.16", "Patient während stationärem Aufenthalt verstorben"},
    {"E250", "1.3.6.1.4.1.19376.3.276.1.5.16", "stationäre Verlegung"},
    {"E251", "1.3.6.1.4.1.19376.3.276.1.5.16", "Verlegung innerhalb eines Krankenhauses"},
    {"E252", "1.3.6.1.4.1.19376.3.276.1.5.16", "Verlegung in ein anderes Krankenhaus"},
    {"E253", "1.3.6.1.4.1.19376.3.276.1.5.16", "externe Verlegung in Psychiatrie"},
    {"E270",
     "1.3.6.1.4.1.19376.3.276.1.5.16",
     "kurzzeitige Unterbrechung einer stationären Behandlung"},
    {"E280", "1.3.6.1.4.1.19376.3.276.1.5.16", "Konsil"},
    {"E300", "1.3.6.1.4.1.19376.3.276.1.5.16", "Behandlung im häuslichen Umfeld"},
    {"E400", "1.3.6.1.4.1.19376.3.276.1.5.16", "Virtual Encounter"},
    {"H1", "1.3.6.1.4.1.19376.3.276.1.5.15", "vom Patienten mitgebracht"},
    {"H2", "1.3.6.1.4.1.19376.3.276.1.5.15", "noch nicht mit Patient besprochen"},
    {"H3", "1.3.6.1.4.1.19376.3.276.1.5.15", "eventuell veraltete Daten"},
    {"H4", "1.3.6.1.4.1.19376.3.276.1.5.15", "vorläufiges Dokument"},
    {"urn:ihe:iti:xdw:2011:eventCode:open", "1.3.6.1.4.1.19376.1.2.3", "Workflow offen"},
    {"urn:ihe:iti:xdw:2011:eventCode:closed", "1.3.6.1.4.1.19376.1.2.3", "Workflow abgeschlossen"},
    {"00", "1.2.276.0.76.5.223", "nicht gesetzt"},
    {"01", "1.2.276.0.76.5.223", "DM2"},
    {"02", "1.2.276.0.76.5.223", "BRK"},
    {"03", "1.2.276.0.76.5.223", "KHK"},
    {"04", "1.2.276.0.76.5.223", "DM1"},
    {"05", "1.2.276.0.76.5.223", "Asthma"},
    {"06", "1.2.276.0.76.5.223", "COPD"},
    {"07", "1.2.276.0.76.5.223", "HI"},
    {"08", "1.2.276.0.76.5.223", "Depression"},
    {"09", "1.2.276.0.76.5.223", "Rueckenschmerz"},
    {"10", "1.2.276.0.76.5.223", "Rheuma"},
    {"11", "1.2.276.0.76.5.223", "Osteoporose"},
};


const BinaryDocument::CodedValueEntryRefToIndexMap BinaryDocument::mCodedValueIndices = []() {
    CodedValueEntryRefToIndexMap initMap;
    for (size_t index = 0; index < mCodedValues.size(); index++)
        initMap.emplace(mCodedValues[index], index);
    return initMap;
}();

} // namespace epa
