/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee3/HeaderBodySplittingStream.hxx"
#include "library/common/BoostBeastHeaderConverter.hxx"
#include "library/server/ErrorHandler.hxx"
#include "library/util/Assert.hxx"

#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>

namespace epa
{

/**
 * Determine how to check if a stream has reached its end and track and run this check when data
 * is read from a stream.
 * The different modes are:
 * - body is presented as multiple parts (Content-Type:multipart/related). The end of the stream
 *   is expected to be tracked by gSOAP.
 * - body is chunked (Transfer-Encoding:chunked): the boost::beast parser must parse the body,
 *   process the chunk headers and pass on only the body bytes.
 * - body has an explicit content length: the boost beast parser tracks the remaining number of
 *   body bytes.
 * - there is no explicit content length: for POST and PUT requests this leads to an error, for all
 *   other requests an implicit content length of 0 is assumed (standard behavior of the boost beast
 *   parser).
 */
class HeaderBodySplittingStreamImplementation::EndDetector
{
public:
    explicit EndDetector(const Header& header, const bool isChunked, const bool isRequest)
    {
        if (header.hasHeader(Header::ContentLength))
        {
            // With an explicit Content-Length value in the main header, we can just wait until
            // mParser.is_done() returns true;
            mEosCondition = ParserDone;
            Assert(! isChunked);
        }
        else if (const auto& contentType = header.header(Header::ContentType);
                 contentType && contentType->find("multipart/related") != std::string::npos)
        {
            // The request is a multi part request, that does not have an overall content length.
            // Technically that would allow individual parts to have Content-Length fields. But
            // with Gematik/SOAP requests that is not the case. That means we, or rather GSoap,
            // has to look for the --<id>-- marker at the end of the body.
            // On this low level that means that we have to ignore the `mParser.is_done()`
            // condition and let GSoap decide when to stop reading data.
            mEosCondition = MultiPart;
            Assert(! isChunked);
        }
        else if (const auto& transferEncoding = header.header(Header::TransferEncoding);
                 transferEncoding
                 && (*transferEncoding == "chunked" || *transferEncoding == "aws-chunked"))
        {
            // Body is transported in individual chunks. These are parsed by the boost beast
            // parser.
            // Note that for the communication with IBM's COS a double chunking is used with headers
            //     Transfer-Encoding: chunked
            //     aws-chunk...
            // but https://docs.aws.amazon.com/AmazonS3/latest/API/sigv4-streaming.html
            // indicates that
            //     Transfer-Encoding: aws-chunked
            // should also work.

            mEosCondition = ParserDone;
            Assert(isChunked);
        }
        else
        {
            Assert(! isChunked);
            if (! isRequest
                || (header.method() != HttpMethod::PUT && header.method() != HttpMethod::POST))
            {
                // Assume that the missing Content-Length means that there is no body.
                mEosCondition = ParserDone;
            }
            else
                Failure() << "request has no Content-Length and is not a multipart request: "
                          << "don't know how to determine the end-of-stream condition";
        }
    }

    /**
     * Return true when the end of the stream is reached.
     * Note that for multipart encoding, this method never returns true. That is because
     * gSoap is responsible to detect the end of the stream and, if it does, not to read
     * from the stream again and hence not call the eos() method in a case where it should
     * return true.
     */
    bool eos(const bool parserDone)
    {
        switch (mEosCondition)
        {
            case ParserDone:
                return parserDone;

            case MultiPart:
                // We will never return StreamBuffer::IsLast and rely on GSoap to
                // detect the multipart end marker and stop triggering this method.
                return false;

            case None:
            default:
                Failure() << "don't know how to determine end-of-stream";
        }
    }

    /**
     * Return true when data that is read from a stream has to be parsed by boost::beast.
     */
    bool mustParseBody() const
    {
        return mEosCondition == ParserDone;
    }

private:
    enum EosCondition
    {
        None,
        ParserDone,
        MultiPart
    };
    EosCondition mEosCondition;
};


/**
 * The boost::beast::parser type is templated and depends on whether a request or a response is
 * parsed. We can make this choice only at runtime and therefore have to introduce an abstraction
 * layer to transition from compile time choice to runtime choice, hence the abstract base class.
 */
class HeaderBodySplittingStreamImplementation::Parser
{
public:
    virtual ~Parser() = default;

    enum State
    {
        OK,
        NeedsMore,
        Error
    };
    virtual std::tuple<size_t, State> parseHeader(const StreamBuffers& buffers) = 0;
    virtual StreamBuffers parseBody(StreamBuffers&& buffers) = 0;
    virtual bool isHeaderFinished() = 0;
    virtual bool isDone() = 0;
    virtual bool isChunked() = 0;
    virtual bool isRequest() = 0;
    virtual Header getParsedHeader() = 0;
};

namespace
{
    template<bool IsRequest>
    class ParserImplementation : public HeaderBodySplittingStreamImplementation::Parser
    {
    public:
        explicit ParserImplementation(const std::optional<uint64_t> bodyLimit)
          : mParser(),
            mBuffers(StreamBuffer::NotLast)
        {
            mParser.body_limit(
                bodyLimit.has_value() ? boost::optional<uint64_t>(bodyLimit.value())
                                      : boost::optional<uint64_t>());
        }

        /**
         * Parse a portion of the header.
         * @return how many bytes have been consumed and whether more calls to this method
         * are required, or if there has been an error.
         */
        std::tuple<size_t, State> parseHeader(const StreamBuffers& buffers) override
        {
            const auto bufferSequence = wrapBuffers(buffers);

            boost::system::error_code ec = {};
            // clang-tidy reports access to uninitialized memory deep in boost beast code.
            // A simple example was not enough to trigger the problem (and create a bug task
            // for boost beast). As we have not seen any problems in this area and there are
            // reports regarding false positives for the UndefinedBinaryOperatorResult check,
            // the clang-tidy check has been disabled.
            // NOLINTBEGIN(clang-analyzer-core.UndefinedBinaryOperatorResult)
            const size_t count = mParser.put(bufferSequence, ec);
            // NOLINTEND(clang-analyzer-core.UndefinedBinaryOperatorResult)

            if (ec)
            {
                if (ec == boost::beast::http::error::need_more)
                    return {count, State::NeedsMore};
                else
                {
                    ErrorHandler(ec).reportServerError("splitHeaderBody");
                    return {count, State::Error};
                }
            }
            else
            {
                return {count, State::OK};
            }
        }

        /**
         * Parse a portion of the body.
         * @return the bytes that belong to the body, i.e. filter out (after processing them) chunk
         * headers.
         */
        StreamBuffers parseBody(StreamBuffers&& buffers) override
        {
            // Append the new data to data from previous calls that have not yet been consumed
            // by the parser.
            mBuffers.pushBack(std::move(buffers));

            // The boost::beast parser may have to be called multiple times. In each pass
            // the consumed bytes are removed from the head of mBuffers.
            while (mBuffers.size() > 0)
            {
                // Wrap mBuffers in a thin structure that makes the data compatible with the parser.
                const auto bufferSequence = wrapBuffers(mBuffers);

                // Let the parser process the next portion of the body.
                boost::system::error_code ec = {};
                const size_t count = mParser.put(bufferSequence, ec);

                // When the parser needs more bytes, break the loop and wait next call.
                if (ec == boost::beast::http::error::need_more)
                    break;
                else if (ec)
                    ErrorHandler(ec).throwOnServerError("splitHeaderBody");

                // Split off the bytes that have been consumed by the parser.
                Assert(count > 0);
                mBuffers = mBuffers.splitOff(count);
            }

            // Return the portion of the body that the parser has let through.
            std::string body;
            body.swap(mParser.get().body());
            return StreamBuffers::from(body.data(), body.size(), mParser.is_done());
        }

        bool isHeaderFinished() override
        {
            return mParser.is_header_done();
        }
        bool isDone() override
        {
            return mParser.is_done();
        }
        bool isChunked() override
        {
            return mParser.chunked();
        }
        bool isRequest() override
        {
            return IsRequest;
        }
        Header getParsedHeader() override
        {
            if constexpr (IsRequest)
                return BoostBeastHeaderConverter::convertRequestHeader(mParser.get());
            else
                return BoostBeastHeaderConverter::convertResponseHeader(mParser.get());
        }

    private:
        boost::beast::http::parser<IsRequest, boost::beast::http::string_body> mParser;
        StreamBuffers mBuffers;

        std::vector<boost::asio::const_buffer> wrapBuffers(const StreamBuffers& buffers)
        {
            // Create a thin wrapper around the StreamBuffers so that the boost::beast parser
            // can work with them.
            std::vector<boost::asio::const_buffer> bufferSequence;
            bufferSequence.reserve(buffers.size());
            for (const auto& buffer : buffers)
                bufferSequence.emplace_back(buffer.data(), buffer.size());
            return bufferSequence;
        }
    };
}


HeaderBodySplittingStreamImplementation::HeaderBodySplittingStreamImplementation(
    Stream&& delegate,
    const std::optional<uint64_t> bodyLimit,
    HeaderConsumer&& headerConsumer,
    const ReadableOrWritable readableOrWritable,
    const bool isRequest,
    const bool readHeaderOnInitialization)
  : mParser(
        isRequest ? std::unique_ptr<Parser>(new ParserImplementation<true>(bodyLimit))
                  : std::unique_ptr<Parser>(new ParserImplementation<false>(bodyLimit))),
    mEndDetector(),
    mHeaderConsumer(std::move(headerConsumer)),
    mReadableOrWritable(readableOrWritable),
    mDelegate(std::move(delegate)),
    mIsHeaderFinished(false),
    mHeaderBuffers(StreamBuffer::NotLast)
{
    if (readHeaderOnInitialization)
        readHeader();
}


void HeaderBodySplittingStreamImplementation::readHeader()
{
    // Header can not be read from a writable stream (it will be read when data is written to the
    // stream).
    Assert(mReadableOrWritable == ReadableOrWritable::Readable);

    while (! (mIsHeaderFinished || mParser->isHeaderFinished()))
    {
        read();
    }
}


HeaderBodySplittingStreamImplementation::~HeaderBodySplittingStreamImplementation() = default;


bool HeaderBodySplittingStreamImplementation::isReadSupported() const
{
    return mReadableOrWritable == ReadableOrWritable::Readable;
}


bool HeaderBodySplittingStreamImplementation::isWriteSupported() const
{
    return mReadableOrWritable == ReadableOrWritable::Writable;
}


StreamBuffers HeaderBodySplittingStreamImplementation::doRead()
{
    if (! mIsHeaderFinished)
    {
        processHeaderBuffers(mDelegate.read());
        return StreamBuffers(StreamBuffer::NotLast);
    }
    else if (
        mEndDetector == nullptr                  // Header parsing finished unsuccessfully.
        || mEndDetector->eos(mParser->isDone())) // Header parsing finished successfully
    {
        mHeaderBuffers.setIsLast(true);
        return std::move(mHeaderBuffers);
    }
    else
    {
        StreamBuffers buffers =
            mHeaderBuffers.isEmpty() ? mDelegate.read() : std::move(mHeaderBuffers);

        if (buffers.isEmpty())
        {
            // Empty buffers are a normal case. This happens when for example a decryption step has not
            // had enough data to decrypt the next block. Just exit early, there is nothing to do for
            // empty buffers.
            return buffers;
        }

        return processBodyBuffers(std::move(buffers));
    }
}


void HeaderBodySplittingStreamImplementation::doWrite(StreamBuffers&& buffers)
{
    if (buffers.isEmpty())
        return;

    if (! mIsHeaderFinished)
    {
        processHeaderBuffers(std::move(buffers));

        // When the header has been parsed and there are bytes of the body in mHeaderBuffers,
        // write them now because we may not be called another time.
        if (mIsHeaderFinished && ! mHeaderBuffers.isEmpty())
        {
            mHeaderBuffers.setIsLast(mEndDetector->eos(mParser->isDone()));
            mDelegate.write(std::move(mHeaderBuffers));
        }
    }
    else if (mEndDetector->eos(mParser->isDone()))
    {
        mHeaderBuffers.setIsLast(true);
        mDelegate.write(std::move(mHeaderBuffers));
    }
    else
    {
        if (! mHeaderBuffers.isEmpty())
        {
            mHeaderBuffers.pushBack(std::move(buffers));
            buffers = std::move(mHeaderBuffers);
        }

        if (buffers.isEmpty())
        {
            // Empty buffers are a normal case. This happens when for example a decryption step has not
            // had enough data to decrypt the next block. Just exit early, there is nothing to do for
            // empty buffers.
            return;
        }
        else
            mDelegate.write(processBodyBuffers(std::move(buffers)));
    }
}


void HeaderBodySplittingStreamImplementation::processHeaderBuffers(StreamBuffers&& buffers)
{
    if (buffers.size() == 0 && buffers.isLast())
    {
        LOG(WARNING) << "stream ended while parsing header";
        mIsHeaderFinished = true;
        return;
    }

    mHeaderBuffers.pushBack(std::move(buffers));

    // Parse the stream data.
    const auto [count, state] = mParser->parseHeader(mHeaderBuffers);

    Assert(state != Parser::Error) << " can not parse header " << mHeaderBuffers.toString();

    // Discard the head of the buffers, the part that has been consumed by the parser.
    if (count > 0)
        mHeaderBuffers = mHeaderBuffers.splitOff(count);

    // Check if the header is finished and for future reads, we can bypass the parser.
    if (mParser->isHeaderFinished())
    {
        mIsHeaderFinished = true;

        auto header = mParser->getParsedHeader();
        mEndDetector =
            std::make_unique<EndDetector>(header, mParser->isChunked(), mParser->isRequest());

        // Make the parsed header available to our caller.
        if (mHeaderConsumer)
            mHeaderConsumer(std::move(header));
    }
}


StreamBuffers HeaderBodySplittingStreamImplementation::processBodyBuffers(StreamBuffers&& buffers)
{
    if (mEndDetector->mustParseBody())
        return mParser->parseBody(std::move(buffers));
    else
        return buffers;
}

} // namespace epa
