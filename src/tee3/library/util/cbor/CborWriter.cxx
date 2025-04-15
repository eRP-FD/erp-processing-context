/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/cbor/CborWriter.hxx"
#include "library/stream/ReadableByteStream.hxx"
#include "library/util/Assert.hxx"
#include "library/util/Serialization.hxx"

#include <sstream>

namespace epa
{

CborWriter::CborWriter(Stream& output)
  : mOutput(output),
    mBuffer(),
    mUsed(0)
{
}


CborWriter::~CborWriter()
{
    flushBuffer(StreamBuffer::IsLast);
}


void CborWriter::writeMajorType(const Cbor::MajorType majorType, const uint64_t value)
{
    const auto shiftedMajorType =
        static_cast<uint8_t>(static_cast<uint8_t>(majorType) << Cbor::CborMajorTypeShift);

    // Find the smallest integer type that can express the `value`.
    if (value <= std::numeric_limits<uint16_t>::max())
    {
        if (value <= static_cast<uint8_t>(Cbor::Info::LargestDirectValue))
        {
            writeUint8(shiftedMajorType | static_cast<uint8_t>(value));
        }
        else if (value <= std::numeric_limits<uint8_t>::max())
        {
            writeUint8(shiftedMajorType | static_cast<uint8_t>(Cbor::Info::OneByteArgument));
            writeUint8(static_cast<uint8_t>(value));
        }
        else
        {
            writeUint8(shiftedMajorType | static_cast<uint8_t>(Cbor::Info::TwoByteArgument));
            writeUint16(static_cast<uint16_t>(value));
        }
    }
    else
    {
        if (value <= std::numeric_limits<uint32_t>::max())
        {
            writeUint8(shiftedMajorType | static_cast<uint8_t>(Cbor::Info::FourByteArgument));
            writeUint32(static_cast<uint32_t>(value));
        }
        else
        {
            writeUint8(shiftedMajorType | static_cast<uint8_t>(Cbor::Info::EightByteArgument));
            writeUint64(static_cast<uint64_t>(value));
        }
    }
}


void CborWriter::writeMajorTypeIndefinite(const Cbor::MajorType majorType)
{
    const auto shiftedMajorType =
        static_cast<uint8_t>(static_cast<uint8_t>(majorType) << Cbor::CborMajorTypeShift);
    writeUint8(shiftedMajorType | static_cast<uint8_t>(Cbor::Info::Indefinite));
}


void CborWriter::writeSimple(const Cbor::Simple value)
{
    const uint8_t shiftedMajorType = static_cast<uint8_t>(Cbor::MajorType::BreakSimpleFloat)
                                     << Cbor::CborMajorTypeShift;
    writeUint8(shiftedMajorType | static_cast<uint8_t>(value));
}


void CborWriter::writeBreak()
{
    writeSimple(Cbor::Simple::Break);
}


void CborWriter::flushBuffer(const bool isLast)
{
    if (mUsed > 0)
    {
        mOutput.write(
            StreamBuffers::from(reinterpret_cast<const char*>(mBuffer.data()), mUsed, isLast));
        mUsed = 0;
    }
    else if (isLast)
    {
        // There is nothing to flush, but in order to signal the end-of-stream, we write
        // an empty buffer with the eos bit set.
        mOutput.write(StreamBuffers::from({}, isLast));
    }
}


void CborWriter::writeBytes(const uint8_t* data, const size_t count)
{
    size_t remaining = count;
    const uint8_t* p = data;
    while (remaining > 0)
    {
        const size_t n = std::min(remaining, mBuffer.size() - mUsed);
        std::copy(p, p + n, mBuffer.data() + mUsed);
        mUsed += n;
        p += n;
        remaining -= n;
        if (mUsed == mBuffer.size())
        {
            // Write `mBuffer` out to `mOutput`. After that, `mBuffer` is empty, i.e. mUsed==0.
            flushBuffer();
        }
    }
}


// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
void CborWriter::writeBuffers(StreamBuffers&& buffers)
{
    for (const auto& buffer : buffers)
        writeBytes(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());
}

//NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
void CborWriter::writeUint8(uint8_t value)
{
    mBuffer[mUsed] = value;
    mUsed++;
    if (mUsed == mBuffer.size())
        flushBuffer();
}
//NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

void CborWriter::writeUint16(uint16_t value)
{
    Serialization::uint16::write(value, [this](uint8_t b) { writeUint8(b); });
}


void CborWriter::writeUint32(const uint32_t value)
{
    Serialization::uint32::write(value, [this](uint8_t b) { writeUint8(b); });
}


void CborWriter::writeUint64(const uint64_t value)
{
    Serialization::uint64::write(value, [this](uint8_t b) { writeUint8(b); });
}

} // namespace epa
