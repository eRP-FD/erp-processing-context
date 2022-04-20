#!/bin/bash

# This script generates a test PKI -- root CA, sub CA ( TSL signer ) and uses them to sign TSL templates.
# Input data can be found in resources/test/generated_pki_input/, the generated data are put
# into resources/test/generated_pki/. For each created CA a subdirectory is created, which contains, besides some
# management data for the CA, all certificates and associated private keys issued by the CA.
#
# See resources/test/generated_pki_input/README.txt for a layout of resources/test/generated_pki/.
#

set -e

scriptDir=$(dirname "$(realpath "${BASH_SOURCE[0]}")")
rootDir=$(dirname "$scriptDir")
testDataDir="$rootDir/resources/test/generated_pki"
inputDataDir=$(dirname "$scriptDir")/resources/test/generated_pki_input
opensslConfig="$inputDataDir/openssl.cnf"

defaultKeyArguments="-newkey rsa:4096"


function printUsage()
{
  cat <<EOF
Usage: $0 <options>

Generate all PKI test data. Unless --clean is given, only the elements that
don't exist are generated.

Options:
  <no options>
      generate root CA, sub CA ( TSL signer ) and use them to sign TSL templates.
  --clean
      Remove all pre-existing data first, and then start the generation
EOF
}


function get_named_arguments()
{
  local names=",$1,"
  shift

  while [ $# -ge 1 ]; do
    local nameValuePair="$1"
    shift

    local name="${nameValuePair%%=*}"
    local value="${nameValuePair#*=}"
    if [[ "$names" == *",$name,"* ]]; then
      local quotedValue=\'${value//\'/\'\\\'\'}\'
      echo "local $name=$quotedValue;"
    fi
  done
}


function get_arguments_for_key_type()
{
  local keyType="$1"
  if [ -z "$keyType" ]; then
    return
  fi

  case "$keyType" in
    rsa:*)
      echo "-newkey $keyType"
      ;;
    ec:*)
      echo "-newkey ec -pkeyopt ec_paramgen_curve:${keyType:3}"
      ;;
    "")
      echo "$defaultKeyArguments"
      ;;
    *)
      echo "Invalid key type argument: \"$keyType\"" >&2
      exit 1
      ;;
  esac
}


function init_ca_directory()
{
  local caDir="$1"

  mkdir "$caDir"
  (cd "$caDir" && {
    mkdir certsdb certreqs crl private certificates
    touch index.txt
    echo "unique_subject = yes" > index.txt.attr
    echo dd if=/dev/urandom bs=8 count=1 status=none | xxd -u -p -g8 > serial
  })
}


function generate_root_ca()
{
  local caName="$1"
  local keyType="$2"

  local caDir="$testDataDir/$caName"

  local keyArguments  # separate assignment to have exit code != 0 take effect
  keyArguments=$(get_arguments_for_key_type "$keyType")

  if [ -e "$caDir" ]; then
    echo "CA dir \"$caDir\" already exists, skipping generation."
    return
  fi

  init_ca_directory "$caDir"

  (cd "$caDir" && {
    # create private key and certificate signing request
    echo -e '\n\n\n\n\nExample Inc. Root CA\n' \
      | openssl req -new $keyArguments \
        -keyout private/ca_key.pem \
        -out ca_req.pem -config "$opensslConfig" \
        -nodes
    chmod 0600 private/ca_key.pem

    # create the certificate
    echo -e 'y\ny\n' \
      | openssl ca -out ca.pem -days 3653 \
        -keyfile private/ca_key.pem -selfsign \
        -extensions v3_ca_has_san -config "$opensslConfig" \
        -infiles ca_req.pem

    openssl x509 -outform der -in ca.pem -out ca.der

    # the certificate chain to the CA certificate
    ln -s ca.pem ca_cert_chain.pem
  })

  caMap[$caName]="$caName"
}


function generate_certificate()
{
  local caName="$1"
  local alias=$2
  local commonName="$3"
  local extensions="$4"
  local keyType="$5"
  shift 5

  eval "$(get_named_arguments subjectAltName "$@")"

  local caDir="$testDataDir/$caName"

  local keyArguments  # separate assignment to have exit code != 0 take effect
  keyArguments=$(get_arguments_for_key_type "$keyType")

  (cd "$caDir" && {
    local outDir=certificates/$alias

    if [ -e "$outDir" ]; then
      echo "Certificate directory \"$outDir\" already exists, skipping generation."
      return
    fi

    local keyFile="$outDir/${alias}_key.pem"
    local certFile="$outDir/${alias}.pem"
    local certDerFile="$outDir/${alias}.der"
    local csrFile

    if [[ "$OSTYPE" == "darwin"* ]]; then
      csrFile=$(mktemp "certreqs/${alias}-XXXXXXXX")
    else
      csrFile=$(mktemp "certreqs/${alias}-XXXXXXXX")
    fi

    # create output directory
    mkdir -p "$outDir"

    local additionalEnvVars=()

    # If a SubjectAltName is given, overrided the value in openssl.cnf.
    if [ -n "$subjectAltName" ]; then
      additionalEnvVars=("SUBJECT_ALT_NAME=$subjectAltName")
    fi

    # create private key and CSR
    echo "Generating key and CSR..."
    echo -e "\n\n\n\n\n$commonName\n\n" \
      | openssl req -new $keyArguments -keyout "$keyFile" \
        -out "$csrFile" -config "$opensslConfig" \
        -nodes

    # sign the certificate
    echo "Signing certificate..."
    echo -e "y\ny\n" \
      | env "${additionalEnvVars[@]}" \
        openssl ca -config "$opensslConfig" \
          -extensions "$extensions" -in "$csrFile" -out "$certFile"

    openssl x509 -outform der -in "$certFile" -out "$certDerFile"
  })

  caMap[$alias]="$caName"
}


function generate_sub_ca()
{
  local parentCaName=$1
  local caName=$2
  local commonName="$3"
  local keyType="$4"
  shift 4

  local parentCaDir="$testDataDir/$parentCaName"
  local caDir="$testDataDir/$caName"

  if [ -e "$caDir" ]; then
    echo "CA dir \"$caDir\" already exists, skipping generation."
    return
  fi

  # create sub CA certificate
  generate_certificate "$parentCaName" "$caName" "$commonName" v3_ca_has_san "$keyType" "$@"

  # initialize sub CA directory
  init_ca_directory "$caDir"

  ln -s "../$parentCaName/certificates/$caName/$caName.pem" "$caDir/ca.pem"
  ln -s "../$parentCaName/certificates/$caName/$caName.der" "$caDir/ca.der"
  ln -s "../../$parentCaName/certificates/$caName/${caName}_key.pem" "$caDir/private/ca_key.pem"

  # the certificate chain to the CA certificate
  cat "$caDir/ca.pem" "$parentCaDir/ca_cert_chain.pem" > "$caDir/ca_cert_chain.pem"
}


function generate_tsl()
{
  local tslSignerName=$1
  local caName=$2
  local tslName=$3

  # check, if TSL already exists
  local tslDir="$testDataDir/tsl"
  mkdir -p "$tslDir"

  local tslSigned="$tslDir/$tslName"
  if [ -e "$tslSigned" ]; then
    echo "TSL file \"$tslSigned\" already exists, skipping generation."
    return
  fi

  # find signer certificate and key
  local signerCaName="${caMap[$tslSignerName]}"
  if [ -z "$signerCaName" ]; then
    echo "Error: no CA for $tslSignerName" >&2
    exit 1
  fi

  local signerCertDir="$testDataDir/$signerCaName/certificates/$tslSignerName"
  local signerKeyFile="$signerCertDir/${tslSignerName}_key.pem"
  local signerCertFile="$signerCertDir/${tslSignerName}.pem"
  if [[ ! -f "$signerKeyFile" || ! -f "$signerCertFile" ]]; then
    echo "Error: no key file or cert file for $tslSignerName" >&2
    exit 1
  fi

  # read the CA certificate
  local caDir="$testDataDir/$caName"
  local caCertificateFile="$caDir/ca.pem"
  local caCertificateBase64
  local tslNextUpdate

  if [[ "$OSTYPE" == "darwin"* ]]; then
    caCertificateBase64="$(openssl x509 -in "$caCertificateFile" -outform der | base64)"
    tslNextUpdate="$(date -j -v +29d '+%Y-%m-%dT%H:%M:%SZ')"
  else
    caCertificateBase64="$(openssl x509 -in "$caCertificateFile" -outform der | base64 --wrap=0)"
    tslNextUpdate="$(date -d '+29 days' '+%Y-%m-%dT%H:%M:%SZ')"
  fi

  if [ -z "$caCertificateBase64" ]; then
    echo "Error: failed to read CA certificate $caCertificateFile" >&2
    exit 1
  fi

  # create final unsigned TSL
  local tslUnsigned="$tslDir/unsigned_$tslName"
  local tslTemplate="$inputDataDir/template_$tslName"
  sed -e "s@%CERTIFICATE_DER_BASE64%@$caCertificateBase64@" -e "s@%TSL_NEXT_UPDATE%@$tslNextUpdate@" "$tslTemplate" > "$tslUnsigned"

  # sign TSL
  xmlsec1 --sign --privkey-pem "$signerKeyFile,$signerCertFile" --output "$tslSigned" "$tslUnsigned"
}


function sign()
{
  local dataName="$1"
  local name="$2"
  local data="$3"

  shift 3

  local hashTwiceCommand=cat

  while [ $# -ge 1 ]; do
    if [ "$1" = "--hash-twice" ]; then
      # This supports a peculiarity of the record removal protocol -- the data
      # data to be signed is hashed once before hashing+signing.
      hashTwiceCommand="openssl dgst -sha256 -binary"
    else
      echo "error: sign: unsupported option: $1"
    fi
    shift
  done

  local caName="${caMap[$name]}"
  if [ -z "$caName" ]; then
    echo "Error: no CA for $name" >&2
    exit 1
  fi

  local certDir="$testDataDir/$caName/certificates/$name"
  local keyFile="$certDir/${name}_key.pem"
  local certFile="$certDir/${name}.pem"
  if [[ ! -f "$keyFile" || ! -f "$certFile" ]]; then
    echo "Error: no key file or cert file for $name" >&2
    exit 1
  fi

  local signatureDir="$testDataDir/signatures"
  mkdir -p "$signatureDir"

  local outFile="$signatureDir/$dataName.txt"

  if [ -e "$outFile" ]; then
    echo "Signature file \"$outFile\" already exists, skipping generation."
    return
  fi

  if [[ "$OSTYPE" == "darwin"* ]]; then
    openssl dgst -sha256 -sign "$keyFile" \
        <(echo -n "$data" | $hashTwiceCommand) \
      | base64 > "$outFile"
  else
    openssl dgst -sha256 -sign "$keyFile" \
        <(echo -n "$data" | $hashTwiceCommand) \
      | base64 --wrap=0 > "$outFile"
  fi
}


function scriptExit()
{
  if $success; then
    echo "Test data created successfully."
  else
    echo "Error: Creating test data FAILED!" >&2
  fi
}


# Print a success or error message when the script terminates.
success=false
trap scriptExit EXIT

# parse arguments
removeExistingData=false

while [ $# -ge 1 ]; do
  case "$1" in
    --clean)
      removeExistingData=true
      shift
      ;;
    -h|--help)
      printUsage
      exit
      ;;
    *)
      printUsage >&2
      exit 1
      ;;
  esac
done

if $removeExistingData; then
  rm -rf "$testDataDir"
fi
mkdir -p "$testDataDir"

declare -A caMap

generate_root_ca root_ca_ec ec:brainpoolP256r1
generate_sub_ca root_ca_ec sub_ca1_ec "Example Inc. Sub CA EC 1" ec:brainpoolP256r1
generate_certificate sub_ca1_ec tsl_signer_ec "TSL signer" tsl_signer_cert ec:brainpoolP256r1 \
    subjectAltName=email:admin@example.com

# The Gematik TSLs to generate from templates
generate_tsl tsl_signer_ec sub_ca1_ec "TSL_valid.xml"
generate_tsl tsl_signer_ec sub_ca1_ec "TSL_no_ocsp_mapping.xml"
generate_tsl tsl_signer_ec sub_ca1_ec "TSL_outdated.xml"

success=true
