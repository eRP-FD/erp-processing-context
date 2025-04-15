/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/StreamHelper.hxx"
#include "library/stream/Stream.hxx"
#include "library/stream/StreamFactory.hxx"

#include <streambuf>
#include <utility> // for std::move

namespace epa
{

std::string to_string(std::streambuf& stream)
{
    std::string s;
    char buffer[1024];
    while (true)
    {
        const auto count = gsl::narrow<size_t>(stream.sgetn(buffer, sizeof(buffer)));
        if (count == 0)
            break;
        s.append(buffer, count);
    }
    return s;
}


std::string to_string(Stream& stream)
{
    return stream.readAll().toString();
}


std::string to_string(Stream&& stream)
{
    Stream s = std::move(stream);
    return to_string(s);
}


size_t copyStream(Stream& from, Stream& to)
{
    size_t totalLength = 0;
    while (true)
    {
        StreamBuffers buffers = from.read();
        totalLength += buffers.size();
        const bool isLast = buffers.isLast();
        to.write(std::move(buffers));
        if (isLast)
            break;
    }
    to.notifyEnd();
    return totalLength;
}


std::string peekStringFromStream(Stream& stream, std::size_t maxLength)
{
    if (maxLength == 0)
        return std::string();

    std::string string;
    std::vector<Stream> streams;
    bool foundLast = false;

    while (string.size() < maxLength && ! foundLast)
    {
        StreamBuffers streamBuffers = stream.readAtMost(maxLength - string.size());
        string += streamBuffers.toString();
        foundLast = streamBuffers.isLast();
        streams.emplace_back(StreamFactory::makeReadableStream(std::move(streamBuffers)));
    }

    if (! foundLast)
        streams.emplace_back(std::move(stream));

    stream = StreamFactory::makeReadableStream(std::move(streams));
    return string;
}


StreamAndSize::StreamAndSize()
  : stream(),
    size(0)
{
}


StreamAndSize::StreamAndSize(Stream&& stream, std::size_t size)
  : stream(std::move(stream)),
    size(size)
{
}


StreamAndSize::StreamAndSize(std::string&& string)
  : size(string.size())
{
    stream = StreamFactory::makeReadableStream(std::move(string));
}


StreamAndSize::StreamAndSize(const std::string_view& stringView)
  : StreamAndSize(StreamFactory::makeReadableStream(stringView), stringView.size())
{
}


StreamAndSize::StreamAndSize(const BinaryView& binaryView)
  : StreamAndSize(
        StreamFactory::makeReadableStream(binaryView.data(), binaryView.size()),
        binaryView.size())
{
}


StreamAndSize::StreamAndSize(std::vector<StreamAndSize>&& streamsAndSizes)
{
    std::vector<Stream> streams;
    streams.reserve(streamsAndSizes.size());
    std::size_t totalSize = 0;
    for (auto&& streamAndSize : std::move(streamsAndSizes))
    {
        streams.emplace_back(std::move(streamAndSize.stream));
        totalSize += streamAndSize.size;
    }

    stream = StreamFactory::makeReadableStream(std::move(streams));
    size = totalSize;
}


ReferencingStreamAndSize::ReferencingStreamAndSize(const BinaryView& binaryView)
  : StreamAndSize(
        StreamFactory::makeReadableStream(StreamBuffers::from(StreamBuffer::fromReferencing(
            reinterpret_cast<const char*>(binaryView.data()),
            binaryView.size(),
            true))),
        binaryView.size())
{
}


ReferencingStreamAndSize::ReferencingStreamAndSize(const std::string_view& stringView)
  : StreamAndSize(
        StreamFactory::makeReadableStream(StreamBuffers::from(
            StreamBuffer::fromReferencing(stringView.data(), stringView.size(), true))),
        stringView.size())
{
}

} // namespace epa
