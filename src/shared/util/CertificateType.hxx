/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_CERTIFICATETYPE_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_CERTIFICATETYPE_HXX

enum class CertificateType
{
    C_CH_AUT,
    C_CH_AUT_ALT,
    C_FD_AUT,
    C_FD_SIG,
    C_FD_OSIG,
    C_FD_TLS_S,
    C_HCI_ENC,
    C_HCI_AUT,
    C_HCI_OSIG,
    C_HP_QES,
    C_CH_QES,

    /**
     * The following certificate type is introduced to comply with updated requirement A_20159-*
     */
    C_HP_ENC,
    C_ZD_TLS_S,
};

#endif
