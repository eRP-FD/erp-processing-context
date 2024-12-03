/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/AesGcmStreamCryptor.hxx"
#include "library/crypto/SecureRandomGenerator.hxx"
#include "library/log/Logging.hxx"
#include "library/stream/StreamFactory.hxx"
#include "library/stream/StreamHelper.hxx"
#include "library/stream/implementation/ProcessingStreamImplementation.hxx"
#include "library/util/Assert.hxx"
#include "library/util/ByteHelper.hxx"

#include "shared/crypto/OpenSslHelper.hxx"
#include "shared/util/String.hxx"

namespace epa
{

namespace
{
    void throwOnUnexpectedStatus(
        const int expectedStatus,
        const int status,
        const char* message,
        const std::optional<size_t> size = std::nullopt)
    {
        if (expectedStatus != status)
        {
            LOG(ERROR) << "there was an error while calling an openssl function: " << message;
            if (size)
            {
                LOG(ERROR) << "processed message size at this point is " << (*size);
            }
            showAllOpenSslErrors();
            throw AesGcmStreamCryptor::Exception(message);
        }
    }

    AesGcmStreamCryptor::unique_cipher_context createCipherContext()
    {
        auto context = std::unique_ptr<EVP_CIPHER_CTX, void (*)(EVP_CIPHER_CTX*)>(
            EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
        if (context == nullptr)
            throw AesGcmStreamCryptor::Exception("can't create cipher context for AES-GCM");
        return context;
    }

// Make sure that Assert throws an AesGcmStreamCryptor::Exception
#define AssertCryptor(expression)                                                                  \
    Assert(expression) << assertion::exceptionType<AesGcmStreamCryptor::Exception>()
}


namespace
{
    constexpr size_t randomLength = 32 / 8;
    void copyRandomData(void* dest, const size_t length)
    {
        BinaryBuffer randomBytes = SecureRandomGenerator::generate(length);
        Assert(randomBytes.size() == length);
        String::copyMemorySafely(dest, length, randomBytes.data(), randomBytes.size());
    }
}


// "VAU-Protokoll: Client, verschlüsselte Kommunikation (1)"
// GEMREQ-start A_16945-02
// GEMREQ-start A_17220, A_17221-01
// A_24628
AesGcmStreamCryptor::InitializationVector AesGcmStreamCryptor::createSemiRandomIV(
    const uint64_t messageCounter)
{
    // A_16945-02: IV is 32 random bits || 64 bit big endian unsigned message counter
    // Initialize the iv to hold 96 bits.
    auto iv =
        InitializationVector(static_cast<size_t>(EVP_CIPHER_iv_length(EVP_aes_256_gcm())), ' ');
    Assert(iv.size() == 96 / 8) << assertion::exceptionType<AesGcmStreamCryptor::Exception>()
                                << "invalid IV length";

    // Fill the first 32 bits with random data.
    copyRandomData(iv.data(), randomLength);

    // Fill the remaining 64 bits with the big-endian messageCounter.
    ByteHelper::intToBigEndianBytes(messageCounter, iv.data() + randomLength);

    return iv;
}
// GEMREQ-end A_16945-02


AesGcmStreamCryptor::InitializationVector AesGcmStreamCryptor::createRandomIV()
{
    auto iv =
        InitializationVector(static_cast<size_t>(EVP_CIPHER_iv_length(EVP_aes_256_gcm())), ' ');
    Assert(iv.size() == 96 / 8) << assertion::exceptionType<AesGcmStreamCryptor::Exception>()
                                << "invalid IV length";
    copyRandomData(iv.data(), iv.size());

    return iv;
}


std::string AesGcmStreamCryptor::createRandomKey()
{
    std::string key(static_cast<size_t>(EVP_CIPHER_key_length(EVP_aes_256_gcm())), ' ');
    copyRandomData(key.data(), key.size());
    return key;
}
// GEMREQ-end A_17220, A_17221-01


// GEMREQ-start A_17220, A_17221-01
Stream AesGcmStreamCryptor::makeEncryptingStream(
    const std::function<Key()>& symmetricKeySupplier,
    Stream input,
    uint64_t messageCounter,
    const std::function<InitializationVector(uint64_t)>& ivSupplier,
    const std::function<StreamBuffers()>& aadSupplier)
{
    return Stream(std::make_unique<ProcessingStreamImplementation>(
        std::move(input),
        Encryptor(symmetricKeySupplier, messageCounter, ivSupplier, aadSupplier),
        Encryptor(symmetricKeySupplier, messageCounter, ivSupplier, aadSupplier)));
}


Stream AesGcmStreamCryptor::makeEncryptingStream(
    const std::function<Key()>& symmetricKeySupplier,
    Stream input,
    const std::function<InitializationVector()>& ivSupplier,
    const std::function<StreamBuffers()>& aadSupplier)
{
    return Stream(std::make_unique<ProcessingStreamImplementation>(
        std::move(input),
        Encryptor(symmetricKeySupplier, ivSupplier, aadSupplier),
        Encryptor(symmetricKeySupplier, ivSupplier, aadSupplier)));
}


Stream AesGcmStreamCryptor::makeDecryptingStream(
    const std::function<Key()>& symmetricKeySupplier,
    Stream input,
    const std::function<void(uint64_t)>& messageCounterValidator,
    const std::function<StreamBuffers()>& aadSupplier,
    const std::function<void(std::exception_ptr)>& authenticationTagMismatchHandler)
{
    return Stream(std::make_unique<ProcessingStreamImplementation>(
        std::move(input),
        Decryptor(
            symmetricKeySupplier,
            messageCounterValidator,
            aadSupplier,
            authenticationTagMismatchHandler),
        Decryptor(
            symmetricKeySupplier,
            messageCounterValidator,
            aadSupplier,
            authenticationTagMismatchHandler)));
}
// GEMREQ-end A_17220, A_17221-01


// GEMREQ-start A_17220, A_17221-01, A_15745#encrypt
std::string AesGcmStreamCryptor::encrypt(const std::string& plainText, const Key& key)
{
    return to_string(makeEncryptingStream(
        [key]() { return key; }, StreamFactory::makeReadableStream(plainText)));
}
// GEMREQ-end A_17220, A_17221-01, A_15745#encrypt


std::string AesGcmStreamCryptor::decrypt(const std::string& encryptedText, const Key& key)
{
    return to_string(makeDecryptingStream(
        [key]() { return key; }, StreamFactory::makeReadableStream(encryptedText)));
}


size_t AesGcmStreamCryptor::adjustSizeForEncryption(size_t sizeBeforeEncryption)
{
    return sizeBeforeEncryption + NonceLength + HashLength;
}


size_t AesGcmStreamCryptor::adjustSizeForDecryption(size_t sizeBeforeDecryption)
{
    AssertCryptor(sizeBeforeDecryption > NonceLength + HashLength)
        << "invalid size before decryption";
    return sizeBeforeDecryption - NonceLength - HashLength;
}


// ===== Encryptor ===========================================================

AesGcmStreamCryptor::Encryptor::Encryptor(
    std::function<Key()> symmetricKeySupplier,
    uint64_t messageCounter,
    std::function<InitializationVector(uint64_t)> ivSupplier,
    std::function<StreamBuffers()> aadSupplier)
  : mKeySupplier(std::move(symmetricKeySupplier)),
    mIvSupplier(
        [supplier = std::move(ivSupplier), messageCounter] { return supplier(messageCounter); }),
    mAadSupplier(std::move(aadSupplier)),
    mState(State::NotStarted),
    mCipherContext(createCipherContext()),
    mBlockSize(static_cast<size_t>(EVP_CIPHER_block_size(EVP_aes_256_gcm()))),
    mAccumulatingMessageSize(0)
{
    AssertCryptor(mBlockSize > 0) << "invalid block size";
}


AesGcmStreamCryptor::Encryptor::Encryptor(
    std::function<Key()> symmetricKeySupplier,
    std::function<InitializationVector()> ivSupplier,
    std::function<StreamBuffers()> aadSupplier)
  : mKeySupplier(std::move(symmetricKeySupplier)),
    mIvSupplier(std::move(ivSupplier)),
    mAadSupplier(std::move(aadSupplier)),
    mState(State::NotStarted),
    mCipherContext(createCipherContext()),
    mBlockSize(static_cast<size_t>(EVP_CIPHER_block_size(EVP_aes_256_gcm()))),
    mAccumulatingMessageSize(0)
{
    AssertCryptor(mBlockSize > 0) << "invalid block size";
}


AesGcmStreamCryptor::Encryptor::Encryptor(const Encryptor& other)
  : mKeySupplier(other.mKeySupplier),
    mIvSupplier(other.mIvSupplier),
    mAadSupplier(other.mAadSupplier),
    mState(State::NotStarted),
    mCipherContext(createCipherContext()),
    mBlockSize(static_cast<size_t>(EVP_CIPHER_block_size(EVP_aes_256_gcm()))),
    mAccumulatingMessageSize(0)
{
}


StreamBuffers AesGcmStreamCryptor::Encryptor::operator()(StreamBuffers buffers)
{
    Assert(mState != State::Finished) << "AES-GCM already finished";

    StreamBuffers result(StreamBuffer::NotLast);

    if (mState == State::NotStarted)
    {
        // Initialize the encryption and add a buffer with the IV/nonce.
        result.pushBack(start());
        mState = State::Initialized;

        // Start encryption with the additional authentication data.
        if (mAadSupplier)
        {
            addAdditionalAuthenticationData(mAadSupplier());
            mAadSupplier = {};
        }
    }

    result.pushBack(encryptBuffers(buffers));

    if (buffers.isLast())
    {
        // Finish the encryption and add any remaining data bytes.
        result.pushBack(finalize());
        // Append the hash.
        result.pushBack(getHashBuffer());
        mState = State::Finished;
    }

    Assert((mState == State::Finished) == result.isLast());

    return result;
}


StreamBuffer AesGcmStreamCryptor::Encryptor::start()
{
    auto iv = mIvSupplier();
    EncryptionKey key(mKeySupplier());

    AssertCryptor(static_cast<int>(key.size()) == EVP_CIPHER_key_length(EVP_aes_256_gcm()))
        << "invalid key length, actual: " << std::to_string(key.size())
        << ", expected: " << std::to_string(EVP_CIPHER_key_length(EVP_aes_256_gcm()));
    AssertCryptor(static_cast<int>(iv.size()) == EVP_CIPHER_iv_length(EVP_aes_256_gcm()))
        << "invalid IV length";

    const int status = EVP_EncryptInit_ex(
        mCipherContext.get(),
        EVP_aes_256_gcm(),
        nullptr,
        key.data(),
        reinterpret_cast<const unsigned char*>(iv.data()));
    throwOnUnexpectedStatus(1, status, "can't initialize AES-GCM encryption");

    return StreamBuffer::from(reinterpret_cast<char*>(iv.data()), iv.size(), StreamBuffer::NotLast);
}


// GEMREQ-start A_15745#encryptBuffers
StreamBuffer AesGcmStreamCryptor::Encryptor::encryptBuffers(StreamBuffers& buffers)
{
    mAccumulatingMessageSize += buffers.size();

    // We can use this opportunity to flatten the incoming StreamBuffers object
    // by letting `EVP_EncryptUpdate` write its output for all input `buffers`
    // into a single output buffer.
    const size_t maxOutputSize = buffers.size() + mBlockSize - 1;
    auto outputBuffer = WritableStreamBuffer::createBuffer(maxOutputSize);
    auto* output = reinterpret_cast<unsigned char*>(outputBuffer.writableData());
    size_t outputBufferSize = 0;
    for (const auto& buffer : buffers)
    {
        int count = 0;
        const int status = EVP_EncryptUpdate(
            mCipherContext.get(),
            output,
            &count,
            reinterpret_cast<const unsigned char*>(buffer.data()),
            gsl::narrow<int>(buffer.size()));
        throwOnUnexpectedStatus(1, status, "can't encrypt data");
        output += count;
        outputBufferSize += static_cast<size_t>(count);

        // Paranoia test: we are setting the output buffer size to what is documented for
        // block ciphers in the man page of EVP_EncryptUpdate. Check that OpenSSL honored that
        // and does not have a programming error that leads to a buffer overflow.
        AssertCryptor(outputBufferSize <= maxOutputSize) << "wrote too many bytes on encryption";
    }
    outputBuffer.setSize(outputBufferSize);
    return outputBuffer;
}
// GEMREQ-end A_15745#encryptBuffers


void AesGcmStreamCryptor::Encryptor::addAdditionalAuthenticationData(StreamBuffers buffers)
{
    for (const auto& buffer : buffers)
    {
        int count = 0;
        const int status = EVP_EncryptUpdate(
            mCipherContext.get(),
            nullptr, // This is the signal to OpenSSL, that `buffer` is not to be encrypted
                     // but only added as authentication data.
            &count,
            reinterpret_cast<const unsigned char*>(buffer.data()),
            gsl::narrow<int>(buffer.size()));
        throwOnUnexpectedStatus(1, status, "can't encrypt data");
        Assert(count == gsl::narrow<int>(buffer.size()));
    }
}


StreamBuffer AesGcmStreamCryptor::Encryptor::finalize()
{
    const size_t maxOutputSize = mBlockSize;
    auto outputBuffer = WritableStreamBuffer::createBuffer(maxOutputSize);
    int outputSize = 0;

    const int status = EVP_EncryptFinal_ex(
        mCipherContext.get(),
        reinterpret_cast<unsigned char*>(outputBuffer.writableData()),
        &outputSize);
    throwOnUnexpectedStatus(1, status, "can't finalize AES-GCM encryption");
    outputBuffer.setSize(gsl::narrow<size_t>(outputSize));

    return outputBuffer;
}


StreamBuffer AesGcmStreamCryptor::Encryptor::getHashBuffer()
{
    char hash[HashLength];
    const int status =
        EVP_CIPHER_CTX_ctrl(mCipherContext.get(), EVP_CTRL_AEAD_GET_TAG, HashLength, hash);
    throwOnUnexpectedStatus(1, status, "can't extract AES-GCM hash");

    return StreamBuffer::from(hash, HashLength, StreamBuffer::IsLast);
}


// ===== Decryptor ===========================================================

AesGcmStreamCryptor::Decryptor::Decryptor(
    std::function<Key()> symmetricKeySupplier,
    std::function<void(uint64_t)> messageCounterValidator,
    std::function<StreamBuffers()> aadSupplier,
    std::function<void(std::exception_ptr)> authenticationTagMismatchHandler)
  : mKeySupplier(std::move(symmetricKeySupplier)),
    mAadSupplier(std::move(aadSupplier)),
    mAuthenticationTagMismatchHandler(std::move(authenticationTagMismatchHandler)),
    mState(State::NotStarted),
    mCipherContext(createCipherContext()),
    mPendingBuffers(StreamBuffer::NotLast),
    mBlockSize(static_cast<size_t>(EVP_CIPHER_block_size(EVP_aes_256_gcm()))),
    mMessageCounterValidator(std::move(messageCounterValidator)),
    mAccumulatingMessageSize(0)
{
}


AesGcmStreamCryptor::Decryptor::Decryptor(const Decryptor& other)
  : mKeySupplier(other.mKeySupplier),
    mAadSupplier(other.mAadSupplier),
    mAuthenticationTagMismatchHandler(other.mAuthenticationTagMismatchHandler),
    mState(State::NotStarted),
    mCipherContext(createCipherContext()),
    mPendingBuffers(StreamBuffer::NotLast),
    mBlockSize(static_cast<size_t>(EVP_CIPHER_block_size(EVP_aes_256_gcm()))),
    mMessageCounterValidator(other.mMessageCounterValidator),
    mAccumulatingMessageSize(0)
{
    Assert(other.mPendingBuffers.size() == 0)
        << "can not copy a Decryptor object with pending buffers";
}

//NOLINTBEGIN(readability-function-cognitive-complexity)
StreamBuffers AesGcmStreamCryptor::Decryptor::operator()(StreamBuffers buffers)
{
    if (mState == State::Finished)
    {
        // Allow empty buffers after decryption has already finished.
        if (buffers.isEmpty())
            return buffers;
        else
            throw AesGcmStreamCryptor::Exception("AES-GCM already finished");
    }

    mPendingBuffers.pushBack(std::move(buffers));

    if (mState == State::NotStarted)
    {
        // We need at least 12 bytes to extract the IV/nonce.
        if (mPendingBuffers.size() < NonceLength)
        {
            if (mPendingBuffers.isLast())
                throw AesGcmStreamCryptor::Exception(
                    "stream too short, does not even contain the IV");
            else
                return StreamBuffers(StreamBuffer::NotLast);
        }

        // Extract IV/nonce from the stream and pass it to the AES-GCM initialization.
        auto messageBuffers = mPendingBuffers.splitOff(NonceLength);

        AssertCryptor(mPendingBuffers.size() == NonceLength) << "IV extraction failed";
        start(mPendingBuffers);

        // Continue with what is in `messageBuffers`.
        mPendingBuffers = std::move(messageBuffers);

        mState = State::Initialized;

        if (mAadSupplier)
        {
            addAdditionalAuthenticationData(mAadSupplier());
            mAadSupplier = {};
        }
    }

    // Make sure that we don't read past the start of the authentication tag. For that we always
    // keep the last 16 bytes that we have seen so far in reserve.
    auto [messageBuffers, reserve] = splitOffHash(std::move(mPendingBuffers));

    auto decryptedBuffers = StreamBuffers::from(decryptBuffers(messageBuffers));

    if (reserve.isLast())
    {
        // We got to the end of the stream and can ask openssl to verify the stream for us.
        std::string hash = reserve.toString();

        try
        {
            decryptedBuffers.pushBack(finalizeAndVerify(hash));
        }
        catch (const std::runtime_error&)
        {
            // There are use cases, like the TEE protocol, where errors regarding a mismatch
            // of the authentication tag have to be reported with specialized exceptions.
            if (mAuthenticationTagMismatchHandler)
                mAuthenticationTagMismatchHandler(std::current_exception());
            else
                throw;
        }

        decryptedBuffers.setIsLast(true);
        mState = State::Finished;
    }
    else
    {
        // Keep the buffers that may contain some or all of the hash bytes for the next invocation.
        mPendingBuffers = std::move(reserve);
    }

    return decryptedBuffers;
}
//NOLINTEND(readability-function-cognitive-complexity)


void AesGcmStreamCryptor::Decryptor::start(const StreamBuffers& ivBuffers)
{
    const EncryptionKey key(mKeySupplier());
    const std::string iv = ivBuffers.toString();

    AssertCryptor(static_cast<int>(key.size()) == EVP_CIPHER_key_length(EVP_aes_256_gcm()))
        << "wrong key size for AES-256-GCM";
    AssertCryptor(static_cast<int>(iv.size()) == EVP_CIPHER_iv_length(EVP_aes_256_gcm()))
        << "wrong IV size for AES-256-GCM";

    if (mMessageCounterValidator)
    {
        // Interpret the last 8 bytes == 64 bits of the IV as message counter and validate its value.
        // This is required by A_16945-02.
        // The first 32 bits are randomly chosen and can not be validated.
        mMessageCounterValidator(
            ByteHelper::intFromBigEndianBytes<uint64_t>(iv.data() + randomLength));
    }

    const int status = EVP_DecryptInit_ex(
        mCipherContext.get(),
        EVP_aes_256_gcm(),
        nullptr,
        key.data(),
        reinterpret_cast<const unsigned char*>(iv.data()));
    throwOnUnexpectedStatus(1, status, "can't initialize AES-GCM decryption");
}


StreamBuffer AesGcmStreamCryptor::Decryptor::decryptBuffers(const StreamBuffers& buffers)
{
    mAccumulatingMessageSize += buffers.size();

    // We can use this opportunity to flatten the incoming StreamBuffers object
    // by letting `EVP_DecryptUpdate` write its output for all input `buffers`
    // into a single output buffer.
    const size_t maxOutputSize = buffers.size() + mBlockSize;
    auto outputBuffer = WritableStreamBuffer::createBuffer(maxOutputSize);
    auto* output = reinterpret_cast<unsigned char*>(outputBuffer.writableData());
    size_t outputBufferSize = 0;

    for (const auto& buffer : buffers)
    {
        int bytesWritten = 0;
        const int status = EVP_DecryptUpdate(
            mCipherContext.get(),
            output,
            &bytesWritten,
            reinterpret_cast<const unsigned char*>(buffer.data()),
            gsl::narrow<int>(buffer.size()));
        throwOnUnexpectedStatus(1, status, "can't decrypt data");
        output += bytesWritten;
        outputBufferSize += static_cast<size_t>(bytesWritten);

        // Paranoia test: we are setting the output buffer size to what is documented for
        // block ciphers in the man page of EVP_DecryptUpdate. Check that OpenSSL honored that
        // and does not have a programming error that leads to a buffer overflow.
        AssertCryptor(outputBufferSize <= maxOutputSize) << "wrote too many bytes on decryption";
    }
    outputBuffer.setSize(outputBufferSize);
    return outputBuffer;
}


void AesGcmStreamCryptor::Decryptor::addAdditionalAuthenticationData(StreamBuffers buffers)
{
    for (const auto& buffer : buffers)
    {
        int count = 0;
        const int status = EVP_DecryptUpdate(
            mCipherContext.get(),
            nullptr, // This is the signal to OpenSSL, that `buffer` is not to be encrypted
                     // but only added as authentication data.
            &count,
            reinterpret_cast<const unsigned char*>(buffer.data()),
            gsl::narrow<int>(buffer.size()));
        throwOnUnexpectedStatus(1, status, "can't encrypt data");
        Assert(count == gsl::narrow<int>(buffer.size()));
    }
}


StreamBuffer AesGcmStreamCryptor::Decryptor::finalizeAndVerify(std::string& hash)
{
    AssertCryptor(hash.size() == HashLength) << "invalid hash size";

    {
        const int status = EVP_CIPHER_CTX_ctrl(
            mCipherContext.get(), EVP_CTRL_GCM_SET_TAG, HashLength, hash.data());
        throwOnUnexpectedStatus(1, status, "can't verify decrypted data");
    }

    {
        const size_t maxOutputSize = mBlockSize;
        auto outputBuffer = WritableStreamBuffer::createBuffer(maxOutputSize);
        int outputSize = 0;

        const int status = EVP_DecryptFinal_ex(
            mCipherContext.get(),
            reinterpret_cast<unsigned char*>(outputBuffer.writableData()),
            &outputSize);
        throwOnUnexpectedStatus(
            1, status, "can't finalize AES-GCM decryption", mAccumulatingMessageSize);

        AssertCryptor(outputSize <= static_cast<int>(outputBuffer.capacity()))
            << "wrote too many bytes on decryption";
        outputBuffer.setSize(static_cast<size_t>(outputSize));
        return outputBuffer;
    }
}


std::tuple<StreamBuffers, StreamBuffers> AesGcmStreamCryptor::Decryptor::splitOffHash(
    StreamBuffers buffers)
{
    StreamBuffers messageBuffers(StreamBuffer::NotLast);
    StreamBuffers hashBuffers(StreamBuffer::NotLast);
    if (buffers.isLast())
    {
        AssertCryptor(buffers.size() >= HashLength) << "not enough data to split hash off";
        hashBuffers = buffers.splitOff(buffers.size() - HashLength);
        messageBuffers = std::move(buffers);
    }
    else
    {
        // We don't know how many bytes at the end of `buffers` belong to the hash, if any.
        // Therefore, we make sure that we keep at least 16 bytes in the `hashBuffers`. There is no
        // need to keep *exactly* 16 bytes, instead we round at the nearest buffer border.
        const size_t buffersLength = buffers.size();
        size_t count = 0;
        for (auto& buffer : buffers)
        {
            const size_t bufferSize = buffer.size();
            if (count + buffer.size() + HashLength <= buffersLength)
                messageBuffers.pushBack(std::move(buffer));
            else
                hashBuffers.pushBack(std::move(buffer));
            count += bufferSize;
        }
    }

    return std::make_tuple(std::move(messageBuffers), std::move(hashBuffers));
}


// ===== AesGcmStreamCryptor::Exception ======================================

AesGcmStreamCryptor::Exception::Exception(const std::string& what)
  : std::runtime_error(what)
{
}

} // namespace epa
