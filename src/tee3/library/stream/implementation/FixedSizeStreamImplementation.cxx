/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/implementation/FixedSizeStreamImplementation.hxx"
#include "library/log/Logging.hxx"
#include "library/util/Assert.hxx"

namespace epa
{

FixedSizeStreamImplementation::FixedSizeStreamImplementation(
    const char* buffer,
    const size_t bufferSize)
  : FixedSizeStreamImplementation(StreamBuffers::from(buffer, bufferSize, StreamBuffer::IsLast))
{
    Assert(mBuffers.size() == bufferSize);
}


FixedSizeStreamImplementation::FixedSizeStreamImplementation(StreamBuffers buffers)
  : ReadableStreamImplementation(),
    mBuffers(std::move(buffers))
{
    mBuffers.setIsLast(true);
}


FixedSizeStreamImplementation::~FixedSizeStreamImplementation() = default;


StreamBuffers FixedSizeStreamImplementation::doRead()
{
    StreamBuffers buffers(true);
    std::swap(buffers, mBuffers);
    return buffers;
}

} // namespace epa
