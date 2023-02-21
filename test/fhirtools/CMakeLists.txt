add_executable(fhirtools-test
        fhirtools-test.cxx

        ClosedSliceExtensionsTest.cxx
        CollectionTest.cxx
        DateTimeTest.cxx
        DefaultFhirStructureRepository.cxx
        ElementTest.cxx
        ExpressionTest.cxx
        FhirPathParserTest.cxx
        FhirPathValidatorTest.cxx
        FhirStructureRepositoryFixerTest.cxx
        FhirValueSetTest.cxx
        issues/ERP-12159-RefereceTypeCheck.cxx
        ReferenceTest.cxx
        SlicingValidationTest.cxx
        )

target_link_libraries(fhirtools-test fhirtools CONAN_PKG::gtest erp-processing-context-files erp-test-util-lib
        )
