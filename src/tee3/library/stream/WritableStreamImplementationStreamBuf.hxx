/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_WRITABLESTREAMIMPLEMENTATIONSTREAMBUF_HXX
#define EPA_LIBRARY_STREAM_WRITABLESTREAMIMPLEMENTATIONSTREAMBUF_HXX

#include "library/common/Constants.hxx"
#include "library/stream/implementation/StreamImplementation.hxx"

#include <memory>
#include <streambuf>

namespace epa
{

/**
 * A write-only std::streambuf extension that overides the xsputn, overflow methods
 * and implements them based on an IStreamImplementation object.
 * Therefore it can be easily be created from any Stream object.
 */
class WritableStreamImplementationStreamBuf : public std::streambuf
{
public:
    explicit WritableStreamImplementationStreamBuf(
        std::unique_ptr<StreamImplementation> implementation);

protected:
    std::streamsize xsputn(const char* buffer, std::streamsize length) override;
    int overflow(int c) override;
    int sync() override;

private:
    bool writeBuffer();

    char mBuffer[Constants::DefaultBufferSize];
    std::unique_ptr<StreamImplementation> mImplementation;
};

} // namespace epa

#endif
