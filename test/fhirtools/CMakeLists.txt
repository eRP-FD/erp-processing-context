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
        FhirResourceViewConfigurationTest.cxx
        FhirResourceGroupConfigurationTest.cxx
        FhirResourceGroupConstTest.cxx
        FhirResourceGroupTest.cxx
        FhirResourceViewGroupSetTest.cxx
        FhirStructureRepositoryFixerTest.cxx
        FhirStructureRepositoryTest.cxx
        FhirValueSetTest.cxx
        issues/ERP-12159-RefereceTypeCheck.cxx
        ReferenceTest.cxx
        SlicingValidationTest.cxx
        ValueMatchingTest.cxx
        )

target_link_libraries(fhirtools-test fhirtools CONAN_PKG::gtest erp-processing-context-files erp-test-util-lib
        )
