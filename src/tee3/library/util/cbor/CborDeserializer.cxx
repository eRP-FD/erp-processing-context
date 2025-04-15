/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/cbor/CborDeserializer.hxx"

#include "shared/util/Base64.hxx"

namespace epa
{

void CborDeserializer::deserialize(
    const BinaryBuffer& cborData,
    const CborDeserializerContext context)
{
    CborReader reader(cborData, context.maxStringLength);

    // Call read() with a callback that forwards to the processValue() method.
    // The callback will be called for each item (top-level or nested) in the `cborData`
    // CBOR serialization.
    std::set<DeserializationStatus> status;
    reader.read([this, &status, &context](
                    const std::string_view path,
                    const Cbor::Type type,
                    const bool isLast,
                    Cbor::Data data) {
        status.emplace(processValue(mExpectedItems, path, type, isLast, std::move(data), context));
    });

    for (const auto& expectation : mExpectedItems)
        status.emplace(
            processDeserializationErrors(expectation.first, expectation.second.callCount, context));

    // If there have been errors, we have to prioritize which error type to report.
    if (status.contains(DeserializationStatus::MissingItem))
        Failure() << "one or more expected items where not present in CBOR data"
                  << assertion::exceptionType<CborError>(
                         CborError::Code::DeserializationMissingItem);
    else if (status.contains(DeserializationStatus::ExtraItem))
        Failure() << "unexpected items found in CBOR data"
                  << assertion::exceptionType<CborError>(CborError::Code::DeserializationError);
}


CborDeserializer::DeserializationStatus CborDeserializer::processDeserializationErrors(
    const std::string& itemName,
    const size_t callCount,
    const CborDeserializerContext& context)
{
    if (callCount == 0)
    {
        if (context.missingItems == CborDeserializerErrorMode::Error)
        {
            LOG(ERROR) << "expected member '" << itemName
                       << "' has not been initialized during deserialization";
            return DeserializationStatus::MissingItem;
        }
        else if (context.missingItems == CborDeserializerErrorMode::Warning)
        {
            LOG(WARNING) << "expected member '" << itemName
                         << "' has not been initialized during deserialization";
        }

        // Ignore the missing item.
        return DeserializationStatus::OK;
    }
    else if (callCount > 1)
    {
        if (context.extraItems == CborDeserializerErrorMode::Error)
        {
            LOG(ERROR) << "expected member '" << itemName
                       << "' has been initialized more than once: " << callCount;
            return DeserializationStatus::ExtraItem;
        }
        else if (context.extraItems == CborDeserializerErrorMode::Warning)
        {
            LOG(WARNING) << "expected member '" << itemName
                         << "' has been initialized more than once: " << callCount;
        }

        // Ignore the extra item.
        return DeserializationStatus::OK;
    }
    return DeserializationStatus::OK;
}


CborDeserializer::DeserializationStatus CborDeserializer::processValue(
    CborDeserializer::ExpectedItems& expectedItems,
    const std::string_view path,
    const Cbor::Type type,
    const bool isLast,
    Cbor::Data&& data,
    const CborDeserializerContext& context)
{
    // Find a registered item via its CBOR path.
    const auto iterator = expectedItems.find(std::string(path));
    if (iterator == expectedItems.end())
    {
        // Handle unexpected items. By default they are ignored.
        if (context.extraItems == CborDeserializerErrorMode::Error)
        {
            LOG(ERROR) << "unexpected cbor value '" << path << "'";
            return DeserializationStatus::MissingItem;
        }
        else if (context.extraItems == CborDeserializerErrorMode::Warning)
            LOG(WARNING) << "unexpected cbor value '" << path << "'";
        // else ignore the unexpected item.

        return DeserializationStatus::OK;
    }

    // Remember, how often a single item has been read (expected: exactly once).
    ++iterator->second.callCount;

    // When `isLast` is `false` it means that the value is encoded in multiple parts. That is
    // supported only for byte and text strings.
    if (! isLast)
    {
        Assert(type == Cbor::Type::ByteString || type == Cbor::Type::TextString)
            << "multi-part values are supported only for byte and text strings, not for "
            << Cbor::toString(type);
    }

    // Finally, forward to the registered handler for the key (`path`).
    iterator->second.receiver(std::move(data), path);

    return DeserializationStatus::OK;
}

} // namespace epa
