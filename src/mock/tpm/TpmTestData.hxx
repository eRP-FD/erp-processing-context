/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TPMTESTDATA_HXX
#define ERP_PROCESSING_CONTEXT_TPMTESTDATA_HXX

#include <string_view>

#if WITH_HSM_MOCK  != 1
#error TpmTestData.hxx included but WITH_HSM_MOCK not enabled
#endif
namespace tpm
{
    constexpr static auto secretHSM_bin_base64 = std::string_view("AEQAIHUbsj1tMWw/1jZtSZba8U68Fwp94Jt7Q13CUfwwN7T3ACAfnoAiO/asxFfl1z73dG3KMogV0evOONU3Ca4UGyaE0A==");
    constexpr static auto encCredHSM_bin_base64 = std::string_view("AEQAIPrdK/uzWaz+vBOAESXC7M0QwupdErT1KEuY/xyWkmsza2blBl4I5kBg03Z8DFwPt3HhU7OSe5SNUekddHSBnFdwug==");
    constexpr static auto QuoteNONCESaved_bin_base64 = std::string_view("AVCPzbF8yP66xuUvSC3GS8yGVck2rlG/zYt0LfYUR4Y=");

    // Outputs
    constexpr static auto ekcertecc_crt_base64 = std::string_view("MIIBtDCCAVqgAwIBAgIUczdLpnm5IVns5X3lgSgNAcpAahYwCgYIKoZIzj0EAwIwTjELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAk5ZMREwDwYDVQQHDAhZb3JrdG93bjEMMAoGA1UECgwDSUJNMREwDwYDVQQDDAhFSyBFQyBDQTAeFw0yMTAzMzAxMTM4MTJaFw00MTAzMjcxMTM4MTJaMFIxCzAJBgNVBAYTAlVTMQswCQYDVQQIDAJOWTERMA8GA1UEBwwIWW9ya3Rvd24xDDAKBgNVBAoMA0lCTTEVMBMGA1UEAwwMSUJNJ3MgU1cgVFBNMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEB1quje1Gma4kwv9JaoSpWWxU83ruucO9F5zkm//KZwlW+GE4XRXDzXNjw6DFIOiiUxqe8QJ2szoAfGh+XKNLJqMSMBAwDgYDVR0PAQH/BAQDAgMIMAoGCCqGSM49BAMCA0gAMEUCIFNoFxNV1hxtewouQhMxF7l3hM59kpbQ9UucE6ThFKvCAiEA/8eJS34BZkpiMdIVP/2yztqTsW86eljuJh8uS0PZ+Wc=");
    constexpr static auto EKPub_name_base64 = std::string_view("AAufhtCBiEx9ZZov6qDFWtAVo79PGysLgizRXWwVsPAKCA==");
    constexpr static auto AKPub_bin_base64 = std::string_view("AHgAIwALAAUEcgAg5YfBGrUPnYcw9yHj/qQrRsBFWyRvlq7oXRjrO+ZNZmoAEAAYAAsAAwAQACDZ0QGSKhZWWVyL0MvwYlfeZTmoTnQt7Da9wiWHba87cQAg4wFPn/zWhHUn2KUGy7K2t3XP93xKuuyn4ZuGyVrotrU=");
    constexpr static auto h80000002_bin_base64 = std::string_view("AAvsjWaVs/GuW474RCyexJlqK1mkQSf9sRo+266eUk54Aw==");
    constexpr static auto credDecHSMSaved_bin_base64 = std::string_view("kFH9296Z4uZpgZMV2Yw9HSoy43hKTDpMEr77G7R9QHY=");
    constexpr static auto attest_bin_base64 = std::string_view("/1RDR4AYACIAC6kXGKf2buLDAJ0GYfvjpPsZyh3oUZKsxuR1izrF3wngACBwgIvsAd4N1dhnD+P3HDkryA0mbG+dvSkOxEjXgAHb0gAAAAAF9ONwAAAALwAAAAABIBkQIwAWNjYAAAABAAsDAQAAACBmaHqt+GK9d2yPwYuOn44gCJcUhW7iM7OQKlkdDV8pJQ==");
    constexpr static auto quotesig_bin_base64 = std::string_view("ABgACwAgsqHoBGQeOlC1zmbgIe4wO5rOMIXtTEyj9ptJwsoXR14AIA1NXkkVm3XV/a7iGYaKoVyq0WGiO66J8HdkZ75hL0jp");

    constexpr static auto attestationKeyName_base64 = "AAv+egMWhqowY/WSKilTdvXUCQsXOmjta7Z0Qzjv+VY6Xg==";
    //constexpr static auto attestationKeyBlob_base64 = "T2kKELjyE+Pf4yB58tVP31r1Ps/0zYqcSJb3PyEXduNCAAAAU0lNTADvSZA1NjIqpWkGmW5aNOLEN7ZZa7sH58AM+MwTAAAAjwAAANuOEqqHDwOWpQQMqeSP1PX5hn+F1LI/gRU66ZICFAOUueGNQntENT6OypMGB6z+iFtkK7KtVwSIG0hT1XkL6rxabPb+dFnoGXJb/27S9M37DKsNXT+MUTJaDHyNyVIAjUlMpHhvlwkfqf3CPZ6XsunoqfN01lOiX2ShybewqC0avFfKAJ0AjUksj7VmljS2";

    constexpr static auto teeToken_blob_base64 = std::string_view("lAAAAMzMzMzgAAAAAAAAANIiLObaaxQ759fgADmSuPdbczQhAZmcHt0un7P9AAftlAAAAFNJTUwAmP9OFznZZ5Kf6rhrzmkovHFwcpPffyc2uJmX6/9xhZQAAACinf4p2RpGUvse95M+aiQ6ZE5WLdlAFRwXxEVeJo1nV4U9GELFKkKNPrywoagmF1nVy8qiBI3Z0tdVsF05YAxcQ9BrXJBuR7Zl6uKOssea8yXoNL2JbPKLxGq/4Zq8r0sLGqyTiToL/NVtAwChvhBvOZ/SLoez0xM3GgXNBCqe2YImJCrMN0sJ/YcpMCjk7GZzfjfL");
    constexpr static auto TrustedAk_blob_base64 = std::string_view("QgAAAMzMzMzbAAAAAAAAAB/m1TQ5yVp51BDZP4uV0YFg4tvGx0JVSK+Hb7CwNA9/QgAAAFNJTUwAC1h+X8OoMr7dsxs6Vv8mu5MQ+/vF2yLd3PTYhwAAAI8AAACPCvLUAGtBhOtieJ1Ja6h5GUa5UOJlNTla9fHNaaIsLe8JBpCZqeWI0jprBslO3tMLAJGbvRJc+DgF7HtVlZjLOjNgXhF6rHLi5u/4esXxe4sWPdXdKoxZPWbVeOl6ND+sAbFgCTN95o2CcAoTK96kCKpWxTzvtlzqf8B1ETv4nD9Ttr245yqr8CP7ff02VQ==");
    constexpr static auto TrustedQuote_blob_base64 = std::string_view("QgAAAMzMzMwRAQAAAAAAAIE7Jz6aWqlArduWzYkfNa3Oe09+TPqNqJtb6jbXOprHQgAAAFNJTUwAh80ZhQBWEc3H4T5dKETk5AO6UohPSqJjVoFhKP9xhcUAAAC73GDR6OuLECLMgTsR3e2EhuxXyO6t0pC2m0D7Gqv0nyWBjGv+sSTYXTg8poBfJuGpLWm/LrBESlFUKr1BOc8Y+CAV6shp+4q/Qh6hZGHZNZyMozixTdzM/4MAkdUuc/vY7xezR6AUTv/Ooj32WJ+jpymzCBMVxlQSW5UwlA4NnJiSZW2DY29WFlpo9o+wU3F+c0eH93rq/W538yTGUuO6ceBNH5dz9tpIA814P3B5mwdShcaP3co+a5dsslylIJo2p3bWsQ==");
    constexpr static auto task_derivation_key_blob_base64 = std::string_view("QgAAAAAAAAB4AAAAAAAAAOE3LxgYU/52H2HgrRKW3YCv7Mj7ecEeXKtv/HFZ9CtpQgAAAFNJTUwAq7qRADMq4Xy3WH9CkBU8q0OZIDxIb7X0zF6FupeW9ywAAACAtHCFVQxN2x/Zp1q/S9sq9EoJZMZJihFEyyK9jQgApFjoyMM8kZnFxC4fWQ==");
    constexpr static auto vauSigKeupair_blob_base64 =
        std::string_view("FAAAAAAAAADWAAAAAAAAAMC+DozCfXAl1v+TjLngfBXeIx01hAYu7XaByd8f1YvmFAAAAFNJTUwAh7va7ryUuScQr5JTozi4MHU/d9PyRFz22Hfej39QqooAAACI+zRGjitsp9c9lz67owTsFlCIQOdOYW153faef3tqm5kQGnQOBJQN9VXe2P/PWwfBXLSnOQWtuMDqyRUJc03smPIxzUgWJLfknChFxSi7KvxuV5Oq3Z4WalIZNnkQ2cVMyJiU3oFOVzwz6jg4SpLSdIPQcypwosbw2BfE4YOFLVJ9ss1X21vfzIM=");
    constexpr static auto vauSigCertificate_base64 = std::string_view(
        "MIIDKDCCAs+gAwIBAgIQfiiBNU4DSkeQ/6MZYym0UDAKBggqhkjOPQQDAjCBmTEL"
        "MAkGA1UEBhMCREUxHzAdBgNVBAoMFmFjaGVsb3MgR21iSCBOT1QtVkFMSUQxSDBG"
        "BgNVBAsMP0ZhY2hhbndlbmR1bmdzc3BlemlmaXNjaGVyIERpZW5zdC1DQSBkZXIg"
        "VGVsZW1hdGlraW5mcmFzdHJ1a3R1cjEfMB0GA1UEAwwWQUNMT1MuRkQtQ0ExIFRF"
        "U1QtT05MWTAeFw0yNDAxMDEyMzAwMDBaFw0yNTEyMzEyMzAwMDBaMEoxEDAOBgNV"
        "BAMMB2VyZXplcHQxKTAnBgNVBAoMIElCTSBEZXV0c2NobGFuZCBHbWJIIC0gTk9U"
        "LVZBTElEMQswCQYDVQQGEwJERTBaMBQGByqGSM49AgEGCSskAwMCCAEBBwNCAAR+"
        "7RsxaLme16Z3wI0eZDSQ20C6xPoe72kQ4FaP16ZwNR2eigtmgnNcYiGzUDhrqzZ7"
        "6k6+GjSHAo/cR/Jz2sk9o4IBRDCCAUAwHQYDVR0OBBYEFMLuz2SOwwp2pkMguOnJ"
        "e/64U39GMA4GA1UdDwEB/wQEAwIGQDAXBgNVHREEEDAOggxFUlAuSUJNLlRFU1Qw"
        "DAYDVR0TAQH/BAIwADBSBgNVHSAESzBJMDsGCCqCFABMBIEjMC8wLQYIKwYBBQUH"
        "AgEWIWh0dHA6Ly93d3cuZ2VtYXRpay5kZS9nby9wb2xpY2llczAKBggqghQATASC"
        "GzBGBggrBgEFBQcBAQQ6MDgwNgYIKwYBBQUHMAGGKmh0dHA6Ly9vY3NwLXRlc3Qu"
        "b2NzcC50ZWxlbWF0aWstdGVzdDo4MDgwLzAfBgNVHSMEGDAWgBR7DFUnTuWmL10d"
        "EdexG/Z7ueFFJTArBgUrJAgDAwQiMCAwHjAcMBowGDAKDAhFLVJlemVwdDAKBggq"
        "ghQATASCAzAKBggqhkjOPQQDAgNHADBEAiBGhz1v8+7FuyF1xTlTfHu7cY1OeXSw"
        "4MUfmMGw49GgvQIgXK9dq2nJdVUZZsWRgQQGPDsTbCcM6OdRrfGUl1TCh6Y=");
    constexpr static auto eciesKeyPair_blob_base64 = std::string_view("QgAAAMzMzMzWAAAAAAAAALcZakO8hyyKSzMmZMLg+4uXzZcCMlsYMqt939HcBl64QgAAAFNJTUwAr161zuNsjxzzmWqUpQf7fixjvExs91xCSgQ5T4CEm4oAAADRK529M2icpSN+WvMn2QgiwMXsSmOewEjIczHpcny83ZFY1oYyA2VT4ekr5MNU5Ss4B/mclkiT4Fzckc4npRpS9J7fTaiTeVjUXY5FaqxlL1UP6t4B9gVHVA4hbyS+Gx1PNUZT4wX4rh/qTZTICHhy/mrddEGsjjheQSO+SEgyNSQkT5YrZfAu9vM=");
    constexpr static auto hashKey_blob_base64 = std::string_view("QgAAAMzMzMx4AAAAAAAAADxGkn3uQuwSPkZjwM8V4pxR4BaiRnQpxZa1VG/uVCnIQgAAAFNJTUwAXdSmO4u/vUFdkNEhVDE9ufrWguHLLQjSnt9L5P///ywAAABiIrhHkPfdgeWJwrZS0pQ4vkx6/uChYcsRfnKs7zv3kNGDF5NHNTDEqTvaGg==");
    constexpr static auto QuoteNonce_blob_base64 = std::string_view("QgAAAMzMzMx4AAAAAAAAABduk/QOzVwRp8F6pXGZUjRQ7XLCYySJ0OXu74y+TqXxQgAAAFNJTUwAWQexaZD0rrUUi29J3stfoCm4Pyz1qF2cxtToCgAAACwAAAD0v+bW6gx+5O5TU4r6NPZrYu+y1hzC5ovXaqZvVMYAlp3jm3uZ53KmJ/G2yw==");
    constexpr static auto vauAutKeyPair_blob_base64 = std::string_view("QgAAANDYvA/WAAAAAAAAACGWfV398F0Z/hbnl2hBO5m49oontJkxoGYWZykrYTyjQgAAAFNJTUwAXxlkRjKeFf3L6fU+tpMe6dA6ECdJrmaoRD4pin9QqooAAACKvRXRs6VT8mCVOBnXdP06xz5AZUOC3h9WRHoEC7bRDk64dN/mQb/rRqCF1QcbtvMQIgpqf7CPNKIkZtLrkXTfSBoERJckT+/+Sa6kfe/WpICO7pv4eN0RpFLGcVFoU58AnRvTSDzpKlohet0QzC5tNdvBZSDMAn/HsUXlv+cyEpqZ4LySbMMvfxM=");
    constexpr static auto vauAutCertificate_base64 = std::string_view(
        "MIIDcDCCAxegAwIBAgIQPmVMYFdORz2QCj50CWT3MzAKBggqhkjOPQQDAjCBmTEL"
        "MAkGA1UEBhMCREUxHzAdBgNVBAoMFmFjaGVsb3MgR21iSCBOT1QtVkFMSUQxSDBG"
        "BgNVBAsMP0ZhY2hhbndlbmR1bmdzc3BlemlmaXNjaGVyIERpZW5zdC1DQSBkZXIg"
        "VGVsZW1hdGlraW5mcmFzdHJ1a3R1cjEfMB0GA1UEAwwWQUNMT1MuRkQtQ0ExIFRF"
        "U1QtT05MWTAeFw0yNDEwMjQyMjAwMDBaFw0yNjEwMjMyMjAwMDBaMEwxFDASBgNV"
        "BAMMC2VSZXplcHQgVkFVMScwJQYDVQQKDB5JQk0gR21iSCBURVNULU9OTFkgLSBO"
        "T1QtVkFMSUQxCzAJBgNVBAYTAkRFMFowFAYHKoZIzj0CAQYJKyQDAwIIAQEHA0IA"
        "BGF/W0sIVkWeseJnh/Ry8xctxSX9HsG0I2jMjTHjR+kaDLUqZO3A4TIu4WwsKkMm"
        "Ki4heNl4gfZyz+vRKsVhtIejggGKMIIBhjAdBgNVHQ4EFgQULF7zJ/jOYxJxkWWX"
        "8h2XUuo9ZSkwDgYDVR0PAQH/BAQDAgWgMAwGA1UdEwEB/wQCMAAwZwYDVR0gBGAw"
        "XjBQBggqghQATASBIzBEMEIGCCsGAQUFBwIBFjZodHRwOi8vd3d3LmhiYS10ZXN0"
        "LnRlbGVzZWMuZGUvcG9saWNpZXMvRUVfcG9saWN5Lmh0bWwwCgYIKoIUAEwEgRsw"
        "RgYIKwYBBQUHAQEEOjA4MDYGCCsGAQUFBzABhipodHRwOi8vb2NzcC10ZXN0Lm9j"
        "c3AudGVsZW1hdGlrLXRlc3Q6ODA4MC8wHwYDVR0jBBgwFoAUewxVJ07lpi9dHRHX"
        "sRv2e7nhRSUwagYFKyQIAwMEYTBfMF0wWzBZMFcwMgwwRS1SZXplcHQgdmVydHJh"
        "dWVuc3fDvHJkaWdlIEF1c2bDvGhydW5nc3VtZ2VidW5nMAoGCCqCFABMBIICExU5"
        "LUUtUmV6ZXB0LUZhY2hkaWVuc3QwCQYDVR0lBAIwADAKBggqhkjOPQQDAgNHADBE"
        "AiACrLWTSgmzGpeaF5IGbrxuXAuUnHZa3vtERcpnlTl8GAIgL7yiaE5ZQwgu2aRK"
        "1j3XKLmXZG0gKu7zyd9sJbzViU4="
    );
}

#endif
