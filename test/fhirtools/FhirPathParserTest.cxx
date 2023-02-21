/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */


#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/expression/Expression.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "test/fhirtools/DefaultFhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <filesystem>

using namespace fhirtools;

class FhirPathParserTest : public testing::Test
{
public:
    FhirPathParserTest()

    {
        testResource = model::NumberAsStringParserDocument::fromJson(
            ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));

        rootElement = std::make_shared<ErpElement>(&repo, std::weak_ptr<Element>{}, repo.findTypeById("Test"), "Test",
                                                   &testResource);
    }
    void checkBoolResult(const Collection& result, bool expected)
    {
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Boolean);
        ASSERT_EQ(result[0]->asBool(), expected) << result;
    }
    const FhirStructureRepository& repo = DefaultFhirStructureRepository::getWithTest();
    model::NumberAsStringParserDocument testResource;
    std::shared_ptr<ErpElement> rootElement;
};

class FhirPathParserTestCommunication : public FhirPathParserTest
{
public:
    FhirPathParserTestCommunication()
        : communicationResource(model::Communication::fromXmlNoValidation(
              ResourceManager::instance().getStringResource("test/fhir/conversion/communication_info_req.xml")))
    {
        //NOLINTNEXTLINE(readability-qualified-auto) - make sure the return type is reference
        auto& testCommDoc = communicationResource.jsonDocument();
        communicationElement =
            std::make_shared<ErpElement>(&repo, std::weak_ptr<Element>{}, "Communication", &testCommDoc);
    }
    model::Communication communicationResource;
    std::shared_ptr<ErpElement> communicationElement;
};

class FhirPathParserTestBundle : public FhirPathParserTest
{
public:
    FhirPathParserTestBundle()
        : bundleResource(model::KbvBundle::fromXmlNoValidation(ResourceManager::instance().getStringResource(
              "test/validation/xml/kbv/bundle/Bundle_valid_fromErp5822.xml")))
    {
        //NOLINTNEXTLINE(readability-qualified-auto) - make sure the return type is reference
        auto& testDoc = bundleResource.jsonDocument();
        bundleElement = std::make_shared<ErpElement>(&repo, std::weak_ptr<Element>{}, "Bundle", &testDoc);
    }
    model::KbvBundle bundleResource;
    std::shared_ptr<ErpElement> bundleElement;
};

TEST_F(FhirPathParserTest, exists)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "string.exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "Test.string.exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "x.exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
}

TEST_F(FhirPathParserTest, indexer)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "multiString[0].exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "multiString[1].exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "multiString[2].exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
}

TEST_F(FhirPathParserTest, unionOperator)
{
    auto expressions = FhirPathParser::parse(&repo, "multiNum | Test.multiDec");
    ASSERT_TRUE(expressions);
    auto result = expressions->eval({rootElement});
    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0]->asInt(), 1);
    EXPECT_EQ(result[1]->asInt(), 5);
    EXPECT_EQ(result[2]->asInt(), 42);
    EXPECT_EQ(result[3]->asDecimal(), DecimalType("0.1"));
    EXPECT_EQ(result[4]->asDecimal(), DecimalType("3.14"));
}

TEST_F(FhirPathParserTest, orOperator)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "multiNum.exists() or Test.x.exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "y.exists() or Test.x.exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "multiNum.exists() or Test.string.exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
}
TEST_F(FhirPathParserTest, xorOperator)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "multiNum.exists() xor Test.x.exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "y.exists() xor Test.x.exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "multiNum.exists() xor Test.string.exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
}
TEST_F(FhirPathParserTest, andOperator)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "multiNum.exists() and Test.x.exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "y.exists() and Test.x.exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "multiNum.exists() and Test.string.exists()");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
}

TEST_F(FhirPathParserTest, in)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "3.14 in Test.multiDec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "3 in Test.multiDec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
}

TEST_F(FhirPathParserTest, contains)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "Test.multiString contains 'value'");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "multiString contains 'hello'");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
}

TEST_F(FhirPathParserTest, greaterEquals)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "2 >= dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "1.2 >= dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "1.1 >= dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, " dec >= num");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
}

TEST_F(FhirPathParserTest, greater)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "2 > dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "1.2 > dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "1.1 > dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, " dec > num");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
}

TEST_F(FhirPathParserTest, less)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "2 < dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "1.2 < dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "1.1 < dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, " dec < num");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
}

TEST_F(FhirPathParserTest, lessEquals)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "2 <= dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "1.2 <= dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "1.1 <= dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, " dec <= num");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
}

TEST_F(FhirPathParserTest, eq)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "1.2 = dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "1.1 = dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "Test.string = 'value'");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "Test.string = ''");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
}

TEST_F(FhirPathParserTest, neq)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "1.2 != dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "1.1 != dec");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "Test.string != 'value'");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "Test.string != ''");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
}

TEST_F(FhirPathParserTest, is)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "dec is `http://hl7.org/fhirpath/System.Decimal`");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, true));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "dec is Patient");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        EXPECT_NO_FATAL_FAILURE(checkBoolResult(result, false));
    }
}

TEST_F(FhirPathParserTest, as)
{
    {
        auto expressions = FhirPathParser::parse(&repo, "dec as `http://hl7.org/fhirpath/System.Decimal`");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result.single()->asDecimal(), DecimalType("1.2"));
    }
    {
        auto expressions = FhirPathParser::parse(&repo, "dec as `http://hl7.org/fhirpath/System.Integer`");
        ASSERT_TRUE(expressions);
        auto result = expressions->eval({rootElement});
        ASSERT_EQ(result.size(), 0);
    }
}

TEST_F(FhirPathParserTest, compileError)
{
    std::string_view expression =
        R"(conformsTo("http://fhir.de/StructureDefinition/identifier-iknr") implies value.matches('[0-9]{9}'))";
    ASSERT_THROW((void)FhirPathParser::parse(&repo, expression), std::logic_error);
}

TEST_F(FhirPathParserTestCommunication, testele_1)
{
    const char constraint[] = "hasValue() or (children().count() > id.count())";

    auto expressions = FhirPathParser::parse(&repo, constraint);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval({communicationElement});
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asBool(), true);
}

TEST_F(FhirPathParserTestCommunication, testext_1)
{
    const char constraint[] = "extension.exists() != value.exists()";

    auto expressions = FhirPathParser::parse(&repo, constraint);
    ASSERT_TRUE(expressions);
    Collection in{communicationElement->subElements("contained")[0]->subElements("extension")};
    std::ostringstream inStr;
    in.json(inStr);
    auto result = expressions->eval(in);
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asBool(), true) << inStr.str() << " -> " << result;
}

TEST_F(FhirPathParserTestCommunication, testref_1)
{
    const char constraint[] = "reference.startsWith('#').not() or (reference.substring(1).trace('url') in "
                            "%rootResource.contained.id.trace('ids'))";

    auto expressions = FhirPathParser::parse(&repo, constraint);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval(communicationElement->subElements("basedOn"));
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asBool(), true);
}

TEST_F(FhirPathParserTestCommunication, testref_1_2)
{
    const char constraint[] = "reference.startsWith('#').not() or (reference.substring(1).trace('url') in "
                            "%rootResource.contained.id.trace('ids'))";

    auto expressions = FhirPathParser::parse(&repo, constraint);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval(communicationElement->subElements("about"));
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asBool(), true);
}

TEST_F(FhirPathParserTestCommunication, test_dom_3)
{
    const char constraint[] =
        "contained.where((('#'+id in (%resource.descendants().reference | %resource.descendants().as(canonical) | "
        "%resource.descendants().as(uri) | %resource.descendants().as(url))) "
        "or descendants().where(reference = "
        "'#').exists() or descendants().where(as(canonical) = '#').exists() or descendants().where(as(canonical) = "
        "'#').exists()).not()).trace('unmatched', id).empty()";
    auto expressions = FhirPathParser::parse(&repo, constraint);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval({communicationElement});
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asBool(), true);
}

TEST_F(FhirPathParserTestCommunication, resolve)
{
    const char constraint[] = "contained.contains(about.resolve())";
    auto expressions = FhirPathParser::parse(&repo, constraint);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval({communicationElement});
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asBool(), true);
}

TEST_F(FhirPathParserTestCommunication, resolveid)
{
    const char constraint[] = "about.resolve().id = '5fe6e06c-8725-46d5-aecd-e65e041ca3de'";
    auto expressions = FhirPathParser::parse(&repo, constraint);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval({communicationElement});
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asBool(), true);
}

TEST_F(FhirPathParserTestBundle, test_erp_compositionPflicht)
{
    const char constraint[] = "entry.where(resource is Composition).count()=1";
    auto expressions = FhirPathParser::parse(&repo, constraint);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval({bundleElement});
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asBool(), true);
}

TEST_F(FhirPathParserTestBundle, test_erp_begrenzungDate)
{
    const char constraint[] = "authoredOn.toString().length()=10";
    auto expressions = FhirPathParser::parse(&repo, constraint);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval(bundleElement->subElements("entry")[1]->subElements("resource"));
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asBool(), true);
}

TEST_F(FhirPathParserTestBundle, test_erp_angabeDosierung)
{
    const char constraint[] = "(extension('https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_DosageFlag').empty() or "
                            "extension('https://fhir.kbv.de/StructureDefinition/"
                            "KBV_EX_ERP_DosageFlag').value.as(Boolean)=false) implies text.empty()";
    auto expressions = FhirPathParser::parse(&repo, constraint);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval(
        bundleElement->subElements("entry")[1]->subElements("resource")[0]->subElements("dosageInstruction"));
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asBool(), true);
}

TEST_F(FhirPathParserTestBundle, test_org_1)
{
    const char constraint[] = "(identifier.count() + name.count()) > 0";
    auto expressions = FhirPathParser::parse(&repo, constraint);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval(bundleElement->subElements("entry")[4]->subElements("resource"));
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asBool(), true);
}

TEST_F(FhirPathParserTestBundle, resolve)
{
    const char expression[] =
        "entry.where(fullUrl = 'https://e-rezept.de/Composition/7d101376-cafa-43d8-bae9-a2b4511b1bb7')"
            ".resource.as(Composition).subject.resolve()"
        " = "
        "entry.where(fullUrl = 'https://e-rezept.de/Patient/66251aee-a81a-40ca-8938-79d8557b822c')"
            ".resource.as(Patient)";
    auto expressions = FhirPathParser::parse(&repo, expression);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval({bundleElement});
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asBool(), true);
}

TEST_F(FhirPathParserTestBundle, ofTypePatient)
{
    const char expression[] = "entry.resource.ofType(Patient)";
    auto expressions = FhirPathParser::parse(&repo, expression);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval({bundleElement});
    ASSERT_EQ(result.size(), 1);
}

TEST_F(FhirPathParserTestBundle, ofTypeString)
{
    const char expression[] = "ofType(string)";
    auto expressions = FhirPathParser::parse(&repo, expression);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello"),
                                     std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 2)});
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asString(), "hello");
}

TEST_F(FhirPathParserTestBundle, ofTypeInteger)
{
    const char expression[] = "ofType(positiveInt)";
    auto expressions = FhirPathParser::parse(&repo, expression);
    ASSERT_TRUE(expressions);
    auto result = expressions->eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello"),
                                     std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 2)});
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asInt(), 2);
}

class FhirPathParserTestP : public FhirPathParserTest,
                            public ::testing::WithParamInterface<std::pair<std::string, std::string>>
{
};
using FhirPathParserTestP2 = FhirPathParserTestP;

TEST_P(FhirPathParserTestP, unescapeStringLiteralExpression)
{
    auto expressions = FhirPathParser::parse(&repo, GetParam().first);
    ASSERT_TRUE(expressions);
    const auto col =
        expressions->eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, GetParam().second)});
    ASSERT_EQ(col.size(), 1);
    EXPECT_EQ(col.single()->asString(), GetParam().second);
}

TEST_P(FhirPathParserTestP2, unescapeStringLiteral)
{
    auto result = FhirPathParser::unescapeStringLiteral(GetParam().first, GetParam().first[0]);
    EXPECT_EQ(result, GetParam().second);
}

INSTANTIATE_TEST_SUITE_P(StringLiteral, FhirPathParserTestP,
                         ::testing::Values(std::make_pair(R"('\'')", "'"), std::make_pair(R"('\"')", R"(")"),
                                           std::make_pair(R"('\`')", "`"), std::make_pair(R"('\r')", "\r"),
                                           std::make_pair(R"('\n')", "\n"), std::make_pair(R"('\t')", "\t"),
                                           std::make_pair(R"('\f')", "\f"), std::make_pair(R"('\\')", R"(\)"),
                                           std::make_pair(R"('\u0444')", "\u0444"), std::make_pair(R"('\p')", "p"),
                                           std::make_pair(R"('\\p')", R"(\p)"), std::make_pair(R"('\3')", "3"),
                                           std::make_pair(R"('\u005')", "u005"), std::make_pair(R"('\')", ""),
                                           std::make_pair(R"('^\\d{8}$')", "^\\d{8}$")));

INSTANTIATE_TEST_SUITE_P(StringLiteral, FhirPathParserTestP2,
                         ::testing::Values(std::make_pair(R"(`\'`)", "'"), std::make_pair(R"(`\"`)", R"(")"),
                                           std::make_pair(R"(`\``)", "`"), std::make_pair(R"(`\r`)", "\r"),
                                           std::make_pair(R"(`\n`)", "\n"), std::make_pair(R"(`\t`)", "\t"),
                                           std::make_pair(R"(`\f`)", "\f"), std::make_pair(R"(`\\`)", R"(\)"),
                                           std::make_pair(R"(`\u0444`)", "\u0444"), std::make_pair(R"(`\p`)", "p"),
                                           std::make_pair(R"(`\\p`)", R"(\p)"), std::make_pair(R"(`\3`)", "3"),
                                           std::make_pair(R"(`\u005`)", "u005"), std::make_pair(R"(`\`)", ""),
                                           std::make_pair(R"('\'')", "'"), std::make_pair(R"('\"')", R"(")"),
                                           std::make_pair(R"('\`')", "`"), std::make_pair(R"('\r')", "\r"),
                                           std::make_pair(R"('\n')", "\n"), std::make_pair(R"('\t')", "\t"),
                                           std::make_pair(R"('\f')", "\f"), std::make_pair(R"('\\')", R"(\)"),
                                           std::make_pair(R"('\u0444')", "\u0444"), std::make_pair(R"('\p')", "p"),
                                           std::make_pair(R"('\\p')", R"(\p)"), std::make_pair(R"('\3')", "3"),
                                           std::make_pair(R"('\u005')", "u005"), std::make_pair(R"('\')", "")));
