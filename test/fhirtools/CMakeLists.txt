add_executable(fhirtools-test
        fhirtools-test.cxx

        ClosedSliceExtensionsTest.cxx
        CollectionTest.cxx
        ElementTest.cxx
        ExpressionTest.cxx
        FhirPathParserTest.cxx
        FhirPathValidatorTest.cxx
        FhirValueSetTest.cxx
        SlicingValidationTest.cxx
        )

target_link_libraries(fhirtools-test fhirtools CONAN_PKG::gtest erp-processing-context-files erp-test-util-lib
        )
