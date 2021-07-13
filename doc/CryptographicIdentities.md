# Cryptographic Identities, public and private keys and certificates

## VAU / ID.FD.ENC / ICIES
aka ECIES in the HSM world<br>
aka E-Rezept-VAU-Zertifikat or identity that belongs to /VAUCertificate in gemSpec_Krypt

Public key is exposed via the /VAUCertificate endpoint (it's OCSP via /VAUCertificateOCSPResponse).
Private key will be known by the processing context. However, for production that is not yet implemented.

See A_20160-01 for more information.

HsmSession::vauEcies128 is the PC's front end to the HSM functionality for ecdh (elliptic curve diffie hellman) and 
hkdf (hash based key derivation function) with the private ECIES key. This is initiated by the VauRequestHandler to 
obtain the symmetric key to decrypt incoming requests.


## Persistence keys
 
Persistence keys, as specified by e.g. A_19700.
 
Keys are derived by the HSM based on encrypted cryptographic material that is stored in the blob database.
There are separate entries for resources Task, Communication and Audit Log. 
Cryptographic blobs are provided via the enrolment API.


## IDP signer certificate

Used to verify access tokens.
The IDP signer certificate is renewed every hour and stored in PcServiceContext.idp.
The private key is not accessible by the processing context.

Verification of access token is also initiated in the VauRequestHandler. 


## QES signer identity

Used to sign e.g. MedicationDispense receipt.


# Access

HsmSession is accessible via PcServiceContext.getHsmPool().acquire().session()

## VAU / ID.FD.ENC / ICIES
private key: indirectly via HsmSession (vauEcies128)
public key: HsmSession (getEciesPublicKey)
 

## Persistence keys
private keys: indirectly via HsmSession (deriveTaskPersistenceKey, etc.)

## IDP signer certificate
public key certificate: PcServiceContext.idp.getCertificate()

## QES signer identity / C.FD.SIG
?

# Access in Tests

As tests have to simulte the "other" side of cryptography, they sometimes need access to mock private keys etc.

## IDP signer certificate
private key: configuration key TEST_IDP_PRIVATE_KEY
public key: configuration key TEST_IDP_PUBLIC_KEY

```
    const SafeString publicKeyPem = TestConfiguration().getSafeStringValue(TestConfigurationKey::TEST_IDP_PUBLIC_KEY);
    const SafeString privateKeyPem = TestConfiguration().getSafeStringValue(TestConfigurationKey::TEST_IDP_PRIVATE_KEY);
    const shared_EVP_PKEY publicKey = EllipticCurveUtils::pemToPublicKey(publicKeyPem);
    const shared_EVP_PKEY privateKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(privateKeyPem);
```
