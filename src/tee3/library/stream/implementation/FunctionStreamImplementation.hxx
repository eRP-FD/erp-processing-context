/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_IMPLEMENTATION_FUNCTIONSTREAMIMPLEMENTATION_HXX
#define EPA_LIBRARY_STREAM_IMPLEMENTATION_FUNCTIONSTREAMIMPLEMENTATION_HXX

#include "library/stream/implementation/StreamImplementation.hxx"

#include <functional>

namespace epa
{

class FunctionStreamImplementation : public StreamImplementation
{
public:
    using ReadFunction = std::function<StreamBuffers()>;
    using WriteFunction = std::function<void(StreamBuffers&&)>;
    using NotifyEndFunction = std::function<void()>;

    FunctionStreamImplementation(
        ReadFunction readFunction,
        WriteFunction writeFunction,
        NotifyEndFunction notifyEndFunction);
    ~FunctionStreamImplementation() override = default;

    bool isReadSupported() const override;
    bool isWriteSupported() const override;

protected:
    StreamBuffers doRead() override;
    void doWrite(StreamBuffers&&) override;

    void notifyEnd() override;

private:
    ReadFunction mReadFunction;
    WriteFunction mWriteFunction;
    NotifyEndFunction mNotifyEndFunction;
};
} // namespace epa

#endif
