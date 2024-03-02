/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/util/SaxHandler.hxx"
#include "erp/util/String.hxx"
#include "erp/xml/XmlMemory.hxx"

#include <libxml/parserInternals.h>
#include <libxml/xmlschemas.h>
#include <fstream>
#include <map>
#include <utility>

#include "VaListHelper.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/util/Gsl.hxx"

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

// for vsnprintf in error(...):
#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
using fhirtools::SaxHandler;

namespace
{
template<typename Ret, typename... Args>
struct Callback {
    // Type of the actual Callback
    using Type = Ret (*)(void*, Args...);

    // Subclass to call SaxParser member function
    template<auto memberFunctionPointer>
    struct FunctionMakerClass {
        static Ret cStyleCallbackFunction(void* self, Args... args)
        {
            try
            {
                Expect3(self != nullptr, "callback function data is null.", std::invalid_argument);
                (static_cast<SaxHandler*>(self)->*memberFunctionPointer)(std::forward<Args>(args)...);
            }
            catch (...)
            {
                // store the exception for later rethrow.
                // to avoid memory leaks in libxml2 library
                static_cast<SaxHandler*>(self)->mExceptionPtr = std::current_exception();
                TVLOG(1) << "stopping SAX parser now";
                xmlStopParser(static_cast<SaxHandler*>(self)->mContext.get());
            }
        }
    };
};

// Function to derive return type and Arguments of callback
template<typename Ret, typename... Args>
[[maybe_unused]] Callback<Ret, Args...> toCallbackClass(Ret (SaxHandler::*)(Args...));

template<auto memberFunctionPointer>
typename decltype(toCallbackClass(memberFunctionPointer))::Type makeCallback()
{
    using CallbackClass = decltype(toCallbackClass(memberFunctionPointer));
    using CallbackFunctionMakerClass = typename CallbackClass::template FunctionMakerClass<memberFunctionPointer>;
    return &CallbackFunctionMakerClass::cStyleCallbackFunction;
}

void failMaybeWithDiagnostics(const std::string& message, const xmlError& error)
{
    if (error.message)
    {
        std::string errorMessage = error.message;
        if (errorMessage.back() == '\n')// remove trailing line feed contained in message from libxml2;
            errorMessage = errorMessage.substr(0, errorMessage.size() - 1);
        ErpFailWithDiagnostics(HttpStatus::BadRequest, message, errorMessage);
    }
    ErpFail(HttpStatus::BadRequest, message);
}

void genericSuppressingErrorCallback(void*, const char*, ...)
{
}

}// anonymous namespace

std::string_view SaxHandler::Attribute::localname() const
{
    return mData[0];
}

std::optional<std::string_view> SaxHandler::Attribute::prefix() const
{
    return (mData[1] != nullptr) ? std::optional<std::string_view>(mData[1]) : std::optional<std::string_view>{};
}

std::optional<std::string_view> SaxHandler::Attribute::uri() const
{
    return (mData[2] != nullptr) ? std::optional<std::string_view>(mData[2]) : std::optional<std::string_view>{};
}

std::string_view SaxHandler::Attribute::value() const
{
    const auto* begin = mData[3];
    const auto* end = mData[4];
    return std::string_view(begin, static_cast<size_t>(end - begin));
}

template<typename PredicateT>
std::optional<SaxHandler::Attribute> SaxHandler::AttributeList::findAttibuteIf(const PredicateT& predicate) const
{
    if (mAttributes)
    {
        auto* const end = mAttributes + (mSize * 5);
        for (auto* attrPtr = mAttributes; attrPtr != end; attrPtr += 5)
        {
            Attribute attr{attrPtr};
            if (predicate(attr))
            {
                return attr;
            }
        }
    }

    // Due to what may be a compiler bug in GCC (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80635) the return of
    // an empty optional is a bit longer than usual.
    std::optional<SaxHandler::Attribute> empty;
    return empty;
}


std::optional<SaxHandler::Attribute> SaxHandler::AttributeList::findAttribute(const std::string_view& localName) const
{
    return findAttibuteIf([&](const Attribute& attr) {
        return (attr.localname() == localName && ! attr.uri().has_value());
    });
}


std::optional<SaxHandler::Attribute>
SaxHandler::AttributeList::findAttribute(const std::string_view& localName, const std::string_view& namespaceURI) const
{
    return findAttibuteIf([&](const Attribute& attr) {
        auto uri = attr.uri();
        return (attr.localname() == localName && uri.has_value() && uri.value() == namespaceURI);
    });
}


SaxHandler::Attribute SaxHandler::AttributeList::get(size_t index) const
{
    Expect3(index < mSize, "Attribute list index out of range.", std::out_of_range);
    return Attribute{mAttributes + 5 * index};
}


SaxHandler::SaxHandler()
{
    // The callbacks must not throw exceptions through the libxml2 library to avoid memory leaks.
    mHandler.initialized = XML_SAX2_MAGIC;
    mHandler.startDocument = makeCallback<&SaxHandler::startDocument>();
    mHandler.endDocument = makeCallback<&SaxHandler::endDocument>();
    mHandler.characters = makeCallback<&SaxHandler::characters>();
    mHandler.startElementNs = makeCallback<&SaxHandler::startElementInternal>();
    mHandler.endElementNs = makeCallback<&SaxHandler::endElement>();
    mHandler.error = &SaxHandler::cErrorCallback;
}


SaxHandler::~SaxHandler() = default;

void SaxHandler::parseStringView(const std::string_view& xmlDocument)
{
    parseStringViewInternal(mHandler, xmlDocument, this);
}

void SaxHandler::validateStringView(const std::string_view& xmlDocument, const XmlValidatorContext& schemaValidationContext)
{
    xmlSAXHandler handler{};
    xmlSAXHandlerPtr handlerPtr = &handler;
    handler.initialized = XML_SAX2_MAGIC;
    handler.error = &SaxHandler::cErrorCallback;

    void* userData = this;
    std::unique_ptr<xmlSchemaSAXPlugStruct, std::function<void(xmlSchemaSAXPlugPtr)>> plugPtr(
        ::xmlSchemaSAXPlug(schemaValidationContext.mXmlSchemaValidCtxt.get(), &handlerPtr, &userData),
        [](xmlSchemaSAXPlugPtr ptr) {
            Expect(::xmlSchemaSAXUnplug(ptr) == 0, "could not unregister schema plugin from SAX parser");
        });

    Expect(plugPtr, "could not register schema plugin for SAX parser");
    parseStringViewInternal(*handlerPtr, xmlDocument, userData);

    if (schemaValidationContext.mExceptionPtr)
    {
        TVLOG(2) << "The following document had validation errors";
        TVLOG(2) << xmlDocument;
        std::rethrow_exception(schemaValidationContext.mExceptionPtr);
    }
}

void SaxHandler::parseAndValidateStringView(const std::string_view& xmlDocument,
                                            XmlValidatorContext& schemaValidationContext)
{
    // We decided to run the SAX parser twice.
    // The first time with only the xsd validation plugin installed,
    // the second time with only the FHIR SAX Handler callbacks installed.
    // We could do it in one run, but the FHIR SAX Handler callbacks are implemented with the
    // premise in mind, that the incoming data is already validated. I.e. they may be not 100% fail safe.
    // libxml2 has the behaviour, that it first calls the registered callbacks, and afterwards does
    // the xsd validation.

    validateStringView(xmlDocument, schemaValidationContext);

    parseStringView(xmlDocument);
}

void SaxHandler::parseStringViewInternal(xmlSAXHandler& handler, const std::string_view& xmlDocument, void* userData)
{
    ModelExpect(!xmlDocument.empty(), "xml document must not be empty");
    // This function is in principle a copy of xmlSAXUserParseMemory, but with auto ptr memory management.

    xmlInitParser();

    mExceptionPtr = nullptr;

    mContext = decltype(mContext)(xmlCreateMemoryParserCtxt(xmlDocument.data(), static_cast<int>(xmlDocument.size())),
                                  [](xmlParserCtxtPtr ptr) {
                                      // otherwise the context tries to free the handler, although it did not allocate it:
                                      ptr->sax = nullptr;
                                      xmlFreeParserCtxt(ptr);
                                  });
    Expect(mContext != nullptr, "Could not create xmlParserCtxtPtr");

    {// from xmlDetectSAX2
        mContext->sax2 = 1;
        mContext->str_xml = xmlDictLookup(mContext->dict, BAD_CAST "xml", 3);
        Expect(mContext->str_xml != nullptr, "Could not lookup ctxt->str_xml");
        mContext->str_xmlns = xmlDictLookup(mContext->dict, BAD_CAST "xmlns", 5);
        Expect(mContext->str_xmlns != nullptr, "Could not lookup str_xmlns");
        mContext->str_xml_ns = xmlDictLookup(mContext->dict, XML_XML_NAMESPACE, 36);
        Expect(mContext->str_xml_ns != nullptr, "Could not lookup str_xml_ns");
    }

    mContext->userData = userData;

    if (mContext->sax)
    {
        xmlFree(mContext->sax);
    }

    mContext->sax = &handler;
    auto oldEntityLoader = xmlGetExternalEntityLoader();
    xmlCtxtUseOptions(mContext.get(), XML_PARSE_NOENT);
    xmlSetExternalEntityLoader([](const char* /*URL*/, const char* /*ID*/, xmlParserCtxtPtr /*ctxt*/) -> xmlParserInputPtr {
        return nullptr;
    });
    // structuredErrorFunc may take priority, so reset it and install our own error callback
    xmlSetStructuredErrorFunc(this, structuredErrorCallback);
    xmlSetGenericErrorFunc(nullptr, genericSuppressingErrorCallback);

    int parseResult = 0;
    try
    {
        parseResult = xmlParseDocument(mContext.get());
    }
    catch (...)
    {
        // if exceptions are thrown through xmlParseDocument (from a callback in our code)
        // we have potential memory leaks, each callback must catch and store exceptions before
        // returning the control back to the libxml2 library.
        TLOG(ERROR) << "internal error: one of the XML callbacks is missing a try/catch, which potentially leads to "
                      "memory leaks.";
        std::rethrow_exception(std::current_exception());
    }
    xmlSetExternalEntityLoader(oldEntityLoader);

    if (mExceptionPtr)
    {
        std::rethrow_exception(mExceptionPtr);
    }

    if (mContext->errNo != 0 && mContext->lastError.level > XML_ERR_WARNING)
    {
        TVLOG(1) << "libxml2 ctxt->errNo: " << mContext->errNo;
        failMaybeWithDiagnostics("Error from xmlParseDocument", mContext->lastError);
    }
    if (parseResult != 0)
    {
        failMaybeWithDiagnostics("Error from xmlParseDocument", mContext->lastError);
    }
    if (! mContext->wellFormed)
    {
        failMaybeWithDiagnostics("Error from xmlParseDocument: !ctxt->wellFormed", mContext->lastError);
    }
}

void SaxHandler::parseFile(const std::filesystem::path& fileName)
{
    std::ifstream stream(fileName, std::ios::binary);
    if (! stream.good())
    {
        Fail("can not find or open for reading the file " + fileName.string());
    }
    std::vector<char> buffer(std::istreambuf_iterator<char>(stream), {});
    parseStringView(std::string_view{buffer.data(), buffer.size()});
}


void SaxHandler::cErrorCallback(void* self, const char* msg, ...)
{
    try
    {
        std::va_list args{}; //NOLINT(cppcoreguidelines-pro-type-vararg)
        va_start(args, msg);
        const auto errStr = fhirtools::VaListHelper::vaListToString(msg, args);
        va_end(args);
        static_cast<SaxHandler*>(self)->error(errStr);
    }
    catch (...)
    {
        static_cast<SaxHandler*>(self)->mExceptionPtr = std::current_exception();
    }
}

void SaxHandler::structuredErrorCallback(void* self, xmlErrorPtr err)
{
    try
    {
        std::ostringstream s;
        s << "error parsing xml, message: " << String::trim(err->message) << ", code: " << err->code
          << ", line: " << err->line;
        static_cast<SaxHandler*>(self)->error(s.str());
    }
    catch (...)
    {
        static_cast<SaxHandler*>(self)->mExceptionPtr = std::current_exception();
    }
}

void SaxHandler::characters(const xmlChar* ch, int len)
{
    (void) ch;
    (void) len;
}

void SaxHandler::startElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri, int nbNamespaces,
                              const xmlChar** namespaces, int nbDefaulted, const SaxHandler::AttributeList& attributes)
{
    (void) localname;
    (void) prefix;
    (void) uri;
    (void) nbNamespaces;
    (void) namespaces;
    (void) nbDefaulted;
    (void) attributes;
}

void SaxHandler::startElementInternal(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri,
                                      int nbNamespaces, const xmlChar** namespaces, int nbAttributes, int nbDefaulted,
                                      const xmlChar** attributes)
{
    startElement(localname, prefix, uri, nbNamespaces, namespaces, nbDefaulted,
                 AttributeList(attributes, gsl::narrow<size_t>(nbAttributes)));
}

void SaxHandler::endElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri)
{
    (void) localname;
    (void) prefix;
    (void) uri;
}

void SaxHandler::error(const std::string& msg)
{
    TVLOG(1) << msg;
}
