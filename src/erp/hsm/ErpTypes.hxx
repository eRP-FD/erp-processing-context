#ifndef ERP_PROCESSING_CONTEXT_ERPBLOCK_HXX
#define ERP_PROCESSING_CONTEXT_ERPBLOCK_HXX

#include "erp/util/Expect.hxx"
#include "erp/util/SafeString.hxx"

#include <array>
#include <vector>


// These constants are defined in the C header of the HSM client. But as this header is not available on all
// platforms and to introduce type safety, we define them here as static variables.
constexpr size_t Sha1Length = 20; // bytes
constexpr size_t TpmObjectNameLength = 34; // bytes
constexpr size_t Aes256Length = 32; // bytes == 256 bit
constexpr size_t Aes128Length = 16; // bytes == 128 bit
constexpr size_t MaxBuffer = 2048;
constexpr size_t MaxRndBytes = 320;
constexpr size_t TeeTokenLifeTimeSeconds = 30 * 60;



/**
 * Represent an unformatted block of binary data. This class is a slightly C++ified version of the C structure ERPBlob
 * of the HSM client.
 *
 * Each blob has a generation number which defines which key must be used by the HSM to decrypt it.
 * Generation is increased every third month. Only the keys for the current and the last generation are kept by the
 * HSM. Older blobs can not be decrypted anymore.
 * Note that the above is not strictly adhered to. At least in tests a generation number can be used multiple times
 * for blobs of the same type.
 *
 * Corresponds to struct ERPBlob_t in ERP_Client.h
 */
class ErpBlob
{
public:
    using GenerationId = uint32_t;

    SafeString data;
    GenerationId generation;

    ErpBlob (void);
    ErpBlob (
        std::vector<uint8_t>&& data,
        GenerationId generation);
    ErpBlob (
        std::string_view data,
        GenerationId generation);
    ErpBlob (const ErpBlob& other);
    ErpBlob (ErpBlob&& other) noexcept = default;

    static ErpBlob fromCDump (std::string_view base64);

    ErpBlob& operator= (const ErpBlob& other);
    ErpBlob& operator= (ErpBlob&& other) noexcept = default;
};


/**
 * A fixed length array that is used to transport binary data.
 */
template<size_t L>
class ErpArray : public std::array<uint8_t, L>
{
public:
    /**
     * If you use `data` with a string literal then you have to take care of zero bytes.
     * If they are to be included then supply a string_view that has been created explicitly with an
     * explicit length like this
     *
     *     ErpArray<5>(std::string_view("test\0", 5))
     *
     * while this one leads to an error
     *
     *     ErpArray<5>("test\0")
     *
     * because here the '\0' is a string terminator that creates a string_view object that points to the string "test".
     *
     *
     * This is not a constructor to not hide std::vectors constructors (and to avoid too many using statements).
     */
    static ErpArray create (const std::string_view data);
};


class ErpVector : public std::vector<uint8_t>
{
public:
    using std::vector<uint8_t>::vector;
    explicit ErpVector(const std::vector<uint8_t>&);
    static ErpVector create (const std::string_view data);

    bool operator== (const ErpVector& other) const;
    size_t hash (void) const;

    bool startsWith (const ErpVector& vector) const;
    ErpVector tail (size_t length) const;

    template<size_t L>
    ErpArray<L> toArray (void) const;
};


/**
 * Note that these values are serialized with their numerical values to the blob database.
 * => if a numerical values changes or a new type is added then update the database table erp.blob
 */
enum class BlobType : int8_t
{
    EndorsementKey             =  1,
    AttestationPublicKey       =  2,
    AttestationKeyPair         =  3,
    Quote                      =  4,
    EciesKeypair               =  5,
    TaskKeyDerivation          =  6,
    CommunicationKeyDerivation =  7,
    AuditLogKeyDerivation      =  8,
    KvnrHashKey                =  9,
    TelematikIdHashKey         = 10,
    VauSigPrivateKey           = 11
};

using BlobId = uint32_t;


template<size_t L>
ErpArray<L> ErpArray<L>::create (const std::string_view data)
{
    Expect(data.size() == L, "invalid data size for initialization of array of length " + std::to_string(L));
    ErpArray<L> array;
    std::copy(data.begin(), data.end(), array.begin());
    return array;
}


namespace std
{
    template <>
    struct hash<ErpVector>
    {
        std::size_t operator() (const ErpVector& v) const
        {
            return v.hash();
        }
    };
}


template<size_t L>
ErpArray<L> ErpVector::toArray (void) const
{
    Expect(size() == L, "ErpVector has a wrong size");
    ErpArray<L> array;
    std::copy(begin(), end(), array.begin());
    return array;
}

#endif
