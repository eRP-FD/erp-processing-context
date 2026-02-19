#!/bin/bash

urls=(
    "https://erp.lu2.erezepttest.net/ocspf"
    "https://erp.box2.erezepttest.net/ocspf"
    "https://erp.dev2.erezepttest.net/ocspf"
)

## created in epa-test-mock using
# openssl ocsp -no_nonce -issuer ACLOS.FD-CA4.pem -cert epa-as-1-mock.epa.telematik-test.pem -reqout - | base64 -w0
VAUSIG_OCSP_request="MFEwTzBNMEswSTAJBgUrDgMCGgUABBSPU+U6fzBWwW/64Paea0ruSxCtKgQUewxVJ07lpi9dHRHXsRv2e7nhRSUCEDCPxhC9ekuXgpKPIi7ON5c="
## created in epa-test-mock using
TLS_OCSP_request="MFIwUDBOMEwwSjAJBgUrDgMCGgUABBS1yWVTi1CrQxeJKDzSr/uvKWuKDAQUCQPQloUXFoP6XmxZ+bFdJde7ja8CEQDdlJgucGtOSZJm6NMu8UCI"
# openssl ocsp -no_nonce -issuer ACLOS.FD-CA1.pem -cert epa-vau-aut.der -reqout - | base64 -w0

errors=()
result=0

request() {
    local data="$1"
    local url="$2"
    local output="$3"
    curl -H "Content-Type: application/ocsp-request" --data-binary @<(base64 -d <<< "$data") "$url" -o "${output}" &&
        openssl ocsp -respin "$output" -noverify
}

try_request_all()
{
    local name="$1"
    local data="$2"
    local output="$3"
    for url in "${urls[@]}" ; do
        echo "Requesting ${server} for OCSP-Response for ${name}." >&2
        if request "$data" "$url" "$output" ; then
             return
        fi
    done
    errors=("${errors[@]}" "failed to get OCSP reponse for ${name}")
    result=1
}

try_request_all "TLS" "${TLS_OCSP_request}" "epa-test-mock/ocsp-response-tls-epa-as-mock.der"
try_request_all "VAUSIG" "${VAUSIG_OCSP_request}" "epa-test-mock/ocsp-response-vau-sig.der"

for err in "${errors[@]}" ; do
    echo "ERROR: ${err}" >&2
done
exit "${result}"
