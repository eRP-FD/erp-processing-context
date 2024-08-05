/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SCHEMATYPE_HXX
#define ERP_PROCESSING_CONTEXT_SCHEMATYPE_HXX

// these enum names must be in line with the schema filenames.
enum class SchemaType
{
    fhir,// general FHIR schema
    BNA_tsl,
    Gematik_tsl,
    Pruefungsnachweis,
    CommunicationDispReqPayload,
    CommunicationReplyPayload,
};

#endif//ERP_PROCESSING_CONTEXT_SCHEMATYPE_HXX
