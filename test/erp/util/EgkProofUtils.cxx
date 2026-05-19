#include "test/erp/util/EgkProofUtils.hxx"

#include "erp/pc/PcServiceContext.hxx"
#include "shared/compression/Deflate.hxx"
#include "shared/enrolment/VsdmHmacKey.hxx"
#include "shared/util/Random.hxx"
#include "shared/util/Base64.hxx"


std::string createEncodedPnw(std::optional<std::string> pnwPzNumberInput)
{
    std::string pnwXml{
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><PN xmlns="http://ws.gematik.de/fa/vsdm/pnw/v1.0" CDM_VERSION="1.0.0"><TS>20230303111110</TS>)"};
    if (pnwPzNumberInput.has_value())
    {
        pnwXml += "<E>1</E><PZ>" + pnwPzNumberInput.value() + "</PZ>";
    }
    else
    {
        pnwXml += "<E>3</E><EC>12101</EC>";
    }
    pnwXml += "</PN>";
    const auto gzippedPnw = Deflate().compress(pnwXml, Compression::DictionaryUse::Undefined);

    return Base64::encode(gzippedPnw);
}


void enrolKey(PcServiceContext& serviceContext, VsdmHmacKey& keyPackage) {

    const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);
    keyPackage.setPlainTextKey(Base64::encode(key));

    auto hsmPoolSession = serviceContext.getHsmPool().acquire();
    auto& hsmSession = hsmPoolSession.session();
    try {
        keyPackage.setPlainTextKey(Base64::encode(
            serviceContext.getVsdmKeyCache().getKey(keyPackage.operatorId(), keyPackage.version())));
    }
    catch (const std::exception&)
    {
        const ErpVector vsdmKeyData = ErpVector::create(keyPackage.serializeToString());
        ErpBlob vsdmKeyBlob = hsmSession.wrapRawPayload(vsdmKeyData, 0);
        auto& vsdmBlobDb = serviceContext.getVsdmKeyBlobDatabase();
        VsdmKeyBlobDatabase::Entry blobEntry;
        blobEntry.operatorId = keyPackage.operatorId();
        blobEntry.version = keyPackage.version();
        blobEntry.createdDateTime = model::Timestamp::now().toChronoTimePoint();
        blobEntry.blob = vsdmKeyBlob;
        vsdmBlobDb.storeBlob(std::move(blobEntry));
    }
}
