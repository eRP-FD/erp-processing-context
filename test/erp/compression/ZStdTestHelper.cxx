/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/erp/compression/ZStdTestHelper.hxx"

#include "shared/util/Base64.hxx"

static constexpr std::string_view sampleDictionaryBase64 =
    "N6Qw7AEAAAA6ELjANgAAAAAAMBERAGAd2pP3u/feXnaL3N8rhUViYeW3729mVn5XzJq10ZLpJYma"
    "iDIzJTYZIxcCCNMCgAgGhkUmA7qwzR4ABOABAIeNHQcIC4wFDIQFCQdKAyFZHBSVxeFACEIokKgo"
    "FOQIGSgHAAC0OVKQIzkQMmQQAAEBEAABAAAAAAEAAAAEAAAACAAAACAgICAgICAgInN5c3RlbSI6"
    "Imh0dHA6Ly91bml0c29mbWVhc3VyZS5vcmciLAogICAgICAgInRleHQiOiJTdW1hdHJpcHRhbi0x"
    "YSBQaGFybWEgMTAwIG1nIFRhYmxldHRlICAgICJleHBpcmF0aW9uRGF0ZSI6IjIwMjAtMDItMDNU"
    "MDA6MDA6MDArMDA6MDAiCiBoaXIua2J2LmRlL0NvZGVTeXN0ZW0vS0JWX0NTX0VSUF9TZWN0aW9u"
    "X1R5cGUiLAogIGhpci5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL2drdi9kbXAta2VubnplaWNoZW4i"
    "LAogICAgInZhbHVlIjoiMTYwLjEyMy40NTYuNzg5LjEyMy41OCIKICAgIH0sCiAgICAidHlvcnki"
    "LAogICAgICAgICAgICAgICAgICAgICAgICAgICAgImNvZGUiOiIwMCIKICAgIHRwOi8vdGVybWlu"
    "b2xvZ3kuaGw3Lm9yZy9Db2RlU3lzdGVtL3YyLTAyMDMiLAogICAgICAgICAgIm5hbWUiOiJIYXVz"
    "YXJ6dHByYXhpcyBEci4gVG9wcC1HbMO8Y2tsaWNoIixDb21wb3NpdGlvbi9lZDUyYzFlMy1iNzAw"
    "LTQ0OTctYWUxOS1iMjM3NDRlMjk4NzYiLCAgICAgICAgICAgICAgICAgICAgICAgInZhbHVlQm9v"
    "bGVhbiI6ZmFsc2UKICAgICAgLWEyMzgtNGRhZi1iNGU2LTY3OWRlZWVmMzgxMSIsCiAgICAgICAg"
    "ICAgICAgICAibWVlU3lzdGVtL0tCVl9DU19TRkhJUl9LQlZfVkVSU0lDSEVSVEVOU1RBVFVTIiwK"
    "ICAgIFN0cnVjdHVyZURlZmluaXRpb24vaXNvMjEwOTAtQURYUC1zdHJlZXROYW1lIiwKICAgICAg"
    "ICAgInR5cGUiOiJib3RoIiwKICAgICAgICAgICAgICAgICAgICAgICAgImxpbmUgICAgICAgICAg"
    "ICAgICAgICAidmFsdWVTdHJpbmciOiJNdXN0ZXJzdHIuIgogICAgIGRsZXwxLjAuMCIKICAgICAg"
    "ICBdCiAgICB9LAogICAgImlkZW50aWZpZXIiOnsKICAgbXAiOiIyMDIwLTA2LTIzVDA4OjMwOjAw"
    "KzAwOjAwIiwKICAgICJlbnRyeSI6WwogICB1Y3R1cmVEZWZpbml0aW9uL0tCVl9FWF9FUlBfU3Rh"
    "dHVzQ29QYXltZW50IiwKICAgIGljYXRpb24vNWZlNmUwNmMtODcyNS00NmQ1LWFlY2QtZTY1ZTA0"
    "MWNhM2RlIiwKICAgaWNhdGlvblJlcXVlc3QvZTkzMGNkZWUtOWViNS00YjQ0LTg4YjUtMmExOGI2"
    "OWYzYjkua2J2LmRlL05hbWluZ1N5c3RlbS9LQlZfTlNfRk9SX1BydWVmbnVtbWVyIiwKICAgIGl0"
    "aW9uZXJSb2xlLzlhNDA5MGY4LThjNWEtMTFlYS1iYzU1LTAyNDJhYzEzMDAwIiwKT3JnYW5pemF0"
    "aW9uL2NmMDQyZTQ0LTA4NmEtNGQ1MS05Yzc3LTE3MmY5YTk3MmUzYiJlIjoiQ292ZXJhZ2UvMWIx"
    "ZmZiNmUtZWIwNS00M2Q3LTg3ZWItZTc4MThmZTk2NjFhInVyZURlZmluaXRpb24vS0JWX1BSX0VS"
    "UF9QcmVzY3JpcHRpb258MS4wLjAiCiAgICAgICAgICAgICAgXSwKICAgICAgICAgICAgICAgICJz"
    "dGF0dXMiOiJhY3RpdmUiLAogICAgICAgICAiaWQiOiIyMDU5N2UwZS1jYjJhLTQ1YjMtOTVmMC1k"
    "YzNkYmRiNjE3YzMiLCAgICAgICAgICAgICAgICAgInVybCI6Imh0dHA6Ly9obDcub3JnL2ZoaXIv"
    "U3RydWN0aXB0aW9uIiwKICAgICAgICAgICAgICAgICAgICAgICAgImV4dGVuc2lvbiI6WwogICBl"
    "c291cmNlIjp7CiAgICAgICAgICAgICAgICAicmVzb3VyY2VUeXBlIjoiUHJhY3RpdCAgICAibWV0"
    "YSI6ewogICAgICAgICJwcm9maWxlIjpbCiAgICAgICAgICAgICJodHRwa2J2LmRlL0NvZGVTeXN0"
    "ZW0vS0JWX0NTX0VSUF9NZWRpY2F0aW9uX0NhdGVnb3J5IixodHRwOi8vcHZzLnByYXhpcy10b3Bw"
    "LWdsdWVja2xpY2gubG9jYWwvZmhpci9QcmFjdCAgICAgICJyZWZlcmVuY2UiOiJQYXRpZW50Lzk3"
    "NzRmNjdmLWEyMzgtNGRhZi1iNGU2dXBwZSIsCiAgICAgICAgICAgICAgICAgICAgICAgICJ2YWx1"
    "ZUNvZGluZyI6ewogICBSX1ByYWN0aXRpb25lcnwxLjAuMSIKICAgICAgICAgICAgICAgICAgICBd"
    "CiAgICAgICAgICAiY29kZSI6ewogICAgICAgICAgICAgICAgICAgICJjb2RpbmciOlsKICA=";

std::string sampleDictionary(Compression::DictionaryUse dictUse, uint8_t version)
{
    static const std::string sampleDictBin = Base64::decodeToString(std::string{sampleDictionaryBase64});
    std::string result = sampleDictBin;
    result[4] = char(uint8_t(dictUse) | (version << 4u));
    return result;
}
