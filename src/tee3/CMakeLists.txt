#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
# non-exclusively licensed to gematik GmbH
#

find_package(Boost REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(botan REQUIRED)
find_package(date REQUIRED)
find_package(glog REQUIRED)
find_package(gsl-lite REQUIRED)

add_library(tee3 STATIC
    library/client/Error.cxx
    library/common/BoostBeastHeaderConverter.cxx
    library/crypto/AesGcmStreamCryptor.cxx
    library/crypto/AuthorizationAssertion.cxx
    library/crypto/DiffieHellman.cxx
    library/crypto/EllipticCurve.cxx
    library/crypto/EllipticCurveKey.cxx
    library/crypto/EllipticCurveUtils.cxx
    library/crypto/Key.cxx
    library/crypto/KeyDerivationUtils.cxx
    library/crypto/SecureRandomGenerator.cxx
    library/crypto/SensitiveDataGuard.cxx
    library/crypto/SignatureVerifier.cxx
    library/crypto/tee/KeyId.cxx
    library/crypto/tee/MessageCounter.cxx
    library/crypto/tee/TeeContext.cxx
    library/crypto/tee/TeeSessionContext.cxx
    library/crypto/tee3/HeaderBodySplittingStream.cxx
    library/crypto/tee3/Kyber768.cxx
    library/crypto/tee3/RcaChain.cxx
    library/crypto/tee3/ServerTeeHandshake.cxx
    library/crypto/tee3/Tee3Context.cxx
    library/crypto/tee3/Tee3Protocol.cxx
    library/crypto/tee3/Tee3SessionContext.cxx
    library/crypto/tee3/TeeError.cxx
    library/crypto/tee3/TeeHandshakeBase.cxx
    library/crypto/tee3/TeeStreamFactory.cxx
    library/crypto/tee3/ClientTeeHandshake.cxx
    library/crypto/tee3/TeeServerKeysImplementation.cxx
    library/crypto/service/CryptoService.cxx
    library/crypto/Signature.cxx
    library/epa/RecordState.cxx
    library/json/BinaryDocument.cxx
    library/json/JsonReader.cxx
    library/json/JsonWriter.cxx
    library/log/ClassifiedText.cxx
    library/log/JsonLogImplementation.cxx
    library/log/Location.cxx
    library/log/Log.cxx
    library/log/LogTrace.cxx
    library/log/PlainLogImplementation.cxx
    library/log/SessionId.cxx
    library/ocsp/OcspHelper.cxx
    library/server/ErrorHandler.cxx
    library/stream/ReadableByteStream.cxx
    library/stream/ReadableStreamImplementationStreamBuf.cxx
    library/stream/Stream.cxx
    library/stream/StreamBuffer.cxx
    library/stream/StreamBuffers.cxx
    library/stream/StreamBuffersContentIterator.cxx
    library/stream/StreamHelper.cxx
    library/stream/WritableStreamImplementationStreamBuf.cxx
    library/stream/implementation/FixedSizeStreamImplementation.cxx
    library/stream/implementation/ProcessingStreamImplementation.cxx
    library/stream/implementation/StreamImplementation.cxx
    library/stream/implementation/ConcatenatingStreamImplementation.cxx
    library/stream/implementation/FunctionStreamImplementation.cxx
    library/stream/StreamFactory.cxx
    library/tsl/CertificateId.cxx
    library/util/Assert.cxx
    library/util/BinaryBuffer.cxx
    library/util/ByteHelper.cxx
    library/util/Configuration.cxx
    library/util/EnumTable.cxx
    library/util/MemoryUnits.cxx
    library/util/Random.cxx
    library/util/Serialization.cxx
    library/util/StringEscaper.cxx
    library/util/Time.cxx
    library/util/TraceId.cxx
    library/util/Uuid.cxx
    library/util/VauCidEncoder.cxx
    library/util/cbor/Cbor.cxx
    library/util/cbor/CborDeserializer.cxx
    library/util/cbor/CborError.cxx
    library/util/cbor/CborReader.cxx
    library/util/cbor/CborSerializer.cxx
    library/util/cbor/CborWriter.cxx
    library/util/ServerException.cxx
    rca-chains.cxx
)
target_compile_definitions(tee3 PRIVATE ENABLE_LOG=1)
target_include_directories(tee3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}) # "library/XXX/Header.hxx"
target_link_libraries(tee3
    PUBLIC
        erp-shared
    PRIVATE
        botan::botan
        boost::boost
        date::date
        glog::glog
        gsl::gsl-lite
        rapidjson
)
