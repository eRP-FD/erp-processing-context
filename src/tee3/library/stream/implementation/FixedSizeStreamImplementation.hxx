/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_IMPLEMENTATION_FIXEDSIZESTREAMIMPLEMENTATION_HXX
#define EPA_LIBRARY_STREAM_IMPLEMENTATION_FIXEDSIZESTREAMIMPLEMENTATION_HXX

#include "library/stream/implementation/StreamImplementation.hxx"

#include <functional>
#include <ostream>
#include <vector>

#include "fhirtools/util/Gsl.hxx"

namespace epa
{
/**
 * Custom StreamImplementation for a fixed size buffer that is known at construction time.
 */
class FixedSizeStreamImplementation : public ReadableStreamImplementation
{
public:
    FixedSizeStreamImplementation(const char* buffer, size_t bufferSize);
    explicit FixedSizeStreamImplementation(StreamBuffers buffers);
    ~FixedSizeStreamImplementation() override;

protected:
    StreamBuffers doRead() override;

private:
    StreamBuffers mBuffers;
};

} // namespace epa

#endif
