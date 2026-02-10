/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/StreamFactory.hxx"
#include "library/crypto/AesGcmStreamCryptor.hxx"
#include "library/crypto/tee/TeeConstants.hxx"
#include "library/crypto/tee/TeeContext.hxx"
#include "library/crypto/tee/TeeSessionContext.hxx"
#include "library/log/Logging.hxx"
#include "library/stream/implementation/ConcatenatingStreamImplementation.hxx"
#include "library/stream/implementation/FixedSizeStreamImplementation.hxx"
#include "library/stream/implementation/FunctionStreamImplementation.hxx"
#include "library/stream/implementation/ProcessingStreamImplementation.hxx"
#include "library/util/Assert.hxx"
#include "library/util/ByteHelper.hxx"

#include <cstring>

#include "shared/util/Base64.hxx"
#include <botan/hash.h>
#include <botan/hex.h>

namespace epa
{

Stream StreamFactory::makeReadableStream()
{
    return makeReadableStream("");
}


Stream StreamFactory::makeReadableStream(const char* data, size_t length)
{
    return Stream(std::make_unique<FixedSizeStreamImplementation>(data, length));
}


Stream StreamFactory::makeReadableStream(const unsigned char* data, size_t length)
{
    return Stream(std::make_unique<FixedSizeStreamImplementation>(
        reinterpret_cast<const char*>(data), length));
}


Stream StreamFactory::makeReadableStream(const std::string_view& data)
{
    return makeReadableStream(data.data(), data.size());
}


Stream StreamFactory::makeReadableStream(const char* nullTerminatedData)
{
    Assert(nullTerminatedData != nullptr);
    return makeReadableStream(std::string(nullTerminatedData));
}


Stream StreamFactory::makeReadableStream(std::string&& data)
{
    return Stream(std::make_unique<FixedSizeStreamImplementation>(
        StreamBuffers::from(std::move(data), StreamBuffer::IsLast)));
}


Stream StreamFactory::makeReadableStream(std::vector<Stream>&& streams)
{
    if (streams.empty())
        return makeReadableStream();
    else if (streams.size() == 1)
        return std::move(streams.at(0));
    else
        return Stream(std::make_unique<ConcatenatingStreamImplementation>(std::move(streams)));
}


Stream StreamFactory::makeReadableStream(StreamBuffers buffers)
{
    return Stream(std::make_unique<FixedSizeStreamImplementation>(std::move(buffers)));
}


Stream StreamFactory::makeWritableStream(
    std::function<void(StreamBuffers&&)>&& writeFunction,
    std::function<void()> notifyEndFunction)
{
    return Stream(std::make_unique<FunctionStreamImplementation>(
        FunctionStreamImplementation::ReadFunction(),
        std::move(writeFunction),
        std::move(notifyEndFunction)));
}


Stream StreamFactory::makeWritableStream(std::streambuf& streambuf)
{
    return makeWritableStream([&streambuf](StreamBuffers buffers) mutable -> void {
        for (const auto& buffer : buffers)
            streambuf.sputn(buffer.data(), gsl::narrow<std::streamsize>(buffer.size()));
    });
}


Stream StreamFactory::makeWritableStream(std::ostream& stream)
{
    return makeWritableStream(*stream.rdbuf());
}


Stream StreamFactory::Crypto::makeAesGcmEncryptingStream(
    const std::function<EncryptionKey()>& symmetricKeySupplier,
    Stream input,
    const uint64_t messageCounter)
{
    return AesGcmStreamCryptor::makeEncryptingStream(
        symmetricKeySupplier, std::move(input), messageCounter);
}


Stream StreamFactory::Crypto::makeAesGcmDecryptingStream(
    const std::function<EncryptionKey()>& symmetricKeySupplier,
    Stream input,
    const std::function<void(uint64_t)>& messageCounterValidator)
{
    return AesGcmStreamCryptor::makeDecryptingStream(
        symmetricKeySupplier, std::move(input), messageCounterValidator);
}


// GEMREQ-start A_15745#makeDocumentEncryptingStream
Stream StreamFactory::Crypto::makeDocumentEncryptingStream(const EncryptionKey& key, Stream input)
{
    return AesGcmStreamCryptor::makeEncryptingStream(
        [key]() -> AesGcmStreamCryptor::Key { return key; }, std::move(input));
}
// GEMREQ-end A_15745#makeDocumentEncryptingStream


Stream StreamFactory::Crypto::makeDocumentDecryptingStream(const EncryptionKey& key, Stream input)
{
    return AesGcmStreamCryptor::makeDecryptingStream(
        [key]() -> AesGcmStreamCryptor::Key { return key; }, std::move(input));
}


// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks): False positive observed with gcc 9 STL.
Stream StreamFactory::Base64::makeEncodingStream(Stream input)
{
    auto encodingFunction = [rest = std::make_shared<StreamBuffers>(StreamBuffer::NotLast)](
                                StreamBuffers buffers) mutable -> StreamBuffers {
        StreamBuffers result(StreamBuffer::NotLast);

        rest->pushBack(std::move(buffers));
        if (rest->isLast())
        {
            result = StreamBuffers::from(
                StreamBuffer::from(::Base64::encode(rest->toString()), StreamBuffer::IsLast));
        }
        else
        {
            auto newRest = rest->splitOff(rest->size() - (rest->size() % 3));
            result = StreamBuffers::from(
                StreamBuffer::from(::Base64::encode(rest->toString()), StreamBuffer::NotLast));
            *rest = std::move(newRest);
        }
        return result;
    };
    return Stream(std::make_unique<ProcessingStreamImplementation>(
        std::move(input), encodingFunction, encodingFunction));
}


Stream StreamFactory::Base64::makeDecodingStream(Stream input)
{
    auto decodingFunction = [rest = std::make_shared<StreamBuffers>(StreamBuffer::NotLast)](
                                StreamBuffers buffers) mutable -> StreamBuffers {
        StreamBuffers result(StreamBuffer::NotLast);

        rest->pushBack(std::move(buffers));
        if (rest->isLast())
        {
            result = StreamBuffers::from(StreamBuffer::from(
                ::Base64::decodeToString(rest->toString()), StreamBuffer::IsLast));
        }
        else
        {
            auto newRest = rest->splitOff(rest->size() - (rest->size() % 4));
            result = StreamBuffers::from(StreamBuffer::from(
                ::Base64::decodeToString(rest->toString()), StreamBuffer::NotLast));
            *rest = std::move(newRest);
        }
        return result;
    };
    return Stream(std::make_unique<ProcessingStreamImplementation>(
        std::move(input), decodingFunction, decodingFunction));
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)


namespace
{
    Stream makeHashingStream(
        Stream input,
        std::string_view algorithm,
        std::function<void(const std::string&)> hashHandlingFunction)
    {
        const auto hash = std::shared_ptr<Botan::HashFunction>(Botan::HashFunction::create_or_throw(algorithm));

        auto hashfunction = [hash = hash,
                             hashHandlingFunction = std::move(hashHandlingFunction)](
                                StreamBuffers buffers) mutable -> StreamBuffers {

            for (const auto& buffer : buffers)
            {
                hash->update(
                    std::span{reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size()});
            }

            if (buffers.isLast())
            {
                const std::string encodedDigest = Botan::hex_encode(hash->final(), false);
                hashHandlingFunction(encodedDigest);
            }

            return buffers;
        };
        return Stream(
            std::make_unique<ProcessingStreamImplementation>(
                std::move(input), hashfunction, hashfunction));
    }
}


Stream StreamFactory::Hash::makeSha1CalculatingStream(
    Stream input,
    std::function<void(const std::string&)> sha1HandlingFunction)
{
    return makeHashingStream(std::move(input), "SHA-1", std::move(sha1HandlingFunction));
}


Stream StreamFactory::Hash::makeSha256CalculatingStream(
    Stream input,
    std::function<void(const std::string&)> sha256HandlingFunction)
{
    return makeHashingStream(std::move(input), "SHA-256", std::move(sha256HandlingFunction));
}


Stream StreamFactory::SizeMeasuring::makeSizeMeasuringStream(
    Stream input,
    std::function<void(size_t)> sizeHandlingFunction)
{
    auto sizeFunction = [wrappedLength = size_t(0),
                         sizeHandlingFunction = std::move(sizeHandlingFunction)](
                            StreamBuffers buffers) mutable -> StreamBuffers {
        wrappedLength += buffers.size();

        if (buffers.isLast())
        {
            sizeHandlingFunction(wrappedLength);
        }

        return buffers;
    };
    return Stream(std::make_unique<ProcessingStreamImplementation>(
        std::move(input), sizeFunction, sizeFunction));
}


Stream StreamFactory::Log::makeLoggingStream(const std::string& prefix, Stream input)
{
    auto loggingFunction = [prefix,
                            totalSize = size_t(0)](StreamBuffers buffers) mutable -> StreamBuffers {
        for (const auto& buffer : buffers)
        {
            LOG(DEBUG1) << prefix << " [" << logging::development(buffer.toString()) << "]";
            totalSize += buffer.size();
        }
        LOG(DEBUG1) << prefix << "   size so far is " << totalSize;
        return buffers;
    };
    return Stream(std::make_unique<ProcessingStreamImplementation>(
        std::move(input), loggingFunction, loggingFunction));
}


Stream StreamFactory::Log::makeLoggingBinaryStream(const std::string& prefix, Stream input)
{
    auto loggingFunction = [prefix,
                            totalSize = size_t(0)](StreamBuffers buffers) mutable -> StreamBuffers {
        for (const auto& buffer : buffers)
        {
            LOG(DEBUG1) << prefix << " <"
                        << logging::development(ByteHelper::toHex(buffer.toString())) << ">";
            totalSize += buffer.size();
        }
        LOG(DEBUG1) << prefix << "   size so far is " << totalSize;
        return buffers;
    };
    return Stream(std::make_unique<ProcessingStreamImplementation>(
        std::move(input), loggingFunction, loggingFunction));
}

} // namespace epa
