/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_READABLESTREAMIMPLEMENTATIONSTREAMBUF_HXX
#define EPA_LIBRARY_STREAM_READABLESTREAMIMPLEMENTATIONSTREAMBUF_HXX

#include "library/common/Constants.hxx"
#include "library/stream/implementation/StreamImplementation.hxx"

#include <memory>
#include <streambuf>

namespace epa
{

/**
 * A read-only std::streambuf extension that overrides the xsgetn, underflow methods
 * and implements them based on an IStreamImplementation object.
 * Therefore it can be easily be created from any Stream object.
 */
class ReadableStreamImplementationStreamBuf : public std::streambuf
{
public:
    explicit ReadableStreamImplementationStreamBuf(
        std::unique_ptr<StreamImplementation> implementation);

protected:
    std::streamsize xsgetn(char_type* resultBuffer, std::streamsize count) override;
    std::streambuf::int_type underflow() override;

private:
    bool ensureBytesReadable();
    void readNextBuffers();

    std::unique_ptr<StreamImplementation> mImplementation;
    StreamBuffers mReadBuffers;
    StreamBuffers::const_iterator mReadBufferIterator;
};

} // namespace epa

#endif
