/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_IMPLEMENTATION_CONCATENATINGSTREAMIMPLEMENTATION_HXX
#define EPA_LIBRARY_STREAM_IMPLEMENTATION_CONCATENATINGSTREAMIMPLEMENTATION_HXX

#include "library/stream/Stream.hxx"
#include "library/stream/implementation/StreamImplementation.hxx"

#include <vector>

namespace epa
{

/**
 * Custom std::streambuf that concatenates a list of streams.
 */
class ConcatenatingStreamImplementation : public ReadableStreamImplementation,
                                          private boost::noncopyable
{
public:
    explicit ConcatenatingStreamImplementation(std::vector<Stream> streams);
    ~ConcatenatingStreamImplementation() override;

protected:
    StreamBuffers doRead() override;

private:
    std::vector<Stream> mStreams;
    size_t mCurrentStreamIndex;
};

} // namespace epa

#endif
