/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_IMPLEMENTATION_PROCESSINGSTREAMIMPLEMENTATION_HXX
#define EPA_LIBRARY_STREAM_IMPLEMENTATION_PROCESSINGSTREAMIMPLEMENTATION_HXX

#include "library/stream/Stream.hxx"
#include "library/stream/implementation/StreamImplementation.hxx"

#include <cstring>
#include <functional>

namespace epa
{

class ProcessingStreamImplementation : public StreamImplementation
{
public:
    /**
     * The processing function has a result type that differs from other definitions of similar functions.
     * That is because a result of 0 does not signal the end of the stream but rather allows the processing
     * function to buffer some of the incoming data.
     * Therefore we have these return value sets:
     * - 1 .. n    Number of used bytes in buffer that may be smaller than, equal to or higher than used.
     * - 0         The processing function has buffered all of its incoming data.
     * - -1        Stream end.
     * - Other negative values are not used at the moment.
     */
    using ProcessingFunction = std::function<StreamBuffers(StreamBuffers buffers)>;

    ProcessingStreamImplementation(
        Stream stream,
        ProcessingFunction&& readFunction,
        ProcessingFunction&& writeFunction);
    ~ProcessingStreamImplementation() override = default;

    bool isReadSupported() const override;
    bool isWriteSupported() const override;

    void notifyEnd() override;

protected:
    StreamBuffers doRead() override;
    void doWrite(StreamBuffers&& buffers) override;

private:
    Stream mStream;
    ProcessingFunction mReadFunction;
    ProcessingFunction mWriteFunction;
};

} // namespace epa

#endif
