/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/request/ClientRequestWriter.hxx"

#include "shared/beast/BoostBeastStringWriter.hxx"
#include "shared/network/client/request/ValidatedClientRequest.hxx"
#include "shared/network/connection/SslStream.hxx"
#include "shared/beast/BoostBeastMethod.hxx"


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
