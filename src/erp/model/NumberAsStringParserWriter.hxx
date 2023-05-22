/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_NUMBERASSTRINGPARSERWRITER_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_NUMBERASSTRINGPARSERWRITER_HXX

#include <rapidjson/writer.h>

#include "erp/model/NumberAsStringParserDocument.hxx"

namespace model
{
/**
 * Generic writer which has to be used to serialize the document of the resource models into
 * a json string. This generic writer removes the prefixes from the document which have been
 * inserted for maintaining the precision of numbers.
 *
 * To create a json string from the document removing the prefixes you have to use the
 * "NumberAsStringParserWriter" as follows:
 *
 *     auto document = model.jsonDocument();
 *     rapidjson::StringBuffer buffer;
 *     NumberAsStringParserWriter<rapidjson::StringBuffer> writer(buffer);
 *     document.Accept(writer);
 */
template<class Stream>
class NumberAsStringParserWriter : public rapidjson::Writer<Stream>
{
public:
    typedef typename rapidjson::UTF8<>::Ch Ch;

    explicit NumberAsStringParserWriter(Stream& out)
        : rapidjson::Writer<Stream>(out)
    {
    }

    bool String(const Ch* text, rapidjson::SizeType length, bool copy)
    {
        RAPIDJSON_ASSERT(text != nullptr && length > 0);
        RAPIDJSON_ASSERT(text[0] == NumberAsStringParserDocument::prefixNumber || text[0] == NumberAsStringParserDocument::prefixString);
        if (text[0] == NumberAsStringParserDocument::prefixNumber)
        {
            // Our base class has a RawNumber() method but that still adds double quotes around `text`.
            // Therefore we have to write the number string out to the stream ourself.
            rapidjson::Writer<Stream>::Prefix(rapidjson::kNumberType);

            // The PutUnsafe in this loop bypasses a lot of UTF8 handling of our base class's String() method.
            // But as numbers have a very limited character set, that should not be a problem.
            // Or are there code points beyond 7 bit ASCII that can be used in JSON numbers?
            while (--length > 0)
            {
                PutUnsafe(*rapidjson::Writer<Stream>::os_, *++text);
            }
            return rapidjson::Writer<Stream>::EndValue(true);
        }
        else // if (text[0] == NumberAsStringParserDocument::prefixString)
        {
            return rapidjson::Writer<Stream>::String(++text, --length, copy);
        }
    }
};

} // namespace model

#endif
