/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/xml/XmlDocument.hxx"
#include "erp/util/Expect.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "fhirtools/util/SaxHandler.hxx"

#include <libxml/xmlschemas.h>
#include <libxml/xpathInternals.h>
#include <sstream>

XmlDocument::XmlDocument (const std::string_view xml)
    : mDocument(),
      mXPathContext(),
      mPathHead()
{
    auto parserContext = std::shared_ptr<xmlParserCtxt>(xmlNewParserCtxt(), xmlFreeParserCtxt);
    auto oldEntityLoader = xmlGetExternalEntityLoader();
    xmlSetExternalEntityLoader([](const char* URL, const char* /*ID*/, xmlParserCtxtPtr /*ctxt*/) -> xmlParserInputPtr {
        LOG(WARNING) << "Suppressing external entity loading from " << URL;
        return nullptr;
    });
    mDocument = std::shared_ptr<xmlDoc>(
        xmlCtxtReadMemory(
            parserContext.get(),
            xml.data(),
            gsl::narrow_cast<int>(xml.size()),
            nullptr,
            nullptr,
            XML_PARSE_DTDATTR),
        xmlFreeDoc);
    xmlSetExternalEntityLoader(oldEntityLoader);

    if (mDocument==nullptr)
    {
        std::ostringstream s;
        s << "xml could not be parsed, error " << std::to_string(parserContext->errNo);
        const auto* xmlError = xmlCtxtGetLastError(parserContext.get());
        if (xmlError != nullptr)
            s << ", at line " << xmlError->line;
        Expect(mDocument!=nullptr, s.str());
    }

    mXPathContext = std::shared_ptr<xmlXPathContext>(xmlXPathNewContext(mDocument.get()), xmlXPathFreeContext);
    if (mXPathContext == nullptr)
    {
        Fail2("can not create XPath context", std::runtime_error);
    }
}

void XmlDocument::validateAgainstSchema(XmlValidatorContext& validatorContext) const
{
    Expect(validatorContext.mXmlSchemaValidCtxt != nullptr, "The context must be set.");
    if (0 != xmlSchemaValidateDoc(validatorContext.mXmlSchemaValidCtxt.get(), mDocument.get()))
    {
        Fail2("TSL schema validation error", std::runtime_error);
    }
}

std::shared_ptr<xmlXPathObject> XmlDocument::getXpathObject (
    const std::string& xPathExpression,
    xmlXPathContext& xpathContext)
{
    try
    {
        auto xpathObject = std::shared_ptr<xmlXPathObject>(
            xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(xPathExpression.c_str()), &xpathContext),
            xmlXPathFreeObject);
        if (xpathObject == nullptr)
        {
            Fail2("can not evaluate XPath expression", std::runtime_error);
        }
        if (xpathObject->nodesetval == nullptr)
        {
            Fail2("did not find element", std::runtime_error);
        }

        const auto resultCount = xpathObject->nodesetval->nodeNr;
        if (resultCount <= 0)
        {
            Fail2("did not find the requested element", std::runtime_error);
        }
        else if (resultCount > 1)
        {
            Fail2("found more than one element", std::runtime_error);
        }

        return xpathObject;
    }
    catch (const std::runtime_error& e)
    {
        Fail2(std::string(e.what()) + " when processing XPath expression " + xPathExpression, std::runtime_error);
    }
}


std::shared_ptr<xmlXPathObject> XmlDocument::getXpathObject (const std::string& xPathExpression) const
{
    return getXpathObject(mPathHead + xPathExpression, *mXPathContext);
}


std::shared_ptr<xmlXPathObject> XmlDocument::getXpathObjects (const std::string& xPathExpression) const
{
    try
    {
        auto xpathObject = std::shared_ptr<xmlXPathObject>(
            xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>((mPathHead+xPathExpression).c_str()), mXPathContext.get()),
            xmlXPathFreeObject);
        if (xpathObject == nullptr)
        {
            Fail2("can not evaluate XPath expression", std::runtime_error);
        }
        if (xpathObject->nodesetval == nullptr)
        {
            Fail2("did not find element", std::runtime_error);
        }

        return xpathObject;
    }
    catch (const std::runtime_error& e)
    {
        Fail2(std::string(e.what()) + " when processing XPath expression " + xPathExpression, std::runtime_error);
    }
}


bool XmlDocument::hasElement (const std::string& xPathExpression) const
{
    auto xpathObject = std::shared_ptr<xmlXPathObject>(
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(xPathExpression.c_str()), mXPathContext.get()),
        xmlXPathFreeObject);

    if (xpathObject == nullptr)
    {
        Fail2("can not evaluate XPath expression", std::runtime_error);
    }

    if (xpathObject->nodesetval == nullptr)
        return false;

    return xpathObject->nodesetval->nodeNr > 0;
}


std::string XmlDocument::getElementText (
    const std::string& xPathExpression) const
{
    auto xpathObject = getXpathObject(xPathExpression);

    xmlNodePtr node = xpathObject->nodesetval->nodeTab[0];
    if (node==nullptr || node->content==nullptr)
    {
        Fail2("empty result", std::runtime_error);
    }

    return std::string(reinterpret_cast<const char*>(node->content));
}

std::optional<std::string> XmlDocument::getOptionalElementText (const std::string& xPathExpression) const
{
    const auto xpathObject = std::shared_ptr<xmlXPathObject>(
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(
                                   (mPathHead + xPathExpression).c_str()),
                               mXPathContext.get()),
        xmlXPathFreeObject);

    if (!xpathObject)
    {
        throw std::runtime_error{
            "can not evaluate XPath expression when processing XPath expression " + xPathExpression};
    }

    if (!xpathObject->nodesetval)
    {
        return std::nullopt;
    }

    const auto count = xpathObject->nodesetval->nodeNr;
    if (!count)
    {
        return std::nullopt;
    }

    if (count > 1)
    {
        throw std::runtime_error{
            "more than one result when processing XPath expression " + xPathExpression};
    }

    const auto* node = xpathObject->nodesetval->nodeTab[0];
    if (!node || !node->content)
    {
        throw std::runtime_error{
            "empty result when processing XPath expression " + xPathExpression};
    }

    return std::string(reinterpret_cast<const char*>(node->content));
}

xmlNodePtr XmlDocument::getNode (
    const std::string& xPathExpression) const
{
    auto xpathObject = getXpathObject(xPathExpression);

    return xpathObject->nodesetval->nodeTab[0];
}


xmlNodeSetPtr XmlDocument::getNodes (
    const std::string& xPathExpression) const
{
    auto xpathObject = getXpathObject(xPathExpression);

    return xpathObject->nodesetval;
}


std::string XmlDocument::getAttributeValue(
    const std::string& xPathExpression) const
{
    auto xpathObject = getXpathObject(xPathExpression);

    xmlNodePtr node = xpathObject->nodesetval->nodeTab[0];
    if (node==nullptr || node->children==nullptr || node->children->content==nullptr)
    {
        Fail2("empty result", std::runtime_error);
    }

    return std::string(reinterpret_cast<const char*>(node->children->content));
}


xmlDoc& XmlDocument::getDocument (void)
{
    Expects(mDocument != nullptr);
    return *mDocument;
}



void XmlDocument::registerNamespace (const char* prefix, const char* href)
{
    xmlXPathRegisterNs(mXPathContext.get(), reinterpret_cast<const xmlChar*>(prefix), reinterpret_cast<const xmlChar*>(href));
}
