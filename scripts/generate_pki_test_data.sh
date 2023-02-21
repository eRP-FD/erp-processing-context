#!/usr/bin/env bash

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
testDataDirRel=resources/test/generated_pki
testDataDir="$rootDir/$testDataDirRel"
inputDataDir=$(dirname "$scriptDir")/resources/test/generated_pki_input
opensslConfig="$inputDataDir/openssl.cnf"

defaultKeyArguments="-newkey rsa:4096"

# TSL and certificates generation modes, meaning depends from context, please see the related method
normal="normal"
outdated="outdated"
multipleNewCA="multipleNewCA"
brokenNewCA="brokenNewCA"
validBeforeOutdated="validBeforeOutdated"


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
  --output-dir=<output_dir> the data will be generated into \$(output_dir)/resources/test/generated_pki
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
      | "$OPENSSL" req -new $keyArguments \
        -keyout private/ca_key.pem \
        -out ca_req.pem -config "$opensslConfig" \
        -nodes
    chmod 0644 private/ca_key.pem

    # create the certificate
    echo -e 'y\ny\n' \
      | "$OPENSSL" ca -out ca.pem -days 3653 \
        -keyfile private/ca_key.pem -selfsign \
        -extensions v3_ca_has_san -config "$opensslConfig" \
        -infiles ca_req.pem

    "$OPENSSL" x509 -outform der -in ca.pem -out ca.der

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
  # generation modes are:
  # normal - normal validity timeframe
  # outdated - outdated validity timeframe
  # validBeforeOutdated - valid currently and validity timeframe starts before outdated validity timeframe ends
  local generationMode="$6"
  shift 6

  if [ "$generationMode" != "$normal" ] \
    && [ "$generationMode" != "$outdated" ] \
    && [ "$generationMode" != "$validBeforeOutdated" ] ; then
      echo "Wrong generation mode '$generationMode' is provided for certificate $alias"
      exit 1
  fi

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
      | "$OPENSSL" req -new $keyArguments -keyout "$keyFile" \
        -out "$csrFile" -config "$opensslConfig" \
        -nodes
    chmod 0644 "$keyFile" "$csrFile"

    # sign the certificate
    echo "Signing certificate..."

    local oudatedStartDate="20210101120000Z"
    local beforeOutdatedStartDate="20210201120000Z"
    local oudatedEndDate="20220101120000Z"

    if [ "$generationMode" == "$normal" ]; then
      echo -e "y\ny\n" \
        | env "${additionalEnvVars[@]}" \
          "$OPENSSL" ca -config "$opensslConfig" \
            -extensions "$extensions" -in "$csrFile" -out "$certFile"
    elif [ "$generationMode" == "$outdated" ]; then
      echo -e "y\ny\n" \
        | env "${additionalEnvVars[@]}" \
          "$OPENSSL" ca -config "$opensslConfig" \
            -extensions "$extensions" -startdate "$oudatedStartDate" -enddate "$oudatedEndDate" -in "$csrFile" -out "$certFile"
    else # "$generationMode" == "$validBeforeOutdated"
      local validEndDate
      if [[ "$OSTYPE" == "darwin"* ]]; then
        validEndDate="$(date -j -v +29d '+%Y%m%d%H%M%SZ')"
      else
        validEndDate="$(date -d '+29 days' '+%Y%m%d%H%M%SZ')"
      fi

      echo -e "y\ny\n" \
        | env "${additionalEnvVars[@]}" \
          "$OPENSSL" ca -config "$opensslConfig" \
            -extensions "$extensions" -startdate "$beforeOutdatedStartDate" -enddate "$validEndDate" -in "$csrFile" -out "$certFile"
    fi

    "$OPENSSL" x509 -outform der -in "$certFile" -out "$certDerFile"
  })

  caMap[$alias]="$caName"
}


function generate_sub_ca()
{
  local parentCaName=$1
  local caName=$2
  local commonName="$3"
  local keyType="$4"
  local generationMode="$5"
  shift 5

  local parentCaDir="$testDataDir/$parentCaName"
  local caDir="$testDataDir/$caName"

  if [ -e "$caDir" ]; then
    echo "CA dir \"$caDir\" already exists, skipping generation."
    return
  fi

  # create sub CA certificate
  generate_certificate "$parentCaName" "$caName" "$commonName" v3_ca_has_san "$keyType" "$generationMode" "$@"

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
  local templateName=$3
  local tslName=$4
  # generation modes are:
  # normal - normal TSL
  # outdated - outdated TSL
  # multipleNewCA - TSL with multiple new trust anchor
  # brokenNewCA - TSL with broken new trust anchor
  local generationMode=$5

  if [ "$generationMode" != "$normal" ] \
    && [ "$generationMode" != "$outdated" ] \
    && [ "$generationMode" != "$multipleNewCA" ] \
    && [ "$generationMode" != "$brokenNewCA" ] ; then
      echo "Wrong generation mode '$generationMode' is provided for TSL name $tslName"
      exit 1
  fi

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

  # bna related certificates
  local bnaSignerCaFile="$testDataDir/bna_signer_ca_ec/ca.pem"
  local bnaSignerCaCertificateBase64
  local bnaSignerFile="$testDataDir/bna_signer_ca_ec/certificates/bna_signer_ec/bna_signer_ec.pem"
  local bnaSignerCertificateBase64

  # read the CA certificate
  local caDir="$testDataDir/$caName"
  local caCertificateFile="$caDir/ca.pem"
  local caCertificateBase64
  local tslNextUpdate
  local tslId
  local tslSequenceNumber

  if [[ "$OSTYPE" == "darwin"* ]]; then
    caCertificateBase64="$($OPENSSL x509 -in "$caCertificateFile" -outform der | base64)"
    bnaSignerCaCertificateBase64="$($OPENSSL x509 -in "$bnaSignerCaFile" -outform der | base64)"
    bnaSignerCertificateBase64="$($OPENSSL x509 -in "$bnaSignerFile" -outform der | base64)"
  else
    caCertificateBase64="$($OPENSSL x509 -in "$caCertificateFile" -outform der | base64 --wrap=0)"
    bnaSignerCaCertificateBase64="$($OPENSSL x509 -in "$bnaSignerCaFile" -outform der | base64 --wrap=0)"
    bnaSignerCertificateBase64="$($OPENSSL x509 -in "$bnaSignerFile" -outform der | base64 --wrap=0)"
  fi

  if [ -z "$caCertificateBase64" ]; then
    echo "Error: failed to read CA certificate $caCertificateFile" >&2
    exit 1
  fi

  if [ "$generationMode" == "$outdated" ]; then
    # outdated TSL
    tslId="ID31028220210914152510Z"
    tslSequenceNumber="10281"
    if [[ "$OSTYPE" == "darwin"* ]]; then
      tslNextUpdate="$(date -j -v -2d '+%Y-%m-%dT%H:%M:%SZ')"
    else
      tslNextUpdate="$(date -d '-2 days' '+%Y-%m-%dT%H:%M:%SZ')"
    fi
  else
    # other modes
    tslId="ID31028220210914152511Z"
    tslSequenceNumber="10282"
    if [[ "$OSTYPE" == "darwin"* ]]; then
      tslNextUpdate="$(date -j -v +29d '+%Y-%m-%dT%H:%M:%SZ')"
    else
      tslNextUpdate="$(date -d '+29 days' '+%Y-%m-%dT%H:%M:%SZ')"
    fi
  fi

  # create final unsigned TSL
  local tslUnsigned="$tslDir/unsigned_$tslName"
  local tslTemplate="$inputDataDir/$templateName"

  if [[ "$generationMode" == "$normal" || "$generationMode" == "$outdated" ]]; then
    sed -e "s@%CERTIFICATE_DER_BASE64%@$caCertificateBase64@" \
        -e "s@%BNA_SIGNER_CERTIFICATE_DER_BASE64%@$bnaSignerCertificateBase64@" \
        -e "s@%BNA_SIGNER_CA_DER_BASE64%@$bnaSignerCaCertificateBase64@" \
        -e "s@%TSL_NEXT_UPDATE%@$tslNextUpdate@" \
        -e "s@%TSL_SYSTEM_ID%@$tslId@" \
        -e "s@%TSL_SEQUENCE_NUMBER%@$tslSequenceNumber@" \
        "$tslTemplate" > "$tslUnsigned"
  elif [ "$generationMode" == "$multipleNewCA" ]; then
    sed -e "s@%CERTIFICATE_DER_BASE64%@$caCertificateBase64@" \
        -e "s@%BNA_SIGNER_CERTIFICATE_DER_BASE64%@$bnaSignerCertificateBase64@" \
        -e "s@%BNA_SIGNER_CA_DER_BASE64%@$bnaSignerCaCertificateBase64@" \
        -e "s@%TSL_NEXT_UPDATE%@$tslNextUpdate@" \
        -e "s@%TSL_SYSTEM_ID%@$tslId@" \
        -e "s@%TSL_SEQUENCE_NUMBER%@$tslSequenceNumber@" \
        -e "s@</TrustServiceProviderList>@$(<"$inputDataDir/TSL_NewCAServerProviders.xml")</TrustServiceProviderList>@" \
        "$tslTemplate" > "$tslUnsigned"
  else # "$generationMode" == "$brokenNewCA"
    sed -e "s@%CERTIFICATE_DER_BASE64%@$caCertificateBase64@" \
        -e "s@%BNA_SIGNER_CERTIFICATE_DER_BASE64%@$bnaSignerCertificateBase64@" \
        -e "s@%BNA_SIGNER_CA_DER_BASE64%@$bnaSignerCaCertificateBase64@" \
        -e "s@%TSL_NEXT_UPDATE%@$tslNextUpdate@" \
        -e "s@%TSL_SYSTEM_ID%@$tslId@" \
        -e "s@%TSL_SEQUENCE_NUMBER%@$tslSequenceNumber@" \
        -e "s@</TrustServiceProviderList>@$(<"$inputDataDir/TSL_BrokenNewCAServerProvider.xml")</TrustServiceProviderList>@" \
        "$tslTemplate" > "$tslUnsigned"
  fi

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
      hashTwiceCommand="$OPENSSL dgst -sha256 -binary"
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
    "$OPENSSL" dgst -sha256 -sign "$keyFile" \
        <(echo -n "$data" | $hashTwiceCommand) \
      | base64 > "$outFile"
  else
    "$OPENSSL" dgst -sha256 -sign "$keyFile" \
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

for i in "$@"; do
  case $i in
    --clean)
      removeExistingData=true
      ;;
    --output-dir=*)
      root="${i#*=}"
      if [ -z "$root" ]; then
        printUsage
        exit 1
      fi
      testDataDir="$root/$testDataDirRel"
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
generate_sub_ca root_ca_ec sub_ca1_ec "Example Inc. Sub CA EC 1" ec:brainpoolP256r1 $normal
generate_sub_ca root_ca_ec bna_signer_ca_ec "Example Inc. BNA signer CA EC" ec:brainpoolP256r1 $normal
generate_sub_ca root_ca_ec outdated_ca_ec "Example Inc. outdated CA EC" ec:brainpoolP256r1 $outdated
generate_certificate sub_ca1_ec tsl_signer_ec "TSL signer" tsl_signer_cert ec:brainpoolP256r1 $normal \
    subjectAltName=email:admin@example.com
generate_certificate bna_signer_ca_ec bna_signer_ec "BNA signer" bna_signer_cert ec:brainpoolP256r1 $normal \
    subjectAltName=email:admin@example.com
generate_certificate outdated_ca_ec qes_cert1_ec "Example Inc. Test QES Certificate" qes_cert1 ec:brainpoolP256r1 \
    $validBeforeOutdated subjectAltName=email:admin@example.com
generate_certificate outdated_ca_ec qes_cert2_ec "Example Inc. Test QES Certificate Invalid" qes_cert2 ec:brainpoolP256r1 \
    $normal subjectAltName=email:admin@example.com
generate_certificate sub_ca1_ec qes_cert_ec_hp_enc "Example Inc. Test QES Certificate HP ENC" qes_cert_hp_enc ec:brainpoolP256r1 \
    $normal subjectAltName=email:admin@example.com
generate_certificate sub_ca1_ec tsl_signer_wrong_key_usage_ec "TSL signer 2" tsl_signer_cert_wrong_key_usage ec:brainpoolP256r1 $normal \
    subjectAltName=email:admin@example.com
generate_certificate sub_ca1_ec smc_b_osig_ec "SMC-B Osig Signer" smc_b_osig ec:brainpoolP256r1 $normal \
    subjectAltName=email:admin@example.com

# The Gematik TSLs to generate from templates
generate_tsl tsl_signer_ec sub_ca1_ec "template_TSL_valid.xml" "TSL_valid.xml" $normal
generate_tsl tsl_signer_ec sub_ca1_ec "template_TSL_no_ocsp_mapping.xml" "TSL_no_ocsp_mapping.xml" $normal
generate_tsl tsl_signer_ec sub_ca1_ec "template_TSL_valid.xml" "TSL_outdated.xml" $outdated
generate_tsl tsl_signer_ec sub_ca1_ec "template_TSL_valid.xml" "TSL_multiple_new_cas.xml" $multipleNewCA
generate_tsl tsl_signer_ec sub_ca1_ec "template_TSL_valid.xml" "TSL_broken_new_ca.xml" $brokenNewCA
generate_tsl bna_signer_ec outdated_ca_ec "template_BNA_EC_valid.xml" "BNA_EC_valid.xml" $normal
generate_tsl tsl_signer_wrong_key_usage_ec sub_ca1_ec "template_TSL_valid.xml" "TSL_wrongSigner.xml" $normal

success=true
