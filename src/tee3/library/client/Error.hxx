/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CLIENT_ERROR_HXX
#define EPA_LIBRARY_CLIENT_ERROR_HXX

#include "library/log/Location.hxx"
#include "library/util/Time.hxx"

#include <optional>
#include <stdexcept>
#include <utility>

#include "shared/network/message/HttpStatus.hxx"

namespace epa
{
//NOLINTBEGIN(bugprone-macro-parentheses)
/** Create a derived class that inherits all constructors. */
#define DEFINE_ERROR_CLASS_DERIVED_FROM(Err, Base)                                                 \
    class Err : public Base                                                                        \
    {                                                                                              \
    public:                                                                                        \
        using Base::Base;                                                                          \
    };
//NOLINTEND(bugprone-macro-parentheses)

class
    _err__Error_Trace; // NOLINT(readability-identifier-naming,bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
struct soap;             // NOLINT(readability-identifier-naming)

struct FailedOperation
{
    /**
     * Construct a FailedOperation.
     *
     * IMPORTANT: All parameters may be shown to the user and must not contain any sensitive
     * information.
     *
     * @param fqdn        This must either be an std::string that is the requested FQDN, or it must
     *                    be an object that has a getFqdn() method for getting the FQDN. For
     *                    example, you can provide a reference to a ClientConnection object from the
     *                    ePA client library.
     * @param operation   This must either be an std::string that is the requested operation, or it
     *                    must be an object that has a getOperation() method for getting the
     *                    operation. For example, you can provide a reference to a UrlPath object
     *                    from the ePA client library.
     * @param httpStatus  The resulting HTTP status code if known.
     * @param errorCode   Any further error code.
     */
    template<class FqdnOrFqdnHolder, class OperationOrOperationHolder>
    FailedOperation(
        const FqdnOrFqdnHolder& fqdn,
        const OperationOrOperationHolder& operation,
        std::optional<HttpStatus> httpStatus = std::nullopt,
        std::optional<std::string> errorCode = std::nullopt,
        std::optional<std::string> errorDetail = std::nullopt)
      : fqdn([](const FqdnOrFqdnHolder& fqdn) {
            if constexpr (std::is_same_v<FqdnOrFqdnHolder, std::string>)
                return fqdn;
            else
                return fqdn.getFqdn();
        }(fqdn)),
        operation([](const OperationOrOperationHolder& operation) {
            if constexpr (std::is_same_v<OperationOrOperationHolder, std::string>)
                return operation;
            else
                return operation.getOperation();
        }(operation)),
        httpStatus(httpStatus),
        errorCode(std::move(errorCode)),
        errorDetail(std::move(errorDetail))
    {
    }

    /**
     * Get a string containing all parameters. This is for debugging or logging.
     */
    std::string toString() const;

    std::string fqdn;
    std::string operation;
    std::optional<HttpStatus> httpStatus;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorDetail;
};


enum class ErrorImpact
{
    /**
     * The current ePA operation should just be cancelled. The session stays valid.
     */
    CancelOperation,

    /**
     * The current ePA operation should not only be cancelled, but also the current ClientSession
     * should consider itself invalid. This mostly applies to the errors from
     * gemSpec_Zugangsgateway, A_15599. But there are also some errors where we consider a logout
     * the best cause of action.
     */
    TerminateSession,

    /**
     * Like TerminateSession, but the FdV should silently re-login and retry the operation without
     * asking the user.
     */
    TerminateSessionAndRetry
};


/**
 * Enumeration that says whether an operations wants to be retried if the telematik event ID is
 * ASSERTION_INVALID (by ErrorImpact::TerminateSessionAndRetry). This is a little bit more safe than
 * a simple bool.
 */
enum class RetryIfAssertionInvalid
{
    No,
    Yes
};


/**
 * This is the base class for errors that are mapped to specific exception/error classes in the FdV
 * SDK. The exception class that escapes from our functions controls which error message will be
 * displayed in the final ePA FdV UI.
 * For example, a CommunicationError might show the dialog "A network error occurred, please try
 * again later." whereas a ValidationError might lead to the message "Please review your input and
 * try again."
 *
 * An overview of our error classes is available here:
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/documentation/Errors.md
 */
class BaseError : public std::runtime_error
{
public:
    BaseError(
        const Location& location,
        const std::string& what,
        std::optional<FailedOperation> failedOperation = std::nullopt,
        ErrorImpact impact = ErrorImpact::CancelOperation);

    int32_t programLocationCode() const;

    const std::optional<FailedOperation>& failedOperation() const;
    void setFailedOperation(FailedOperation failedOperation);

    /**
     * Whether the ClientSession should invalidate itself.
     */
    bool shouldTerminateSession() const;

    /**
     * Whether the FdV wrapper should retry the operation silently. Because shouldTerminateSession()
     * is also true then, the wrapper has to re-login first.
     */
    bool shouldRetryOperation() const;

private:
    friend class TestDataHelper;

    int32_t mProgramLocationCode;
    std::optional<FailedOperation> mFailedOperation;
    ErrorImpact mImpact;
};

/**
 * Implementation error that happened internally in the FdV SDK.
 * Description needs to be determined by the host app.
 *
 * How to handle: Notify IBM.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/InternalErrorException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(InternalError, BaseError);

/**
 * Invalid set of attributes in a model.
 * Description needs to be determined by the host app.
 *
 * How to handle: Notify the user that they provided invalid data.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/ValidationException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(ValidationError, BaseError);

/**
 * The clock time of the insurant's device does not seem to be correct.
 *
 * How to handle: Inform the user that they need to set their device to the correct time.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/TimeSynchronizationException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(TimeSynchronizationError, BaseError);

/**
 * This is passed to `AuthenticationDelegate.didLogOutWithError()` if the app was inactive for too
 * long (>20 minutes in the foreground).
 *
 * How to handle: Depends on the context. Retrying the operation might work.
 * Maybe call `Authentication.extendSession()` to signal user activity.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/InactivityException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(InactivityError, BaseError);

/**
 * Network errors, communication problems between components, protocol errors, etc.
 * Description needs to be determined by the host app.
 *
 * How to handle: Inform the user that the communication with the server was disrupted.
 * Performing the operation again might work.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/CommunicationException.kt
 */
class CommunicationError : public BaseError
{
public:
    using BaseError::BaseError;

    /**
     * Throws an error if the HTTP status is not the required one (according to our APIs).
     *
     * FqdnOrFqdnHolder and OperationOrOperationHolder are template arguments because this file
     * lives in src/library and we have no access to the classes in src/client/state.
     */
    template<class FqdnOrFqdnHolder, class OperationOrOperationHolder>
    static void validateHttpStatus(
        HttpStatus requiredStatus,
        const Location& location,
        const FqdnOrFqdnHolder& fqdn,
        const OperationOrOperationHolder& path,
        HttpStatus status)
    {
        if (status == requiredStatus)
            return;

        switch (status)
        {
            case HttpStatus::Unauthorized: // 401
            case HttpStatus::Forbidden:    // 403
                throw CommunicationError(
                    location,
                    "authentication failure",
                    FailedOperation{fqdn, path, status},
                    ErrorImpact::TerminateSessionAndRetry);
            case HttpStatus::NotFound: // 404
                throw CommunicationError(
                    location, "resource not found", FailedOperation{fqdn, path, status});
            default:
                throw CommunicationError(
                    location, "unexpected HTTP status", FailedOperation{fqdn, path, status});
        }
    }
};

/**
 * No connection to the given host could be opened. The request failed at a lower level than
 * [CommunicationException].
 *
 * How to handle: Inform the user that no connection to the server could be established. Ask them to
 * check their internet connection.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/OfflineException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(OfflineError, CommunicationError);

/**
 * The eGK used to log in is invalid.
 *
 * Text provided as defined by A_15309.
 *
 * How to handle: Display the provided text to the user. Retrying the login will probably not help.
 *
 * If the error persists, users need to contact their insurance company (as stated in the provided
 * error message).
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/InvalidSecurityTokenException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(InvalidSecurityTokenError, CommunicationError);


/**
 * Indicates that a device confirmation was not successful.
 *
 * Happens when the user wants to confirm a device by entering a six-digit confirmation code, but
 * the code or device token do not match. This error offers remainingAttempts to indicate how often
 * the user can retry activating this device.
 *
 * How to handle: Inform the user that they need to retry confirming their device.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/DeviceConfirmationException.kt
 */
class DeviceConfirmationError : public CommunicationError
{
public:
    DeviceConfirmationError(
        const Location& location,
        const std::string& what,
        FailedOperation failedOperation,
        int remainingAttempts);

    const int remainingAttempts;
};


/**
 * Indicates that a device registration was not successful.
 *
 * Happens when the user wants to register a new device, but there have been so many failed
 * confirmation attempts that this function has been temporarily disabled. This error transports the
 * next time at which the user can register their next device.
 *
 * How to handle: Inform the user that they can retry at the given time.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/DeviceRegistrationException.kt
 */
class DeviceRegistrationError : public CommunicationError
{
public:
    DeviceRegistrationError(
        const Location& location,
        const std::string& what,
        FailedOperation failedOperation,
        SystemTime endOfWaitingTime);

    const SystemTime endOfWaitingTime;
};

/**
 * A cryptographic operation has failed. This should not usually happen.
 *
 * How to handle: Inform the user that the communication with the server was disrupted.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/main/src/main/java/com/ibm/epa/client/error/CryptographyException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(CryptographyError, CommunicationError);

/**
 * Error about formalities when doing Crypto operations.
 *
 * This error type is internal and not part of the SDK.
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(CryptoFormalError, CryptographyError);

/**
 * SGD derivation process failed.
 *
 * How to handle: Inform the user that the communication with an SGD server was unsuccessful. The
 * user may not understand "SGD" but it is important that this information is provided in support
 * cases. Performing the operation again might work.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/SgdException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(SgdError, CommunicationError);

/**
 * Intermittent SGD error, process can be restarted.
 *
 * This error type is internal and not part of the SDK.
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(TransientSgdError, SgdError);

/**
 * The current operation has exceeded a time limit.
 *
 * How to handle: Ask the user to restart the operation (later).
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/TimeoutException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(TimeoutError, CommunicationError);

/**
 * Implementation error outside of the FdV SDK, wrong usage of the FdV module (e.g. missing
 * configuration values).
 *
 * How to handle: Don't display anything to the user. Adjust usage of of the ePA SDK.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/ClientErrorException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(ClientError, BaseError);

/**
 * The user tried to log in with a different insurant ID than expected. Most
 * likely, they used a health insurance card of someone else while trying to log
 * in.
 *
 * How to handle: Ask the user to use the identity with the expected ID (KVNR).
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/WrongInsurantIdException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(WrongInsurantIdError, ClientError);

/**
 * Donating or withdrawing documents is only permitted after consent has been given.
 *
 * How to handle: Ask the user to give consent first.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/MissingConsentException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(MissingConsentError, ClientError);

/**
 * PermissionRepository will throw this when adding or replacing a permission for which the subject
 * (HCPO) data is not valid.
 *
 * This is a very common situation in the RU/RU2 environments, where the directory service contains
 * a lot of invalid data.
 *
 * How to handle: Inform the user and recommend retrying at a later date.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/InvalidPermissionSubjectException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(InvalidPermissionSubjectError, BaseError);

/**
 * The user aborted the running operation.
 *
 * How to handle: Depends on the context. Consider aborting the user-initiated action.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/UserCancelledException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(UserCancelledError, BaseError);

/**
 * Data consistency check failed. This is not directly related to user input error but rather a
 * failed sanity check for result verification.
 *
 * How to handle: Depends on the context. Retrying might help.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/InconsistentDataException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(InconsistentDataError, BaseError);

/**
 * The FdV SDK found metadata or other information in an ePA record which it does not understand.
 *
 * How to handle: Inform the user to update their ePA FdV version.
 *
 * https://github.ibmgcloud.net/ePA/fdv-api-spec/blob/master/src/main/java/com/ibm/epa/client/error/UnsupportedRecordContentException.kt
 */
DEFINE_ERROR_CLASS_DERIVED_FROM(UnsupportedRecordContentError, BaseError);

#undef DEFINE_ERROR_CLASS_DERIVED_FROM
} // namespace epa

#endif
