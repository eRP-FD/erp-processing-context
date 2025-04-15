/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/cbor/CborReader.hxx"
#include "library/stream/ReadableByteStream.hxx"
#include "library/util/Assert.hxx"
#include "library/util/Serialization.hxx"
#include "library/util/cbor/Cbor.hxx"
#include "library/util/cbor/CborError.hxx"
#include "library/wrappers/GLog.hxx"

#include <limits>
#include <sstream>

#include "shared/util/Base64.hxx"

namespace epa
{

// NOLINTBEGIN(misc-no-recursion)

CborReader::CborReader(ReadableByteStream&& input, const size_t maxLength)
  : mInput(std::move(input)),
    mMaxLength(maxLength)
{
}


CborReader::CborReader(Stream&& input, const size_t maxLength)
  : CborReader(ReadableByteStream(std::move(input)), maxLength)
{
}


CborReader::CborReader(std::string&& input, const size_t maxLength)
  : CborReader(StreamFactory::makeReadableStream(std::move(input)), maxLength)
{
}


CborReader::CborReader(const BinaryBuffer& input, const size_t maxLength)
  : CborReader(StreamFactory::makeReadableStream(input.data(), input.size()), maxLength)
{
}


Cbor::ItemHead CborReader::parseItemHead()
{
    if (mInput.isAtEnd())
        return Cbor::ItemHead{
            .type = Cbor::Type::EOS, .majorType = Cbor::MajorType::EOS, .info = {}, .length = 0};

    const uint8_t b = mInput.readByte();
    const Cbor::MajorType majorType = Cbor::getMajorType(b);
    const Cbor::Type type = Cbor::getType(b);
    const Cbor::Info info = Cbor::getInfo(b);

    // For small values, the numeric value (in case of strings it denotes the length of the
    // string, hence its name) is encoded in the header byte. For longer values, we have to
    // read 1 to 8 more bytes.
    const uint64_t length = readInfoLength(info);
    return Cbor::ItemHead{.type = type, .majorType = majorType, .info = info, .length = length};
}


ReadableByteStream& CborReader::stream()
{
    return mInput;
}


size_t CborReader::maxLength() const
{
    return mMaxLength;
}


void CborReader::read(Callback callback)
{
    // This top level `read` method will recursively call specialized variants to read
    // specific data types. The top most is an implicit array.
    readPlain(callback, "");
}


//NOLINTBEGIN(readability-function-cognitive-complexity)
Cbor::ItemHead CborReader::readValue(Callback& callback, const std::string& path)
{
    const auto itemHead = parseItemHead();
    switch (itemHead.type)
    {
        case Cbor::Type::Array:
            readArray(callback, path, itemHead.length);
            break;
        case Cbor::Type::Break:
            break;
        case Cbor::Type::ByteString:
            readString(callback, path, itemHead);
            break;
        case Cbor::Type::EOS:
            // Let the caller handle this.
            break;
        case Cbor::Type::False:
            callback(path, itemHead.type, true, false);
            break;
        case Cbor::Type::Map:
            readMap(callback, path, itemHead.length);
            break;
        case Cbor::Type::NegativeInteger:
            // Depending on the value, we use the smallest signed integer type that can hold
            // the value. This helps the callback to assign the value to members without running
            // into problems with incompatible (with respect to value range) types.
            if (itemHead.length <= std::numeric_limits<int16_t>::max())
            {
                if (itemHead.length <= std::numeric_limits<int8_t>::max())
                    callback(path, itemHead.type, true, static_cast<int8_t>(itemHead.length));
                else
                    callback(path, itemHead.type, true, static_cast<int16_t>(itemHead.length));
            }
            else
            {
                if (itemHead.length <= std::numeric_limits<int32_t>::max())
                    callback(path, itemHead.type, true, static_cast<int32_t>(itemHead.length));
                else
                {
                    // Because CBOR stores the sign of negative integers separately, it has
                    // an extra bit for the value. That means that for 64-bit integers we
                    // have to check that the top-most bit is not used, because int64_t uses
                    // that to express the sign.
                    Assert(itemHead.length <= std::numeric_limits<int64_t>::max());
                    callback(path, itemHead.type, true, static_cast<int64_t>(itemHead.length));
                }
            }
            break;
        case Cbor::Type::TextString:
            readString(callback, path, itemHead);
            break;
        case Cbor::Type::True:
            callback(path, itemHead.type, true, true);
            break;
        case Cbor::Type::UnsignedInteger:
            // Depending on the value, we use the smallest unsigned integer type that can hold
            // the value. This helps the callback to assign the value to members without running
            // into problems with incompatible (with respect to value range) types.
            if (itemHead.length <= std::numeric_limits<uint16_t>::max())
            {
                if (itemHead.length <= std::numeric_limits<uint8_t>::max())
                    callback(path, itemHead.type, true, static_cast<uint8_t>(itemHead.length));
                else
                    callback(path, itemHead.type, true, static_cast<uint16_t>(itemHead.length));
            }
            else
            {
                if (itemHead.length <= std::numeric_limits<uint32_t>::max())
                    callback(path, itemHead.type, true, static_cast<uint32_t>(itemHead.length));
                else
                    callback(path, itemHead.type, true, static_cast<uint64_t>(itemHead.length));
            }
            break;
        case Cbor::Type::Null:
            callback(path, itemHead.type, true, std::monostate{});
            break;
        case Cbor::Type::Double:
        case Cbor::Type::Float:
        case Cbor::Type::HalfFloat:
        case Cbor::Type::Simple:
        case Cbor::Type::Tag:
            Failure() << "CBOR type " << Cbor::toString(itemHead.type) << " is not supported";
    }
    return itemHead;
}
//NOLINTEND(readability-function-cognitive-complexity)


void CborReader::readPlain(Callback& callback, const std::string& path)
{
    size_t index = 0;
    while (true)
    {
        // Use an array notation to name objects.
        const auto itemHead = readValue(callback, path + "/[" + std::to_string(index) + "]");

        // Expect the synthetic EOS when the scanner has reached the end of the stream.
        if (itemHead.type == Cbor::Type::EOS)
            break;

        // A break is not expected. The plain section ends when the stream ends.
        Assert(itemHead.type != Cbor::Type::Break);

        ++index;
    }
}


void CborReader::readArray(Callback& callback, const std::string& path, const size_t itemCount)
{
    std::vector<BinaryBuffer> items;

    Callback appendCallback =
        [&items](std::string_view path, Cbor::Type type, bool isLast, Cbor::Data data) mutable {
            (void) path;
            (void) isLast;
            Assert(type == Cbor::Type::ByteString)
                << "arrays with types other than byte string are not supported";
            items.emplace_back(std::get<StreamBuffers>(data).toString());
        };

    size_t index = 0;
    while (itemCount == 0 || index < itemCount)
    {
        const auto itemHead = readValue(appendCallback, path + "/[" + std::to_string(index) + "]");
        Assert(itemHead.type != Cbor::Type::EOS) << "unexpected stream end inside CBOR array";
        if (itemHead.type == Cbor::Type::Break)
        {
            Assert(itemCount == 0) << "got break in non-indefinite array of size " << itemCount;
            break;
        }
        ++index;
    }

    callback(path, Cbor::Type::Array, true, items);
}


void CborReader::readMap(Callback& callback, const std::string& path, const size_t pairCount)
{
    size_t index = 0;
    while (pairCount == 0 || index < pairCount)
    {
        // Read the key. In the Gematik Specifications there don't seem to be any use cases
        // where the key is not a text string. However, the CBOR RFC does not disallow other
        // types. Therefore, we have rudimentary support for types other than text strings.
        std::string key;
        const auto itemHead = parseItemHead();
        switch (itemHead.type)
        {
            case Cbor::Type::Break:
                // A Break is only valid when the map is indefinite.
                Assert(pairCount == 0) << "unexpected break in definite map of size " << pairCount
                                       << " on " << index << "-th item";
                return;
            case Cbor::Type::EOS:
                Failure() << "unexpected end of stream inside map";
            case Cbor::Type::TextString:
                Assert(itemHead.info != Cbor::Info::Indefinite)
                    << "indefinite texts are not supported as map key";
                Assert(itemHead.length <= mMaxLength);
                key = mInput.readExactly(itemHead.length).toString();
                break;
            case Cbor::Type::ByteString:
                Assert(itemHead.info != Cbor::Info::Indefinite)
                    << "indefinite byte strings are not supported as map key";
                Assert(itemHead.length <= mMaxLength);
                key = mInput.readExactly(itemHead.length).toString();
                break;
            case Cbor::Type::UnsignedInteger:
                key = std::to_string(itemHead.length);
                break;
            case Cbor::Type::NegativeInteger:
                Assert(itemHead.length < std::numeric_limits<int64_t>::max())
                    << "value too large for a 64 bit integer, it can not be negated";
                key = std::to_string(-static_cast<int64_t>(itemHead.length));
                break;
            case Cbor::Type::Array:
            case Cbor::Type::Map:
                key = toString(itemHead, ToStringMode::ItemCount, 1);
                break;

            default:
                Failure() << "CBOR type " << Cbor::toString(itemHead.type) << "not yet supported";
        }

        // Read the value.
        readValue(callback, path + "/" + key); // NOLINT

        ++index;
    }
}


void CborReader::readString(
    Callback& callback,
    const std::string& path,
    const Cbor::ItemHead initialItemHead)
{
    // When itemHead.info == Indefinite, read definite ByteString or TextString items
    // until the next Break item.
    // Otherwise, read only the data that follows the already read item head.
    // In either case, obey the mMaxValue and split larger items into smaller chunks where
    // necessary.
    bool loop = true;
    while (loop)
    {
        const auto itemHead =
            initialItemHead.info == Cbor::Info::Indefinite ? parseItemHead() : initialItemHead;

        if (itemHead.type == Cbor::Type::Break)
        {
            callback(
                path, initialItemHead.type, true, Cbor::Data(StreamBuffers(StreamBuffer::IsLast)));
            break;
        }

        size_t remaining = itemHead.length;
        if (remaining == 0)
        {
            // Special handling for an empty string.
            callback(
                path, initialItemHead.type, true, Cbor::Data(StreamBuffers(StreamBuffer::IsLast)));
        }
        else
        {
            while (remaining > 0)
            {
                auto buffers = mInput.readAtMost(std::min(remaining, mMaxLength));
                Assert(buffers.size() > 0);
                remaining -= buffers.size();

                const bool isLast =
                    remaining == 0 && initialItemHead.info != Cbor::Info::Indefinite;

                callback(path, initialItemHead.type, isLast, Cbor::Data(std::move(buffers)));
            }
        }

        // When this is a definite item then there will not be another item and we can stop here.
        if (initialItemHead.info != Cbor::Info::Indefinite)
            loop = false;
    }
}


uint64_t CborReader::readInfoLength(const Cbor::Info info)
{
    if (info <= Cbor::Info::LargestDirectValue)
        return static_cast<uint8_t>(info);
    else if (info == Cbor::Info::OneByteArgument)
        return readUint8();
    else if (info == Cbor::Info::TwoByteArgument)
        return readUint16();
    else if (info == Cbor::Info::FourByteArgument)
        return readUint32();
    else if (info == Cbor::Info::EightByteArgument)
        return readUint64();
    else if (info == Cbor::Info::Indefinite)
        return 0;
    else
        Failure() << "invalid or unsupported CBOR info for unsigned integer";
    return 0;
}


uint8_t CborReader::readUint8()
{
    return mInput.readByte();
}


uint16_t CborReader::readUint16()
{
    return Serialization::uint16::read([this] { return readUint8(); });
}


uint32_t CborReader::readUint32()
{
    return Serialization::uint32::read([this] { return readUint8(); });
}


uint64_t CborReader::readUint64()
{
    return Serialization::uint64::read([this] { return readUint8(); });
}


std::string CborReader::toString(const ToStringMode mode, size_t itemCount)
{
    return toString(parseItemHead(), mode, itemCount);
}

//NOLINTBEGIN(readability-function-cognitive-complexity)
std::string CborReader::toString(
    const Cbor::ItemHead& initialItemHead,
    const ToStringMode mode,
    size_t itemCount)
{
    std::string s;

    bool atStart = true;
    bool loop = true;
    while (loop)
    {
        const auto itemHead = atStart ? initialItemHead : parseItemHead();
        atStart = false;
        s += "<" + std::string(Cbor::toString(itemHead.type)) + ","
             + std::to_string(static_cast<uint8_t>(itemHead.info)) + ","
             + std::to_string(itemHead.length) + ">[";
        switch (itemHead.type)
        {
            case Cbor::Type::Array:
                if (itemHead.info == Cbor::Info::Indefinite)
                    s += toString(ToStringMode::Break, 0);
                else
                    s += toString(ToStringMode::ItemCount, itemHead.length);
                break;

            case Cbor::Type::Break:
                if (mode == Break)
                    loop = false;
                break;

            case Cbor::Type::ByteString:
                if (itemHead.info == Cbor::Info::Indefinite)
                    s += toString(ToStringMode::Break, 0);
                else
                    s += Base64::encode(stream().readExactly(itemHead.length).toString());
                break;

            case Cbor::Type::EOS:
                loop = false;
                break;

            case Cbor::Type::False:
                s += "false";
                break;

            case Cbor::Type::Map:
                if (itemHead.info == Cbor::Info::Indefinite)
                    s += toString(ToStringMode::Break, 0);
                else
                    s += toString(ToStringMode::ItemCount, itemHead.length * 2);
                break;

            case Cbor::Type::NegativeInteger:
                // Handled before the switch.
                break;

            case Cbor::Type::Null:
                s += "null";
                break;

            case Cbor::Type::TextString:
                if (itemHead.info == Cbor::Info::Indefinite)
                    s += toString(ToStringMode::Break, 0);
                else
                    s += stream().readExactly(itemHead.length).toString();
                break;

            case Cbor::Type::True:
                s += "true";
                break;

            case Cbor::Type::UnsignedInteger:
                // Handled before the switch.
                break;

            default:
                s += "<unsupported type " + std::string(Cbor::toString(itemHead.type)) + ">";
                break;
        }

        s += "]\n";

        if (itemCount > 0)
            if (--itemCount == 0 && mode == ItemCount)
                loop = false;
    }

    return s;
}
//NOLINTEND(readability-function-cognitive-complexity)

CborReader::ByteStringReader::ByteStringReader(CborReader& input)
  : StringReaderBase(input, Cbor::Type::ByteString)
{
}


CborReader::TextStringReader::TextStringReader(CborReader& input)
  : StringReaderBase(input, Cbor::Type::TextString)
{
}


CborReader::StringReaderBase::StringReaderBase(CborReader& input, const Cbor::Type type)
  : mReader(input),
    mType(type),
    mRemaining(0),
    mIsDefinite(true),
    mIsFinished(false)
{
    const auto itemHead = mReader.parseItemHead();
    Assert(itemHead.type == mType) << "unexpected CBOR type " << Cbor::toString(itemHead.type);
    if (itemHead.info == Cbor::Info::Indefinite)
        mIsDefinite = false;
    else
        mRemaining = itemHead.length;
}


StreamBuffers CborReader::StringReaderBase::read()
{
    while (mRemaining == 0 && ! mIsFinished)
    {
        if (mIsDefinite)
            mIsFinished = true;
        else
        {
            const auto itemHead = mReader.parseItemHead();
            if (itemHead.type == mType && itemHead.info != Cbor::Info::Indefinite)
                mRemaining = itemHead.length;
            else if (itemHead.type == Cbor::Type::Break && itemHead.info == Cbor::Info::Indefinite)
                mIsFinished = true;
            else
                Failure() << "unsupported nested item in indefinite byte string";
        }
    }

    if (mIsFinished)
        return StreamBuffers(StreamBuffer::IsLast);
    else
    {
        StreamBuffers stringBuffers(StreamBuffer::NotLast);
        while (mRemaining > 0)
        {
            auto buffers = mReader.stream().readAtMost(mRemaining);
            Assert(buffers.size() > 0) << "got no more bytes for CBOR string";
            Assert(buffers.size() <= mRemaining) << "got more bytes for CBOR string than requested";
            mRemaining -= buffers.size();

            stringBuffers.pushBack(std::move(buffers));
        }
        return stringBuffers;
    }
}


std::string CborReader::StringReaderBase::readContent()
{
    std::string result;
    size_t length = 0;
    while (true)
    {
        const auto buffers = read();
        for (const auto& buffer : buffers)
        {
            Assert(length + buffer.size() <= mReader.maxLength())
                << "CBOR byte string longer than permitted";
            result += std::string_view(buffer.data(), buffer.size());
            length += buffer.size();
        }
        if (buffers.isLast())
            break;
    }
    return result;
}

} // namespace epa

// NOLINTEND(misc-no-recursion)
