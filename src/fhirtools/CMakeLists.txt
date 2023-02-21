
add_library(fhirtools STATIC)

target_link_libraries(fhirtools
    PUBLIC
        CONAN_PKG::antlr4-cppruntime
        CONAN_PKG::boost
        CONAN_PKG::date
        CONAN_PKG::glog
        CONAN_PKG::gsl-lite
        CONAN_PKG::magic_enum
        CONAN_PKG::rapidjson
        CONAN_PKG::libxml2
        CONAN_PKG::openssl # only needed because we include Expect.hxx from erp
    )

antlr_target(fhirtools fhirpath.g4 PARSER LEXER VISITOR PACKAGE fhirtools)
target_include_directories(fhirtools
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
    PRIVATE
        "${ANTLR_fhirtools_OUTPUT_DIR}"
)

add_custom_target(generated_source DEPENDS ${ANTLR_fhirtools_CXX_OUTPUTS})

if (CMAKE_COMPILER_IS_GNUCXX)
    set_property(SOURCE ${ANTLR_fhirtools_CXX_OUTPUTS} APPEND PROPERTY COMPILE_FLAGS "-Wno-unused-parameter")
endif()
target_sources(fhirtools
    PRIVATE
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
        model/ValueElement.cxx
        parser/ErrorListener.cxx
        parser/FhirPathParser.cxx
        repository/FhirCodeSystem.cxx
        repository/FhirConstraint.cxx
        repository/FhirElement.cxx
        repository/FhirValueSet.cxx
        repository/FhirSlicing.cxx
        repository/FhirStructureDefinition.cxx
        repository/FhirStructureRepository.cxx
        repository/internal/FhirStructureDefinitionParser.cxx
        repository/internal/FhirStructureRepositoryFixer.cxx
        util/SaxHandler.cxx
        util/Utf8Helper.cxx
        util/VaListHelper.cxx
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
