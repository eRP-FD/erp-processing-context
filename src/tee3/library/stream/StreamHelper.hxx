/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_STREAMHELPER_HXX
#define EPA_LIBRARY_STREAM_STREAMHELPER_HXX

#include "library/stream/Stream.hxx"
#include "library/util/BinaryBuffer.hxx"

namespace epa
{

std::string to_string(std::streambuf& stream);
std::string to_string(Stream& stream);
std::string to_string(Stream&& stream);

size_t copyStream(Stream& from, Stream& to);

/**
 * Peek beginning of a stream up to a given length, and return that as a string. The given stream is
 * replaced by a concatenated stream that consists of the peeked part plus the original stream from
 * which that part has already been read. This is not so efficient - please use this for debugging
 * (logs) only.
 */
std::string peekStringFromStream(Stream& stream, std::size_t maxLength);


/**
 * Simple structure for holding a readable stream and its exact size. The constructors copy or move
 * the given data. See ReferencingStreamAndSize for other constructors.
 */
struct StreamAndSize
{
    Stream stream;
    std::size_t size;

    StreamAndSize();
    StreamAndSize(Stream&& stream, std::size_t size);
    explicit StreamAndSize(std::string&& string);
    explicit StreamAndSize(const std::string_view& stringView);
    explicit StreamAndSize(const BinaryView& binaryView);
    explicit StreamAndSize(std::vector<StreamAndSize>&& streamsAndSizes);
};


/**
 * A derivation of StreamAndSize that just provides additional constructors. These constructors
 * construct Stream instances which reference the given data.
 *
 * THE REFERENCED DATA MUST BE VALID AND UNCHANGED FOR THE LIFETIME OF THE STREAM!
 */
struct ReferencingStreamAndSize : StreamAndSize
{
    explicit ReferencingStreamAndSize(const BinaryView& binaryView);
    explicit ReferencingStreamAndSize(const std::string_view& stringView);
};

} // namespace epa

#endif
