/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/implementation/ProcessingStreamImplementation.hxx"
#include "library/util/Assert.hxx"

#include <stdexcept>

namespace epa
{

ProcessingStreamImplementation::ProcessingStreamImplementation(
    Stream stream,
    ProcessingFunction&& readFunction,
    ProcessingFunction&& writeFunction)
  : StreamImplementation(),
    mStream(std::move(stream)),
    mReadFunction(std::move(readFunction)),
    mWriteFunction(std::move(writeFunction))
{
}


void ProcessingStreamImplementation::doWrite(StreamBuffers&& buffers)
{
    if (! mWriteFunction)
        throw std::logic_error("write is not supported");

    // While writing, mStream is our output stream.

    // 1. Pass the data through the processing function.
    StreamBuffers transformedBuffers = mWriteFunction(std::move(buffers));

    // 2. Give the processed data to the output stream.
    //    avoid unnecessary writings
    if (! transformedBuffers.isEmpty() || transformedBuffers.isLast())
    {
        mStream.write(std::move(transformedBuffers));
    }
}


StreamBuffers ProcessingStreamImplementation::doRead()
{
    if (! mReadFunction)
        throw std::logic_error("read is not supported");

    // While reading, mStream is our input stream.

    // This is the first attempt to detect a bug in the Stream processing framework that
    // would lead to an infinite loop where `mStream.read()` would return empty buffers.
    constexpr size_t maxTryCount = 1024;
    for (size_t index = 0; index < maxTryCount; ++index)
    {
        // 1. Read some or all that data from mStream.
        StreamBuffers buffers = mStream.read();

        // 2. Transform the data with the read transform function. Then return the result.
        StreamBuffers transformedBuffers = mReadFunction(std::move(buffers));

        if (transformedBuffers.isLast() || ! transformedBuffers.isEmpty())
            return transformedBuffers;
        else
            continue;
    }

    Failure() << "could not read non-empty buffer in " << maxTryCount << " attempts";
}


bool ProcessingStreamImplementation::isWriteSupported() const
{
    return mWriteFunction != nullptr;
}


bool ProcessingStreamImplementation::isReadSupported() const
{
    return mReadFunction != nullptr;
}


void ProcessingStreamImplementation::notifyEnd()
{
    StreamImplementation::notifyEnd();
    mStream.notifyEnd();
}

} // namespace epa
