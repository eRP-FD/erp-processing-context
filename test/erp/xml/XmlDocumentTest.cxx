/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/xml/XmlDocument.hxx"
#include "erp/util/GLog.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "test/util/LogTestBase.hxx"

#include <gtest/gtest.h>
#include <memory>


TEST(XmlDocumentTest, externalEntity)
{
    std::string_view xmlData = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
        <!DOCTYPE root [ <!ENTITY % xxe SYSTEM "http://localhost:4564/"> %xxe; ]>
        <root>test</root>)";
    // the only way to validate the not-loading of external entities is via
    // checking the output
    LogTestBase::TestLogSink logSink;

    XmlDocument xmlDocument(xmlData);
    // Search for the log message as proof that the external entity loading was suppressed
    bool success = false;
    logSink.visitLines([&](const std::string& line) {
        if (line.find("Suppressing external entity loading from http://localhost:4564") != std::string::npos)
            success = true;
    });
    ASSERT_TRUE(success) << "did not find expected log message";
}
