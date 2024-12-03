/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "SubscriptionPostHandler.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/crypto/Jws.hxx"
#include "erp/model/Subscription.hxx"
#include "shared/util/Configuration.hxx"

#include <boost/format.hpp>
#include <sw/redis++/redis++.h>

namespace
{
    constexpr std::string_view MessageChannel{"communication-channel"};
    constexpr std::string_view Reason{"communication notifications"};
}

SubscriptionPostHandler::SubscriptionPostHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::POST_Subscription, allowedProfessionOiDs)
{
}


void SubscriptionPostHandler::handleRequest(PcSessionContext& session)
{
    A_22364.start("Prepare response");
    using namespace std::chrono;

    const Header& header = session.request.header();
    TVLOG(1) << name() << ": processing request to " << header.target();

    try
    {
        auto subscription = parseAndValidateRequestBody<model::Subscription>(session);
        ErpExpect(subscription.recipientId().length(), HttpStatus::BadRequest, "Missing recipient");
        ErpExpect((subscription.status() == model::Subscription::Status::Requested), HttpStatus::BadRequest,
                  "Request status not supported");

        A_22363.start("Ensure that a LEI can register only for itself.");
        const std::optional<std::string> telematikId = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
        ErpExpect(telematikId.has_value(), HttpStatus::BadRequest, "No valid Telematik-ID in JWT");
        ErpExpect(subscription.recipientId() == telematikId.value(), HttpStatus::Forbidden, "ID mismatch");
        A_22363.finish();

        A_22365.start("Encrypt recipientId with 128-Bit-AES-CMAC and convert to 32 hex byte string.");
        auto& pseudonymManager = session.serviceContext.getTelematicPseudonymManager();
        const std::string subscriptionId = pseudonymManager.sign(subscription.recipientId()).hex();
        A_22365.finish();
        //
        subscription.setSubscriptionId(subscriptionId);
        subscription.setStatus(model::Subscription::Status::Active);
        subscription.setReason(Reason);
        subscription.setChannelType(model::Subscription::ChannelType::websocket);

        // Allow for 12 hours lifetime until expiration.
        const auto tNow = model::Timestamp::now();
        const auto subscriptionExpiration = tNow + hours(12);
        subscription.setEndTime(subscriptionExpiration);

        JoseHeader tokenHeader(JoseHeader::Algorithm::ES256);
        const std::string fmtPayload = R"--(
{
"subscriptionId": "%1%",
"iat":%2%,
"exp":%3%
})--";
        std::string payload =
            boost::str(boost::format(fmtPayload.c_str()) % subscriptionId %
                       std::to_string(tNow.toChronoTimePoint().time_since_epoch().count()) %
                       std::to_string(subscriptionExpiration.toChronoTimePoint().time_since_epoch().count()));

        A_22366.start("Bearer token is a serialized JWS structure.");
        auto privateKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(
                                                                        SafeString{Configuration::instance().getStringValue(ConfigurationKey::SERVICE_SUBSCRIPTION_SIGNING_KEY)});
        Jws token(tokenHeader, payload, privateKey);

        subscription.setAuthorizationToken(token.compactSerialized());
        A_22366.finish();

        makeResponse(session, HttpStatus::OK, &subscription);
        A_22364.finish();
    }
    catch (const model::ModelException& e)
    {
        TVLOG(1) << "ModelException: " << e.what();
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Error during request parsing.", e.what());
    }
}

void SubscriptionPostHandler::publish(SessionContext& sessionContext, const model::TelematikId& recipient)
{
    try
    {
        auto& serviceContext{sessionContext.serviceContext};
        auto redis = serviceContext.getRedisClient();
        auto& pseudonymManager = serviceContext.getTelematicPseudonymManager();
        auto utcToday = date::floor<date::days>(sessionContext.sessionTime().toChronoTimePoint());
        // ensureKeysUptodateForDay is probably redundant, but it's quick when the keys are up-to-date:
        pseudonymManager.ensureKeysUptodateForDay(utcToday);
        if (pseudonymManager.withinGracePeriod(utcToday))
        {
            redis->publish(MessageChannel, pseudonymManager.sign(0, recipient.id()).hex());
            redis->publish(MessageChannel, pseudonymManager.sign(1, recipient.id()).hex());
        }
        else
        {
            // key rotation happens immediately after the grace-period ends
            // therefore key 0 is always the current key and key 1 is now the next key to become valid:
            redis->publish(MessageChannel, pseudonymManager.sign(0, recipient.id()).hex());
        }
    }
    catch (const sw::redis::Error& exception)
    {
        JsonLog(LogId::INFO, JsonLog::makeVLogReceiver(0))
            .message(std::string{"Redis publish failed: "} + std::string{exception.what()});
    }
}
