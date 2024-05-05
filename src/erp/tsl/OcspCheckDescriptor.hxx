#ifndef ERP_PROCESSING_CONTEXT_OCSPCHECKDESCRIPTOR_HXX
#define ERP_PROCESSING_CONTEXT_OCSPCHECKDESCRIPTOR_HXX

#include "erp/crypto/OpenSslHelper.hxx"


/**
 * The descriptor describes how the OCSP check should be done.
 */
class OcspCheckDescriptor
{
public:

    /**
     * The mode for the OCSP check.
     * Independent from the used mode any received and successfully validated OCSP response
     * with status `good` or `revoked` is stored in cache.
     */
    enum OcspCheckMode
    {
        /**
         * Force OCSP request sending an OCSP Responder, if the request fails report error.
         */
        FORCE_OCSP_REQUEST_STRICT,

        /**
         * Force OCSP request sending on OCSP Responder, if the request fails,
         * try to use cache as fallback.
         */
        FORCE_OCSP_REQUEST_ALLOW_CACHE,

        /**
         * Allow to use provided OCSP response or cached one directly, without sending of OCSP request.
         * If OCSP Response is provided, it must be valid, or an error must be returned.
         * If there is no provided OCSP response, the cache can be used.
         * If no valid cache is available an OCSP request should be sent.
         */
        PROVIDED_OR_CACHE,

        /**
         * Return cached OCSP response.
         * If no Response is cached return an error
         **/
        CACHED_ONLY,

    };

    /**
     * The time settings for OCSP response validity check.
     */
    class TimeSettings
    {
    public:
        /**
         * Optional timepoint to be used for OCSP response validity checks, if it is not set the current timepoint is used.
         */
        std::optional<std::chrono::system_clock::time_point> referenceTimePoint;

        /**
         * The OCSP grace period to use in the scenario.
         */
        std::chrono::system_clock::duration gracePeriod;
    };

    /**
     * The mode to use for the OCSP check.
     */
    OcspCheckMode mode;

    /**
     * The time settings to use for OCSP response validity check.
     */
    TimeSettings timeSettings;

    /**
     * The provided OCSP response to be used in check if any.
     */
    OcspResponsePtr providedOcspResponse;
};


#endif
