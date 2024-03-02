/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/compression/Deflate.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"

#define ZLIB_CONST

#include <zlib.h>
#include <array>
#include <filesystem>
#include <memory>


using namespace std::string_literals;


Deflate::Deflate() = default;

Deflate::~Deflate() = default;


std::string Deflate::compress(std::string_view plain, Compression::DictionaryUse dictUse) const
{
    Expect(dictUse == Compression::DictionaryUse::Undefined, "Unsupported dictionary");
    // Initialize deflation. Let zlib choose alloc and free functions.
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    // Initialize zlib compression for gzip.
    Expect(deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) == Z_OK,
           "could not initialize compression for gzip");

    std::uint8_t buffer[bufferSize];
    stream.avail_in = gsl::narrow<uInt>(plain.size());
    stream.next_in = reinterpret_cast<const unsigned char*>(plain.data());

    // Use Z_FINISH as 'flush' argument for `deflate()` because the whole input is available
    // to `deflate()` and the only reason why we might need to loop more than once, is that the
    // output buffer is not large enough.  In most cases, though, for metadata strings, the
    // buffer should be large enough and one iteration should be enough.
    constexpr auto flush = Z_FINISH;

    int deflateEndResult = Z_OK;
    std::string dataResult{};

    {
        auto inflateFinally = gsl::finally([&stream, &deflateEndResult]() {
            // Release any dynamic memory.
            deflateEndResult = deflateEnd(&stream);
        });
        do
        {
            stream.avail_out = sizeof(buffer);
            stream.next_out = buffer;

            // Deflate the next part of the input. This will either use up all remaining input or fill
            // up the output buffer.
            const int result = deflate(&stream, flush);
            Expect(result >= Z_OK, "compression failed");

            // Append the compressed data to the result string.
            const size_t pendingSize = sizeof(buffer) - stream.avail_out;
            dataResult.append(reinterpret_cast<char*>(buffer), pendingSize);

            // Exit the loop when all input and output has been processed. This avoids one last
            // call to deflate().
            if (result == Z_STREAM_END)
            {
                break;
            }
        } while (stream.avail_out == 0);
    }

    Expect(deflateEndResult == Z_OK, "finishing compression failed");

    return dataResult;
}

std::string Deflate::decompress(std::string_view compressed) const
{
    // Initialize deflation. Let zlib choose alloc and free functions.
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    // Initialize zlib decompression for gzip.
    Expect(inflateInit2(&stream, 16 + MAX_WBITS) == Z_OK, "could not initialize decompression for gzip");

    // Setup `text` as input buffer.
    stream.avail_in = gsl::narrow<uInt>(compressed.size());
    stream.next_in = reinterpret_cast<const unsigned char*>(compressed.data());
    std::array<uint8_t, bufferSize> buffer{};

    int inflateEndResult = Z_OK;
    std::string dataResult{};

    {
        // ensure that inflateEnd() is called also when exceptions are thrown
        auto inflateFinally = gsl::finally([&stream, &inflateEndResult]() {
            inflateEndResult = inflateEnd(&stream);
        });
        do
        {
            // Set up `buffer` as output buffer.
            stream.avail_out = buffer.size();
            stream.next_out = buffer.data();

            // Inflate the next part of the input. This will either use up all remaining input or fill
            // up the output buffer.
            const int result = inflate(&stream, Z_NO_FLUSH);
            Expect(result >= Z_OK, "decompression failed");

            // Not expecting the compression to have used a training dictionary
            Expect(result != Z_NEED_DICT, "Cannot decompress with dict");

            // Append the decompressed data to the result string.
            const size_t decompressedSize = buffer.size() - stream.avail_out;
            dataResult.append(reinterpret_cast<char*>(buffer.data()), decompressedSize);
            Expect3(dataResult.size() <= maxPlainSize, "Uncompressed size too large.", std::logic_error);

            // Exit when all input and output has been processed. This avoids one last
            // call to inflate().
            if (result == Z_STREAM_END)
            {
                break;
            }
        } while (true);
    }

    // Release any dynamic memory.
    Expect(inflateEndResult == Z_OK, "finishing decompression failed");

    return dataResult;
}
