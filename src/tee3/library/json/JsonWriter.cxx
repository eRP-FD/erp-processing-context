/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/json/JsonWriter.hxx"
#include "library/json/BinaryDocument.hxx"
#include "library/util/Assert.hxx"
#include "library/wrappers/RapidJson.hxx"

#include "fhirtools/util/Gsl.hxx"

namespace epa
{

struct JsonWriter::RapidJsonWriter : private boost::noncopyable
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer{buffer};
};


JsonWriter::JsonWriter(JsonVariant variant, bool binary)
  : mVariant{variant},
    mDataType{UNDECIDED}
{
    // This object is the root JsonWriter and initially owns the RapidJsonWriter.
    // As nested JsonWriters are created from this instance, they will borrow the writer and return
    // it upon destruction.
    if (binary)
        mBinDoc = std::make_unique<BinaryDocument>();
    else
        mWriter = std::make_unique<RapidJsonWriter>();
}


JsonWriter::~JsonWriter()
{
    finalize();
}


JsonVariant JsonWriter::getVariant() const
{
    return mVariant;
}


std::string JsonWriter::toString()
{
    Assert(mParentWriter == nullptr) << "toString() is only supported on the root JsonWriter";
    finalize();
    if (mWriter)
        return std::string{mWriter->buffer.GetString(), mWriter->buffer.GetSize()};
    else if (mBinDoc)
        return mBinDoc->encode();
    else
        Failure() << "Blocked on pending nested JsonWriter";
}


void JsonWriter::makeStringValue(std::string_view value)
{
    Assert(mWriter != nullptr || mBinDoc != nullptr) << "Blocked on pending nested JsonWriter";

    if (mDataType == OBJECT_KEY)
    {
        // Special case: If we are writing an object key, we have to call Key() instead of String().
        if (mWriter)
            mWriter->writer.Key(value.begin(), gsl::narrow<rapidjson::SizeType>(value.length()));
        else
            mBinDoc->addKey(std::string(value));
        mDataType = SINGLE_VALUE;
        return;
    }

    Assert(mDataType == UNDECIDED) << "Cannot write plain value, JsonWriter already has content";
    mDataType = SINGLE_VALUE;
    if (mWriter)
        mWriter->writer.String(value.begin(), gsl::narrow<rapidjson::SizeType>(value.length()));
    else
        mBinDoc->addString(std::string(value));
}


void JsonWriter::makeInt64Value(std::int64_t value)
{
    Assert(mWriter != nullptr || mBinDoc != nullptr) << "Blocked on pending nested JsonWriter";
    Assert(mDataType != OBJECT_KEY) << "JSON does not support integer keys";
    Assert(mDataType == UNDECIDED) << "Cannot write plain value, JsonWriter already has content";
    mDataType = SINGLE_VALUE;
    if (mWriter)
        mWriter->writer.Int64(value);
    else
        mBinDoc->addInt64(value);
}


void JsonWriter::makeUint64Value(std::uint64_t value)
{
    Assert(mWriter != nullptr || mBinDoc != nullptr) << "Blocked on pending nested JsonWriter";
    Assert(mDataType != OBJECT_KEY) << "JSON does not support integer keys";
    Assert(mDataType == UNDECIDED) << "Cannot write plain value, JsonWriter already has content";
    mDataType = SINGLE_VALUE;
    if (mWriter)
        mWriter->writer.Uint64(value);
    else
        mBinDoc->addInt64(static_cast<std::int64_t>(value));
}


void JsonWriter::makeDoubleValue(double value)
{
    Assert(mWriter != nullptr || mBinDoc != nullptr) << "Blocked on pending nested JsonWriter";
    Assert(mDataType != OBJECT_KEY) << "JSON does not support floating-point keys";
    Assert(mDataType == UNDECIDED) << "Cannot write plain value, JsonWriter already has content";
    mDataType = SINGLE_VALUE;
    if (mWriter)
        mWriter->writer.Double(value);
    else
        mBinDoc->addDouble(value);
}


void JsonWriter::makeBoolValue(bool value)
{
    Assert(mWriter != nullptr || mBinDoc != nullptr) << "Blocked on pending nested JsonWriter";
    Assert(mDataType != OBJECT_KEY) << "JSON does not support boolean keys";
    Assert(mDataType == UNDECIDED) << "Cannot write plain value, JsonWriter already has content";
    mDataType = SINGLE_VALUE;
    if (mWriter)
        mWriter->writer.Bool(value);
    else
        mBinDoc->addBool(value);
}


void JsonWriter::makeJsonObject(std::string_view json)
{
    Assert(mBinDoc == nullptr) << "makeJsonObject is not supported in binary documents";
    Assert(mWriter != nullptr) << "Blocked on pending nested JsonWriter";
    Assert(mDataType != OBJECT_KEY) << "JSON does not support objects as keys";
    Assert(mDataType == UNDECIDED) << "Cannot write plain value, JsonWriter already has content";
    Assert(json.starts_with('{') && json.ends_with('}')) << "Given JSON is not an object";
    mDataType = SINGLE_VALUE;
    mWriter->writer.RawValue(json.data(), json.size(), rapidjson::Type::kObjectType);
}


void JsonWriter::makeArrayValue(
    std::size_t size,
    const std::function<void(std::size_t, JsonWriter&)>& callback)
{
    Assert(mWriter != nullptr || mBinDoc != nullptr) << "Blocked on pending nested JsonWriter";
    Assert(mDataType != OBJECT_KEY) << "JSON does not support arrays as keys";
    Assert(mDataType == UNDECIDED) << "Cannot write array, JsonWriter already has content";
    if (mWriter)
        mWriter->writer.StartArray();
    else
        mBinDoc->startArray();
    mDataType = ARRAY;
    for (std::size_t index = 0; index < size; ++index)
    {
        JsonWriter arrayValue{*this, UNDECIDED};
        callback(index, arrayValue);
    }
    finalize();
}


JsonWriter::JsonWriter(JsonWriter& parent, DataType dataType)
  : mVariant{parent.mVariant},
    mParentWriter{&parent},
    mDataType{dataType}
{
    Assert(parent.mWriter != nullptr || parent.mBinDoc != nullptr)
        << "Parent blocked on nested JsonWriter";
    if (parent.mWriter)
    {
        Assert(! parent.mWriter->writer.IsComplete())
            << "JsonWriter does not support more nested content";
    }

    switch (mDataType)
    {
        case UNDECIDED:
            // Nothing to do.
            break;
        case OBJECT_KEY:
            // If the parent is still undecided, turn it into an object.
            if (parent.mDataType == UNDECIDED)
            {
                parent.mDataType = OBJECT;
                if (parent.mWriter)
                    parent.mWriter->writer.StartObject();
                else
                    parent.mBinDoc->startObject();
            }
            else
            {
                Assert(parent.mDataType == OBJECT) << "Keys can only be written inside objects";
            }
            break;
        case OBJECT:
            if (parent.mWriter)
                parent.mWriter->writer.StartObject();
            else
                parent.mBinDoc->startObject();
            break;
        case ARRAY:
            if (parent.mWriter)
                parent.mWriter->writer.StartArray();
            else
                parent.mBinDoc->startArray();
            break;
        case SINGLE_VALUE:
            Failure() << "Cannot directly create single-valued nodes";
        case FINALIZED_JSON:
            Failure() << "Cannot directly create finalized nodes";
    }

    mWriter = std::move(parent.mWriter);
    mBinDoc = std::move(parent.mBinDoc);
}


void JsonWriter::finalize() noexcept
{
    // It's not our turn to be finalized.
    if (! mWriter && ! mBinDoc)
        return;

    switch (mDataType)
    {
        case UNDECIDED:
            // If nothing was written to this value, default to {}. This can happen when an object
            // only writes content with writeOptional(...), but all values were nullopt.
            if (mWriter)
                mWriter->writer.StartObject();
            else
                mBinDoc->startObject();
            [[fallthrough]];
        case OBJECT:
            if (mWriter)
                mWriter->writer.EndObject();
            else
                mBinDoc->endObject();
            break;
        case ARRAY:
            if (mWriter)
                mWriter->writer.EndArray();
            else
                mBinDoc->endArray();
            break;
        case SINGLE_VALUE:
        case FINALIZED_JSON:
            // Nothing to do.
            break;
        case OBJECT_KEY:
            // We expected to receive a key, but this didn't happen. We don't want to throw because
            // this is called by the destructor, so instead we free the writer, leaving the parent
            // in an invalid state.
            mWriter.reset();
            mBinDoc.reset();
            return;
    }

    mDataType = FINALIZED_JSON;
    // Return ownership of the writer to the parent, if any.
    if (mParentWriter)
    {
        mParentWriter->mWriter = std::move(mWriter);
        mParentWriter->mBinDoc = std::move(mBinDoc);
    }
}

} // namespace epa
