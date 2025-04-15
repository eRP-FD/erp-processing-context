/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/implementation/FunctionStreamImplementation.hxx"

#include <stdexcept>

namespace epa
{

FunctionStreamImplementation::FunctionStreamImplementation(
    ReadFunction readFunction,
    WriteFunction writeFunction,
    NotifyEndFunction notifyEndFunction)
  : StreamImplementation(),
    mReadFunction(std::move(readFunction)),
    mWriteFunction(std::move(writeFunction)),
    mNotifyEndFunction(std::move(notifyEndFunction))
{
}


void FunctionStreamImplementation::doWrite(StreamBuffers&& buffers)
{
    if (! mWriteFunction)
        throw std::logic_error("write is not supported");

    mWriteFunction(std::move(buffers));
}


StreamBuffers FunctionStreamImplementation::doRead()
{
    if (! mReadFunction)
        throw std::logic_error("read is not supported");

    return mReadFunction();
}


bool FunctionStreamImplementation::isWriteSupported() const
{
    return mWriteFunction != nullptr;
}


bool FunctionStreamImplementation::isReadSupported() const
{
    return mReadFunction != nullptr;
}


void FunctionStreamImplementation::notifyEnd()
{
    if (isWriteSupported())
    {
        if (mNotifyEndFunction != nullptr)
            mNotifyEndFunction();
        else
            mWriteFunction(StreamBuffers(StreamBuffer::IsLast));
    }
}

} // namespace epa
