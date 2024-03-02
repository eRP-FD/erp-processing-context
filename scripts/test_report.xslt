<?xml version="1.0" encoding="UTF-8"?>
<!--
  ~ (C) Copyright IBM Deutschland GmbH 2021, 2024
  ~ (C) Copyright IBM Corp. 2021, 2024
  ~
  ~ non-exclusively licensed to gematik GmbH
  -->
<xsl:stylesheet
        version="1.0"
        xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
        xmlns:html="http://www.w3.org/1999/xhtml"
        xmlns="http://www.w3.org/1999/xhtml"
        exclude-result-prefixes="html"
>
    <xsl:output method="html" encoding="UTF-8"/>

    <xsl:template match="testsuites">
        <html:head>
            <title>MVO Test report</title>
            <style>
                code {
                display:block;
                overflow-x: scroll;
                white-space: pre-wrap;
                white-space: -moz-pre-wrap;
                white-space: -pre-wrap;
                white-space: -o-pre-wrap;
                word-wrap: break-word;
                height: 60px;
                width: 80%;
                }
                .decodedPrescription {
                display:block;
                overflow-x: scroll;
                white-space: pre-wrap;
                white-space: -moz-pre-wrap;
                white-space: -pre-wrap;
                white-space: -o-pre-wrap;
                word-wrap: break-word;
                display: none;
                height: 300px;
                width: 80%;
                }
                .failure {
                color:red;
                }
                .success {
                color:green;
                }
            </style>
        </html:head>
        <html:body>
            <h1>Testreport</h1>
            <p>BuildVersion: <xsl:value-of select="@BuildVersion"/></p>
            <p>BuildType: <xsl:value-of select="@BuildType"/></p>
            <p>ReleaseVersion: <xsl:value-of select="@ReleaseVersion"/></p>
            <p>ReleaseDate: <xsl:value-of select="@ReleaseDate"/></p>
            <p>Test Time: <xsl:value-of select="@timestamp"/></p>
            <p>Total number of tests: <xsl:value-of select="@tests"/></p>
            <p>Failures: <xsl:value-of select="@failures"/></p>
            <p>Disabled: <xsl:value-of select="@disabled"/></p>
            <p>Errors: <xsl:value-of select="@errors"/></p>
            <xsl:apply-templates/>
            <script>
                <![CDATA[
function decodePrescription(prescriptionId){
    var encodedPrescription = document.getElementById("prescription" + prescriptionId).innerHTML;
    document.getElementById("decodedPrescription" + prescriptionId).innerHTML = atob(encodedPrescription).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
    document.getElementById("decodedPrescription" + prescriptionId).style.display = "block";
}
                ]]>
            </script>
        </html:body>
    </xsl:template>

    <xsl:template match="testsuite">
        <h2>Requirement <xsl:value-of select="substring(@name, 5)"/></h2>
        <p>Number of Test: <xsl:value-of select="@tests"/></p>
        <p>Failures: <xsl:value-of select="@failures"/>
           Disabled: <xsl:value-of select="@disabled"/>
           Skipped: <xsl:value-of select="@skipped"/>
           Errors: <xsl:value-of select="@errors"/></p>
        <xsl:apply-templates/>
    </xsl:template>

    <xsl:template match="testcase">
        <h3><xsl:value-of select="properties/property[@name='Description']/@value"/></h3>
        <xsl:apply-templates/>
        <p>Prescription</p>
        <code id="prescription{@name}{position()}"><xsl:value-of select="properties/property[@name='Prescription']/@value"/></code>
        <button onclick="decodePrescription('{@name}{position()}')">Decode Prescription</button>
        <code id="decodedPrescription{@name}{position()}" class ="decodedPrescription"/>
        <p>Result: <xsl:value-of select="@result"/></p>
        <xsl:choose>
            <xsl:when test="count(failure) = 0"><p class="success">PASSED</p></xsl:when>
            <xsl:otherwise><p class="failure">FAILED</p></xsl:otherwise>
        </xsl:choose>

    </xsl:template>

    <xsl:template match="failure">
        <p class="failure">Failure: <xsl:value-of select="@message"/></p>
    </xsl:template>

</xsl:stylesheet>
