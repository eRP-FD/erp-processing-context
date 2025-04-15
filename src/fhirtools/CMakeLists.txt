
add_library(fhirtools STATIC)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(antlr4-runtime REQUIRED)
find_package(date REQUIRED)
find_package(glog REQUIRED)
find_package(gsl-lite REQUIRED)
find_package(libxml2 REQUIRED)
find_package(magic_enum REQUIRED)

target_link_libraries(fhirtools
    PRIVATE
        erp-shared
        antlr4_static
    PUBLIC
        boost::boost
        date::date
        glog::glog
        gsl::gsl-lite
        magic_enum::magic_enum
        rapidjson
        LibXml2::LibXml2
        openssl::openssl # only needed because we include Expect.hxx from erp
    )

antlr_target(fhirtools fhirpath.g4 PARSER LEXER VISITOR PACKAGE fhirtools)
target_include_directories(fhirtools
    PRIVATE
        "${ANTLR_fhirtools_OUTPUT_DIR}"
)

add_custom_target(generated_source DEPENDS ${ANTLR_fhirtools_CXX_OUTPUTS})

if (CMAKE_COMPILER_IS_GNUCXX)
    set_property(SOURCE ${ANTLR_fhirtools_CXX_OUTPUTS} APPEND PROPERTY COMPILE_FLAGS "-Wno-unused-parameter")
endif()
target_sources(fhirtools
    PRIVATE
        converter/FhirConverter.cxx
        converter/FhirConverter.hxx
        converter/internal/FhirJsonToXmlConverter.cxx
        converter/internal/FhirSAXHandler.cxx
        expression/BooleanLogic.cxx
        expression/Comparison.cxx
        expression/Conversion.cxx
        expression/Expression.cxx
        expression/Functions.cxx
        expression/FhirSupplements.cxx
        expression/LiteralExpression.cxx
        expression/Math.cxx
        expression/StringManipulation.cxx
        model/Collection.cxx
        model/DateTime.cxx
        model/Element.cxx
        model/erp/ErpElement.cxx
        model/MutableElement.cxx
        model/NumberAsStringParserDocument.cxx
        model/NumberAsStringParserWriter.cxx
        model/ValueElement.cxx
        parser/ErrorListener.cxx
        parser/FhirPathFixer.cxx
        parser/FhirPathParser.cxx
        repository/DefinitionKey.cxx
        repository/FhirCodeSystem.cxx
        repository/FhirConstraint.cxx
        repository/FhirElement.cxx
        repository/FhirResourceGroup.cxx
        repository/FhirResourceGroupConfiguration.cxx
        repository/FhirResourceGroupConst.cxx
        repository/FhirResourceViewConfiguration.cxx
        repository/FhirResourceViewGroupSet.cxx
        repository/FhirResourceViewVerifier.cxx
        repository/FhirSlicing.cxx
        repository/FhirStructureDefinition.cxx
        repository/FhirStructureRepository.cxx
        repository/FhirValueSet.cxx
        repository/FhirVersion.cxx
        repository/internal/FhirStructureDefinitionParser.cxx
        repository/internal/FhirStructureRepositoryFixer.cxx
        transformer/ResourceProfileTransformer.cxx
        util/SaxHandler.cxx
        util/Utf8Helper.cxx
        util/VaListHelper.cxx
        util/XmlMemory.cxx
        typemodel/ProfiledElementTypeInfo.cxx
        validator/FhirPathValidator.cxx
        validator/internal/ProfileSetValidator.cxx
        validator/internal/ProfileSolver.cxx
        validator/internal/ProfileValidator.cxx
        validator/internal/ReferenceContext.cxx
        validator/internal/ReferenceFinder.cxx
        validator/internal/SlicingChecker.cxx
        validator/internal/ValidationData.cxx
        validator/ValidationResult.cxx
        "${ANTLR_fhirtools_CXX_OUTPUTS}"
)
