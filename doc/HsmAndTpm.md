#HSM
The hardware security module is a cluster of specialized servers that host private keys and allow 
cryptographic operations based on these keys. If we need for example a symmetric key derived from such a private key then
we have to contact the HSM (cluster), authenticate us and provide the derivation data. The HSM will respond with the symmetric key.

Our implementation of an HSM client is based on a C library hsmclient. The C++ implementation may look a bit more complex
than seems necessary on first glance. But that is necessary in order to allow for a mock version of the client.

The classes and their roles are
- HsmSession
  The HsmSession represents an established connection (successfully logged on) to a remote HSM server.
  Its TeeToken, which is used in every call to the HSM to authenticate the session, is provided by the HsmFactory.
  - Created by HsmFactory but owned by the HsmPool.
- HsmFactory
  - Creates HsmSession objects by connecting to an HSM and negotiating a TeeToken.
  - TeeTokens are renewed periodically.
  - HsmSession objects are expected to be used for only a short time span so that
     - pooling them in the HsmPool becomes possible while giving each user of an HsmSession object exclusive use while
       in its possession.
     - the TeeToken must not renewed while in use by an HsmSession
  - Owned by the HsmPool  
- HsmPool
  - Maintains one or more HsmSession objects. 
  - HsmSession objects whose TeeToken is about to expire are retired and removed from the pool after adding a new
    instance with a fresh TeeToken.
  - Owned by PcServiceContext. 
- BlobCache
  - Cache for the BlobDatabase that optimizes for an expected small number of entries (10 to 20) and almost only read operations.
  - Ownership is shared between the HsmPool  and the service context of the enrolment service.  
- BlobDatabase
  - Database for blobs that are set via the enrolment service.
  - Owned by the BlobCache  

The main interface is HsmSession, which represents a successfully connected and authenticated connection. HsmSession objects
can be obtained from the HsmPool which is accessible via the ServiceContext. The objects returned by HsmPool are thin wrappers
around HsmSession objects that will release the sessions to the pool automatically when the wrapper is destroyed.

