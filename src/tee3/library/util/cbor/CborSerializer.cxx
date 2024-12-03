/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/cbor/CborSerializer.hxx"
#include "library/stream/ReadableByteStream.hxx"
#include "library/util/Assert.hxx"

#include <sstream>

namespace epa
{

/**
 * Use a macro to provide a frequently used assertion, so that the original line number
 * is printed but code duplication can still be avoided.
 */
#define assertNotEos()                                                                             \
    Assert(! mEos) << "end of stream has already been written, can not append more data"


CborSerializer::CborSerializer(Stream& output)
  : mWriter(output),
    mCurrentContext{.type = Context::Plain, .missing = 0},
    mContextStack(),
    mEos(false)
{
}


CborSerializer::~CborSerializer()
{
    notifyEnd();
}


CborSerializer& CborSerializer::unsignedInteger(const uint64_t value)
{
    assertNotEos();
    mCurrentContext.decreaseMissing();
    mWriter.writeMajorType(Cbor::MajorType::UnsignedInteger, value);
    return *this;
}


CborSerializer& CborSerializer::negativeInteger(const int64_t value)
{
    Assert(value < 0);
    assertNotEos();
    mCurrentContext.decreaseMissing();
    mWriter.writeMajorType(Cbor::MajorType::NegativeInteger, static_cast<uint64_t>(-value));
    return *this;
}


CborSerializer& CborSerializer::integer(const int64_t value)
{
    // When the 64-th bit is important then the caller should have called either
    // `unsignedInteger()` or `negativeInteger()` directly. In this method we assume, that
    // the top most bit does not matter.
    if (value < 0)
        return negativeInteger(value);
    else
        return unsignedInteger(static_cast<uint64_t>(value));
}


CborSerializer& CborSerializer::boolean(const bool trueOrFalse)
{
    assertNotEos();
    mCurrentContext.decreaseMissing();
    mWriter.writeSimple(trueOrFalse ? Cbor::Simple::True : Cbor::Simple::False);
    return *this;
}


CborSerializer& CborSerializer::text(std::string_view value)
{
    assertNotEos();
    mCurrentContext.decreaseMissing();
    mWriter.writeMajorType(Cbor::MajorType::TextString, value.size());
    mWriter.writeBytes(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    return *this;
}


CborSerializer& CborSerializer::startArray(const size_t itemCount)
{
    assertNotEos();
    mCurrentContext.decreaseMissing();
    mWriter.writeMajorType(Cbor::MajorType::Array, itemCount);
    pushContextStack({.type = Context::Array, .missing = itemCount});
    return *this;
}


CborSerializer& CborSerializer::startArray()
{
    assertNotEos();
    mCurrentContext.decreaseMissing();
    mWriter.writeMajorTypeIndefinite(Cbor::MajorType::Array);
    pushContextStack({.type = Context::ArrayIndefinite, .missing = 0});
    return *this;
}


CborSerializer& CborSerializer::finishArray()
{
    Assert(mCurrentContext.isArray());
    assertNotEos();
    if (mCurrentContext.type == Context::ArrayIndefinite)
        mWriter.writeBreak();
    else
        Assert(mCurrentContext.missing == 0)
            << "array is still missing " << mCurrentContext.missing << " items";
    popContextStack();
    return *this;
}


CborSerializer& CborSerializer::startMap()
{
    assertNotEos();
    mCurrentContext.decreaseMissing();
    mWriter.writeMajorTypeIndefinite(Cbor::MajorType::Map);
    pushContextStack({.type = Context::MapIndefinite, .missing = 0});
    return *this;
}


CborSerializer& CborSerializer::startMap(size_t pairCount)
{
    assertNotEos();
    mCurrentContext.decreaseMissing();
    mWriter.writeMajorType(Cbor::MajorType::Map, pairCount);
    const size_t itemCount = pairCount * 2;
    pushContextStack({.type = Context::Map, .missing = itemCount});
    return *this;
}


CborSerializer& CborSerializer::finishMap()
{
    Assert(mCurrentContext.isMap());
    assertNotEos();
    if (mCurrentContext.type == Context::MapIndefinite)
        mWriter.writeBreak();
    else
        Assert(mCurrentContext.missing == 0)
            << "map is still missing " << mCurrentContext.missing << " pairs";
    popContextStack();
    return *this;
}


CborSerializer& CborSerializer::key(std::string_view value)
{
    Assert(mCurrentContext.isMap());
    Assert(mCurrentContext.type == Context::MapIndefinite || mCurrentContext.missing > 0);
    assertNotEos();
    mCurrentContext.decreaseMissing();
    mWriter.writeMajorType(Cbor::MajorType::TextString, value.size());
    mWriter.writeBytes(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    return *this;
}


void CborSerializer::notifyEnd()
{
    if (! mEos)
    {
        // In case the caller forgot to (or intentionally did not) call pending finish...() methods,
        // we do that here to produce the necessary Break items.
        while (mCurrentContext.type != Context::Plain)
        {
            switch (mCurrentContext.type)
            {
                case Context::Map:
                case Context::MapIndefinite:
                    finishMap();
                    break;
                case Context::Array:
                case Context::ArrayIndefinite:
                    finishArray();
                    break;
                default:
                    popContextStack();
                    break;
            }
        }

        // Flush any pending buffers to the stream and make sure that the last written buffer
        // has the IsLast bit set.
        mWriter.flushBuffer(StreamBuffer::IsLast);

        // Make sure that the code above is executed only once and that following calls that
        // attempt to write more data will be caught.
        mEos = true;
    }
}


CborSerializer& CborSerializer::binary(const BinaryBuffer& value)
{
    return binary(std::string_view(reinterpret_cast<const char*>(value.data()), value.size()));
}


CborSerializer& CborSerializer::binary(const std::string_view value)
{
    assertNotEos();
    mCurrentContext.decreaseMissing();
    mCurrentContext.assertCanWriteBinary();
    mWriter.writeMajorType(Cbor::MajorType::ByteString, value.size());
    mWriter.writeBytes(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    return *this;
}


CborSerializer& CborSerializer::binary(StreamBuffers&& buffers)
{
    assertNotEos();
    mCurrentContext.decreaseMissing();
    mCurrentContext.assertCanWriteBinary();
    mWriter.writeMajorType(Cbor::MajorType::ByteString, buffers.size());
    mWriter.writeBuffers(std::move(buffers));
    return *this;
}


CborSerializer& CborSerializer::startBinary()
{
    assertNotEos();
    mWriter.writeMajorTypeIndefinite(Cbor::MajorType::ByteString);
    pushContextStack({.type = Context::BinaryIndefinite, .missing = 0});
    return *this;
}


CborSerializer& CborSerializer::finishBinary()
{
    Assert(mCurrentContext.appendingBinary());
    assertNotEos();
    if (mCurrentContext.type == Context::BinaryIndefinite)
        mWriter.writeBreak();
    popContextStack();
    return *this;
}


void CborSerializer::pushContextStack(Context context)
{
    mContextStack.emplace(mCurrentContext);
    mCurrentContext = context;
}


void CborSerializer::popContextStack()
{
    Assert(! mContextStack.empty());
    mCurrentContext = mContextStack.top();
    mContextStack.pop();
}


void CborSerializer::Context::decreaseMissing()
{
    if (type == Array || type == Map)
    {
        Assert(missing > 0);
        --missing;
    }
}


void CborSerializer::Context::assertCanWriteBinary() const
{
    switch (type)
    {
        case Key:
        case Text:
        case TextIndefinite:
            Failure() << "can not write binary data";

        default:
            break;
    }
}


bool CborSerializer::Context::appendingBinary() const
{
    return type == Binary || type == BinaryIndefinite;
}


bool CborSerializer::Context::isArray() const
{
    return type == Context::Array || type == Context::ArrayIndefinite;
}


bool CborSerializer::Context::isMap() const
{
    return type == Context::Map || type == Context::MapIndefinite;
}

} // namespace epa
