/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/StreamBuffer.hxx"

#include <cstring>
#include <string>

#include "fhirtools/util/Gsl.hxx"
#include "shared/util/String.hxx"

namespace epa
{

/**
 * Data holder. There are two implementations. OwningData owns an array of bytes. DataView references other Data
 * objects, typically instances of OwningData.
 */
class StreamBuffer::Data
{
public:
    virtual ~Data() = default;
    virtual char* writableData() = 0;
    virtual const char* data() const = 0;
    virtual size_t size() const = 0;
    virtual size_t capacity() const = 0;
    virtual void resize(size_t newSize) = 0;
    virtual std::shared_ptr<Data> createView(size_t begin, size_t end) = 0;
};

/**
 * A Data class that owns its data.
 */
class OwningData : public StreamBuffer::Data, public std::enable_shared_from_this<OwningData>
{
public:
    explicit OwningData(size_t capacity);
    OwningData(const char* data, size_t size);

    char* writableData() override;
    const char* data() const override;
    size_t size() const override;
    size_t capacity() const override;
    void resize(size_t newSize) override;
    std::shared_ptr<Data> createView(size_t begin, size_t end) override;

private:
    OwningData(size_t size, size_t capacity);

    std::unique_ptr<char[]> mData;
    size_t mSize;
    size_t mCapacity;
};

/**
 * A Data class that owns its data in form of a std::string,
 * that preferrably is provided at construction time.
 */
class StringData : public StreamBuffer::Data, public std::enable_shared_from_this<StringData>
{
public:
    explicit StringData(std::string data);

    char* writableData() override;
    const char* data() const override;
    size_t size() const override;
    size_t capacity() const override;
    void resize(size_t newSize) override;
    std::shared_ptr<Data> createView(size_t begin, size_t end) override;

private:
    std::string mData;
};


/**
 * A Data class that references external data.
 */
class ReferencedData : public StreamBuffer::Data,
                       public std::enable_shared_from_this<ReferencedData>
{
public:
    ReferencedData(const char* data, size_t size);

    char* writableData() override;
    const char* data() const override;
    size_t size() const override;
    size_t capacity() const override;
    void resize(size_t newSize) override;
    std::shared_ptr<Data> createView(size_t begin, size_t end) override;

private:
    const char* mData;
    size_t mSize;
};


/**
 * Reference an OwningData object that is held via a shared_ptr.
 * This class has become necessary because the FdV client does not stream its documents but loads them into memory.
 * The upload, however, splits the documents into smaller StreamBuffer objects. Depending on the document size this
 * can lead to a lot of calls to StreamBuffer::split(). When done with OwningData objects this would result in a lot
 * of copying bytes from larger buffers into smaller ones. This DataView helps making the split() method run in
 * constant time.
 */
class DataView : public StreamBuffer::Data
{
public:
    DataView(const std::shared_ptr<StreamBuffer::Data>& data, size_t begin, size_t end);

    char* writableData() override;
    const char* data() const override;
    size_t size() const override;
    size_t capacity() const override;
    void resize(size_t newSize) override;
    std::shared_ptr<Data> createView(size_t begin, size_t end) override;

private:
    std::shared_ptr<StreamBuffer::Data> mData;
    size_t mBegin;
    size_t mEnd;
};


// ===== StreamBuffer ========================================================

StreamBuffer StreamBuffer::from(size_t length, const bool isLast)
{
    return StreamBuffer(std::make_shared<OwningData>(length), isLast, false);
}


StreamBuffer StreamBuffer::from(const char* data, size_t length, const bool isLast)
{
    return StreamBuffer(std::make_shared<OwningData>(data, length), isLast, false);
}


StreamBuffer StreamBuffer::from(const std::string& data, const bool isLast)
{
    return StreamBuffer(std::make_shared<OwningData>(data.data(), data.size()), isLast, false);
}


StreamBuffer StreamBuffer::from(std::string&& data, const bool isLast)
{
    return StreamBuffer(std::make_shared<StringData>(std::move(data)), isLast, false);
}


StreamBuffer StreamBuffer::emptyBuffer(const bool isLast)
{
    return StreamBuffer(std::make_shared<DataView>(std::shared_ptr<Data>(), 0, 0), isLast, true);
}


StreamBuffer StreamBuffer::fromReferencing(const char* data, size_t length, bool isLast)
{
    return StreamBuffer(std::make_shared<ReferencedData>(data, length), isLast, true);
}


StreamBuffer::StreamBuffer(bool isReadOnly)
  : mData(std::make_shared<DataView>(std::shared_ptr<Data>(), 0, 0)),
    mIsLast(NotLast),
    mIsReadOnly(isReadOnly)
{
}


StreamBuffer::StreamBuffer(std::shared_ptr<Data>&& data, const bool isLast, const bool isReadOnly)
  : mData(std::move(data)),
    mIsLast(isLast),
    mIsReadOnly(isReadOnly)
{
}


StreamBuffer::StreamBuffer(StreamBuffer&& other) noexcept
  : mData(),
    mIsLast(other.isLast()),
    mIsReadOnly(other.mIsReadOnly)
{
    mData.swap(other.mData);
}


StreamBuffer::~StreamBuffer() = default;


const char* StreamBuffer::data() const
{
    if (mData == nullptr)
        return nullptr;
    else
        return mData->data();
}


size_t StreamBuffer::size() const
{
    if (mData == nullptr)
        return 0;
    else
        return mData->size();
}


WritableStreamBuffer StreamBuffer::makeWritable()
{
    if (mIsReadOnly)
    {
        auto data = std::make_unique<OwningData>(mData->data(), mData->size());
        return WritableStreamBuffer(std::move(data), mIsLast);
    }
    else
        return WritableStreamBuffer(std::move(mData), mIsLast);
}


bool StreamBuffer::isReadOnly() const
{
    return mIsReadOnly;
}


bool StreamBuffer::isLast() const
{
    return mIsLast;
}


void StreamBuffer::setIsLast(const bool isLast)
{
    mIsLast = isLast;
}


char StreamBuffer::operator[](const size_t index) const
{
    return mData->data()[index];
}


char& StreamBuffer::operator[](const size_t index)
{
    if (mIsReadOnly)
        throw std::runtime_error("StreamBuffer is read-only");
    return mData->writableData()[index];
}


std::tuple<StreamBuffer, StreamBuffer> StreamBuffer::split(const size_t length) const
{
    if (mData->size() <= length)
        throw std::logic_error("StreamBuffer smaller than split position");

    StreamBuffer bufferA =
        StreamBuffer(mData->createView(0, length), StreamBuffer::NotLast, mIsReadOnly);
    StreamBuffer bufferB =
        StreamBuffer(mData->createView(length, mData->size()), mIsLast, mIsReadOnly);

    return std::make_tuple(std::move(bufferA), std::move(bufferB));
}


std::string_view StreamBuffer::toString() const
{
    if (mData->size() > 0)
        return std::string_view(mData->data(), mData->size());
    else
        return std::string_view();
}


// ===== WritableStreamBuffer ================================================

WritableStreamBuffer WritableStreamBuffer::createBuffer(const size_t requestedLength)
{
    WritableStreamBuffer buffer(std::make_unique<OwningData>(requestedLength), NotLast);
    return buffer;
}


WritableStreamBuffer::WritableStreamBuffer(std::shared_ptr<Data>&& data, const bool isLast)
  : StreamBuffer(std::move(data), isLast, false)
{
}


char* WritableStreamBuffer::writableData()
{
    return mData->writableData();
}


size_t WritableStreamBuffer::capacity() const
{
    return mData->capacity();
}


void WritableStreamBuffer::setSize(size_t newSize)
{
    Expects(newSize <= mData->capacity());
    mData->resize(newSize);
}


// ===== OwningData ==========================================================

OwningData::OwningData(size_t size, size_t capacity)
  : mData{std::make_unique<char[]>(capacity)},
    mSize{size},
    mCapacity{capacity}
{
}

OwningData::OwningData(size_t capacity)
  : OwningData{static_cast<size_t>(0), capacity}
{
}


OwningData::OwningData(const char* data, size_t size)
  : OwningData(size, size)
{
    String::copyMemorySafely(mData.get(), mCapacity, data, size);
}


char* OwningData::writableData()
{
    return mData.get();
}


const char* OwningData::data() const
{
    return mData.get();
}


size_t OwningData::size() const
{
    return mSize;
}


size_t OwningData::capacity() const
{
    return mCapacity;
}


void OwningData::resize(size_t newSize)
{
    Expects(newSize <= mCapacity);
    mSize = newSize;
}


std::shared_ptr<StreamBuffer::Data> OwningData::createView(size_t begin, size_t end)
{
    Expects(begin <= end);
    Expects(end <= mSize);
    return std::make_shared<DataView>(shared_from_this(), begin, end);
}


// ===== StringData ==========================================================

StringData::StringData(std::string data)
  : mData(std::move(data))
{
}


char* StringData::writableData()
{
    return mData.data();
}


const char* StringData::data() const
{
    return mData.data();
}


size_t StringData::size() const
{
    return mData.size();
}


size_t StringData::capacity() const
{
    return mData.size();
}


void StringData::resize(size_t)
{
    throw std::logic_error("StringData::resize is not implemented");
}


std::shared_ptr<StreamBuffer::Data> StringData::createView(size_t begin, size_t end)
{
    Expects(begin <= end);
    Expects(end <= mData.size());
    return std::make_shared<DataView>(shared_from_this(), begin, end);
}


// ===== ReferencedData ============================================================

ReferencedData::ReferencedData(const char* data, size_t size)
  : mData(data),
    mSize(size)
{
}


char* ReferencedData::writableData()
{
    throw std::logic_error("ReferencedData is read-only");
}


const char* ReferencedData::data() const
{
    return mData;
}


size_t ReferencedData::size() const
{
    return mSize;
}


size_t ReferencedData::capacity() const
{
    return mSize;
}


void ReferencedData::resize(size_t)
{
    throw std::logic_error("ReferencedData is cannot be resized");
}


std::shared_ptr<StreamBuffer::Data> ReferencedData::createView(size_t begin, size_t end)
{
    Expects(begin <= end);
    Expects(end <= size());
    return std::make_shared<DataView>(shared_from_this(), begin, end);
}


// ===== DataView ============================================================

DataView::DataView(const std::shared_ptr<StreamBuffer::Data>& data, size_t begin, size_t end)
  : mData(data),
    mBegin(begin),
    mEnd(end)
{
}


char* DataView::writableData()
{
    return mData->writableData() + mBegin;
}


const char* DataView::data() const
{
    return mData->data() + mBegin;
}


size_t DataView::size() const
{
    return mEnd - mBegin;
}


size_t DataView::capacity() const
{
    return mEnd - mBegin;
}


void DataView::resize(size_t newSize)
{
    Expects(newSize < size());
    mEnd = mBegin + newSize;
}


std::shared_ptr<StreamBuffer::Data> DataView::createView(size_t begin, size_t end)
{
    Expects(begin <= end);
    Expects(end <= size());
    return std::make_shared<DataView>(mData, mBegin + begin, mBegin + end);
}

} // namespace epa
