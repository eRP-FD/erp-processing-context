/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/client/request/ClientRequestWriter.hxx"

#include "erp/beast/BoostBeastStringWriter.hxx"
#include "erp/client/request/ValidatedClientRequest.hxx"
#include "erp/server/SslStream.hxx"
#include "erp/common/BoostBeastMethod.hxx"


ClientRequestWriter::ClientRequestWriter(
    const ValidatedClientRequest& request)
    : mRequest(request),
      mBeastRequest(),
      mSerializer(mBeastRequest)
{
    mSerializer.limit(Constants::DefaultBufferSize);
    mBeastRequest.method(toBoostBeastVerb(request.getHeader().method()));
    mBeastRequest.version(request.getHeader().version());
    mBeastRequest.target(request.getHeader().target());
    mBeastRequest.keep_alive(request.getHeader().keepAlive());
    mBeastRequest.body() = mRequest.getBody();

    for (const auto& field : mRequest.getHeader().headers())
        mBeastRequest.set(field.first, field.second);
}


std::string ClientRequestWriter::toString (void)
{
    return BoostBeastStringWriter::serializeRequest(mRequest.getHeader(), mRequest.getBody());
}
