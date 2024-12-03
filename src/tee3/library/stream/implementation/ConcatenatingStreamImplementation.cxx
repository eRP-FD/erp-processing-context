/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/implementation/ConcatenatingStreamImplementation.hxx"
#include "library/log/Logging.hxx"

#include <cstring>

namespace epa
{

ConcatenatingStreamImplementation::ConcatenatingStreamImplementation(std::vector<Stream> streams)
  : ReadableStreamImplementation(),
    mStreams(std::move(streams)),
    mCurrentStreamIndex(0)
{
}


ConcatenatingStreamImplementation::~ConcatenatingStreamImplementation() = default;


StreamBuffers ConcatenatingStreamImplementation::doRead()
{
    // Forward the read request to the current stream. If that can not provide any more bytes (and returns 0)
    // switch to the next stream.
    while (mCurrentStreamIndex < mStreams.size())
    {
        auto buffers = mStreams[mCurrentStreamIndex].read();
        if (buffers.isLast())
        {
            // Switch to the next stream.
            ++mCurrentStreamIndex;
            if (buffers.isEmpty())
            {
                continue;
            }
            else
            {
                // For all but the last stream, reset the isLast flag so that we get called again for the next stream.
                buffers.setIsLast(mCurrentStreamIndex >= mStreams.size());

                return buffers;
            }
        }
        else
            return buffers;
    }

    // We should not get here, if a) we handle the isLast flag correctly in this method and b) the caller
    // handles this flag correctly.
    return StreamBuffers(StreamBuffer::IsLast);
}
} // namespace epa
