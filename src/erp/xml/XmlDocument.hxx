/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SOAP_XMLDOCUMENT_HXX
#define ERP_PROCESSING_CONTEXT_SOAP_XMLDOCUMENT_HXX

#include <libxml/xpathInternals.h>

#include <memory>
#include <optional>
#include <string>

struct XmlValidatorContext;

/**
 * Convenience wrapper around libxml2 DOM parser and XPath access.
 */
class XmlDocument
{
public:
    explicit XmlDocument (const std::string_view xml);

    static std::shared_ptr<xmlXPathObject> getXpathObject (
        const std::string& xPathExpression,
        xmlXPathContext& xpathContext);

    /**
     * Return a single object that matches the given `xPathExpression`. No match or more than
     * one match are regarded as errors and result in a `std::runtime_error` exception.
     */
    std::shared_ptr<xmlXPathObject> getXpathObject (const std::string& xPathExpression) const;
    /**
     * Return zero, one or more nodes that match the given `xPathExpression`. A non-match
     * is not regarded as error and therefore must be handled by the caller.
     */
    std::shared_ptr<xmlXPathObject> getXpathObjects (const std::string& xPathExpression) const;

    bool hasElement (const std::string& xPathExpression) const;
    std::string getElementText (const std::string& xPathExpression) const;
    std::optional<std::string> getOptionalElementText (const std::string& xPathExpression) const;
    xmlNodePtr getNode (const std::string& xPathExpression) const;
    xmlNodeSetPtr getNodes (const std::string& xPathExpression) const;
    std::string getAttributeValue (const std::string& xPathExpression) const;

    xmlDoc& getDocument (void);

    void registerNamespace(const char* prefix, const char* href);

    void validateAgainstSchema(XmlValidatorContext& validatorContext) const;

private:
    std::shared_ptr<xmlDoc> mDocument;
    std::shared_ptr<xmlXPathContext> mXPathContext;
    const std::string mPathHead;
    const std::string mXsdPath;
};


#endif
