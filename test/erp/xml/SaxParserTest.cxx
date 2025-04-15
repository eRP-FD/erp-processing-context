/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/util/SaxHandler.hxx"
#include "fhirtools/util/XmlHelper.hxx"

#include <functional>
#include <gtest/gtest.h>

class SaxParserTest
    : public ::testing::Test
    , public fhirtools::SaxHandler
{
public:
    void startDocument() override { if (onStartDocument) onStartDocument(); }
    void endDocument() override { if (onEndDocument) onEndDocument(); }
    void characters(const xmlChar* ch, int len) override { if (onCharacters) onCharacters(ch, len); }
    void startElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri,
                              int nbNamespaces, const xmlChar** namespaces, int nbDefaulted,
                              const AttributeList& attributes) override
    {
        if (onStartElement)
        {
            onStartElement(localname, prefix, uri, nbNamespaces, namespaces, nbDefaulted, attributes);
        }
    }

    void endElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri) override
    {
        if (onEndElement)
        {
            onEndElement(localname, prefix, uri);
        }
    }

    void error(const std::string& msg) override
    {
        if (onError)
        {
            onError(msg);
        }
    }

    ~SaxParserTest() override = default;

    std::function<void()> onStartDocument;
    std::function<void()> onEndDocument;
    std::function<void(const xmlChar* ch, int len)> onCharacters;
    std::function<void(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri,
                              int nbNamespaces, const xmlChar** namespaces, int nbDefaulted,
                              const AttributeList& attributes)> onStartElement;
    std::function<void(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri)> onEndElement;
    std::function<void(const std::string& msg)> onError;

};


TEST_F(SaxParserTest, regularCallbacks) // NOLINT
{
    bool startDocumentCalled = false;
    bool endDocumentCalled = false;
    bool charactersCalled = false;
    bool startElementCalled = false;
    bool endElementCalled = false;
    onStartDocument = [&]{startDocumentCalled = true;};
    onEndDocument = [&]{endDocumentCalled = true;};
    onCharacters = [&](const xmlChar*, int) { charactersCalled = true; };
    onStartElement =
        [&](const xmlChar*, const xmlChar*, const xmlChar*, int, const xmlChar**, int, const AttributeList&)
        {
            startElementCalled = true;
        };
    onEndElement =
        [&](const xmlChar*, const xmlChar*, const xmlChar*)
        {
            endElementCalled = true;
        };
    parseStringView("<root/>");
    EXPECT_TRUE(startDocumentCalled);
    EXPECT_TRUE(endDocumentCalled);
    EXPECT_FALSE(charactersCalled);
    EXPECT_TRUE(startElementCalled);
    EXPECT_TRUE(endElementCalled);
    startDocumentCalled = false;
    endDocumentCalled = false;
    charactersCalled = false;
    startElementCalled = false;
    endElementCalled = false;
    parseStringView("<root>characters</root>");
    EXPECT_TRUE(startDocumentCalled);
    EXPECT_TRUE(endDocumentCalled);
    EXPECT_TRUE(charactersCalled);
    EXPECT_TRUE(startElementCalled);
    EXPECT_TRUE(endElementCalled);
}


TEST_F(SaxParserTest, errorCallback) // NOLINT
{
    bool errorCalled = false;
    onError = [&](const std::string&) { errorCalled = true; };
    parseStringView("<root/>");
    EXPECT_FALSE(errorCalled);
    EXPECT_ANY_THROW(parseStringView("<root>"));
    EXPECT_TRUE(errorCalled);
}


TEST_F(SaxParserTest, AttributeList) // NOLINT
{
    //NOLINTNEXTLINE(readability-function-cognitive-complexity)
    onStartElement = [&](const xmlChar*, const xmlChar*, const xmlChar*, int, const xmlChar**, int, const AttributeList& attributes)
    {
        EXPECT_EQ(attributes.size(), 4);
        ASSERT_TRUE(attributes.findAttribute("attr1"));
        EXPECT_EQ(attributes.findAttribute("attr1")->value(), "value1");
        ASSERT_TRUE(attributes.findAttribute("attr2"));
        EXPECT_EQ(attributes.findAttribute("attr2")->value(), "value2");
        ASSERT_TRUE(attributes.findAttribute("attr3"));
        EXPECT_EQ(attributes.findAttribute("attr3")->value(), "value3");
        ASSERT_TRUE(attributes.findAttribute("attr4"));
        EXPECT_EQ(attributes.findAttribute("attr4")->value(), "value4");
        for (size_t i = 0; i < attributes.size(); ++i)
        {
            EXPECT_EQ(attributes.get(i).value(), "value" + std::to_string(i + 1));
        }
    };
    EXPECT_NO_FATAL_FAILURE(parseStringView(R"(<root attr1="value1" attr2="value2" attr3="value3" attr4="value4"/>)"));
}

TEST_F(SaxParserTest, Attribute) // NOLINT
{
    using namespace xmlHelperLiterals;
    bool hadNoNamespace = false;
    bool hadWithNamespace = false;
    //NOLINTNEXTLINE(readability-function-cognitive-complexity)
    onStartElement = [&](const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri, int, const xmlChar**, int, const AttributeList& attributes)
    {
        if (localname == "root"_xs)
        {
            EXPECT_EQ(attributes.size(), 0);
            return;
        }
        constexpr auto ns1Uri{"http://ns1"_xs};
        if (uri == ns1Uri)
        {
            hadWithNamespace = true;
            EXPECT_EQ(localname, "element1"_xs) << "localname:" << (localname?reinterpret_cast<const char*>(localname):"null");
            EXPECT_EQ(prefix, "ns1"_xs) << "prefix:" << (prefix?reinterpret_cast<const char*>(prefix):"null");
            ASSERT_EQ(attributes.size(), 1);
            auto attr0 = attributes.findAttribute("attr0");
            ASSERT_TRUE(attr0);
            EXPECT_EQ(attr0->value(), "value10");
            EXPECT_EQ(attr0->localname(), "attr0");
            EXPECT_FALSE(attr0->prefix().has_value());
            EXPECT_FALSE(attr0->uri().has_value());
            EXPECT_FALSE(attributes.findAttribute("attr0", ns1Uri).has_value());
        }
        else
        {
            hadNoNamespace  = true;
            ASSERT_EQ(uri, nullptr) << "URI:" << (uri?reinterpret_cast<const char*>(uri):"null");
            EXPECT_EQ(localname, "element0"_xs) << "localname:" << (localname?reinterpret_cast<const char*>(localname):"null");
            EXPECT_EQ(prefix, nullptr) << "prefix:" << (prefix?reinterpret_cast<const char*>(prefix):"null");
            ASSERT_EQ(attributes.size(), 2);
            auto attr0ns1 = attributes.findAttribute("attr0", ns1Uri);
            ASSERT_TRUE(attr0ns1);
            EXPECT_EQ(attr0ns1->value(), "value10");
            EXPECT_EQ(attr0ns1->localname(), "attr0");
            EXPECT_EQ(attr0ns1->prefix().value_or("null"), "ns1");
            EXPECT_EQ(attr0ns1->uri().value_or("null"), ns1Uri);
            auto attr0noNs = attributes.findAttribute("attr0");
            ASSERT_TRUE(attr0noNs);
            EXPECT_EQ(attr0noNs->value(), "value0");
            EXPECT_EQ(attr0noNs->localname(), "attr0");
            EXPECT_FALSE(attr0noNs->prefix().has_value());
            EXPECT_FALSE(attr0noNs->uri().has_value());
            EXPECT_FALSE(attributes.findAttribute("non_existent").has_value());
            EXPECT_FALSE(attributes.findAttribute("non_existent", ns1Uri).has_value());
        }
    };
    EXPECT_NO_FATAL_FAILURE(
        parseStringView(R"(
            <root xmlns:ns1="http://ns1">
                <element0 attr0="value0" ns1:attr0="value10"/>
                <ns1:element1 attr0="value10"/>
            </root>
        )")
    );

    EXPECT_TRUE(hadNoNamespace);
    EXPECT_TRUE(hadWithNamespace);
}
