/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/ErpTypes.hxx"

#include "shared/database/DatabaseModel.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/TLog.hxx"


ErpBlob::ErpBlob (void)
    : ErpBlob("", 0)
{
}


ErpBlob::ErpBlob (
    std::vector<uint8_t>&& data_,
    GenerationId generation_)
    : data(std::move(data_)),
      generation(generation_)
{
}


ErpBlob::ErpBlob (
    std::string_view data_,
    GenerationId generation_)
    : data(SafeString::no_zero_fill, data_.size()),
      generation(generation_)
{
    std::ranges::copy(data_, static_cast<char*>(data));
}


ErpBlob::ErpBlob (const ErpBlob& other)
    : data(SafeString::no_zero_fill, other.data.size()),
      generation(other.generation)
{
    std::copy_n(static_cast<const char*>(other.data), other.data.size(), static_cast<char*>(data));
}


/**
 * Assume that `base64` contains a base64 encoded memory dump of this C struct:

 * typedef struct ERPBlob_s {
 *   uint32_t BlobGeneration;
 *   size_t BlobLength;   // length
 *   char BlobData[MAX_BUFFER]; // binary
 * } ERPBlob;
 *
 * Note that due to alignment of BlobLength on a multiple of sizeof(size_t)==8 there is a gap of 4 bytes between
 * BlobGeneration and BlobLength.
 * Note also that this is hacky code that tries to do the best with an under-defined data structure.
 */
ErpBlob ErpBlob::fromCDump (const std::string_view base64)
{
    std::string binary = Base64::decodeToString(base64);
    ErpBlob blob;
    blob.generation = *reinterpret_cast<const uint32_t*>(binary.data());
    const auto sizeOfSizeT = sizeof(size_t);
    static_assert(sizeOfSizeT == 8);
    const size_t offset = sizeOfSizeT /* compensate for aliasing instead of using 4==sizeof(uint32_t)*/ + sizeof(size_t);
    const size_t blobSize = *reinterpret_cast<const size_t*>(binary.data() + sizeOfSizeT);
    Expect(binary.size()-offset >= blobSize, "inconsistent size");
    blob.data = SafeString(SafeString::no_zero_fill, blobSize);
    const auto dataStart = binary.begin() + offset;
    std::copy(dataStart, dataStart + gsl::narrow<ptrdiff_t>(blobSize), static_cast<char*>(blob.data));

    return blob;
}


ErpVector::ErpVector(const std::vector<uint8_t>& source)
    : std::vector<uint8_t>(source)
{
}


ErpBlob& ErpBlob::operator= (const ErpBlob& other)
{
    if (this != &other)
    {
        data = SafeString(SafeString::no_zero_fill, other.data.size());
        std::copy(static_cast<const char*>(other.data), static_cast<const char*>(other.data) + other.data.size(), static_cast<char*>(data));
        generation = other.generation;
    }
    return *this;
}

bool ErpBlob::operator==(const ErpBlob& rhs) const
{
    return data == rhs.data && generation == rhs.generation;
}

bool ErpBlob::operator!=(const ErpBlob& rhs) const
{
    return ! (rhs == *this);
}


ErpVector ErpVector::create (const std::string_view data)
{
    ErpVector vector;
    vector.resize(data.size());
    std::ranges::copy(data, vector.begin());
    return vector;
}


bool ErpVector::operator== (const ErpVector& other) const
{
    return size() == other.size()
           && std::equal(begin(), end(), other.begin());
}


size_t ErpVector::hash (void) const
{
    return std::hash<std::string_view>()(std::string_view(reinterpret_cast<const char*>(data()), size()));
}


bool ErpVector::startsWith (const ErpVector& other) const
{
    return size() >= other.size()
           &&  std::equal(other.begin(), other.end(), begin());
}


ErpVector ErpVector::tail (size_t length) const
{
    Expect(length <= size(), "tail must not be longer than ErpVector");
    ErpVector result;
    result.reserve(length);
    std::copy(end() - gsl::narrow<ptrdiff_t>(length), end(), std::back_inserter(result));
    return result;
}

ErpVector::ErpVector(const std::basic_string_view<std::byte>& blob)
{
    append(blob);
}

ErpVector::ErpVector(const db_model::Blob& blob)
    : ErpVector(blob.binarystring())
{
}

void ErpVector::append(const std::basic_string_view<std::byte>& blob)
{
    reserve(blob.size() + size());
    insert(end(), reinterpret_cast<const uint8_t*>(blob.data()),
           reinterpret_cast<const uint8_t*>(blob.data()) + blob.size());
}

void ErpVector::append(const db_model::Blob& blob)
{
    append(blob.binarystring());
}
