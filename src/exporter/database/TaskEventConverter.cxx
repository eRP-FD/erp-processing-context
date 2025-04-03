/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/database/TaskEventConverter.hxx"
#include "exporter/model/HashedKvnr.hxx"
#include "erp/database/ErpDatabaseModel.hxx"
#include "erp/model/Binary.hxx"
#include "shared/crypto/Jwt.hxx"
#include "shared/util/JsonLog.hxx"


TaskEventConverter::TaskEventConverter(const DataBaseCodec& codec, const TelematikLookup& telematikLookup)
    : mCodec(codec)
    , mTelematikLookup(telematikLookup)
{
}

model::BareTaskEvent TaskEventConverter::convertBareEvent(const db_model::EncryptedBlob& kvnr,
                                                          const db_model::HashedKvnr& hashedKvnr,
                                                          const std::string& usecase,
                                                          model::PrescriptionId prescriptionId,
                                                          std::int16_t prescriptionType, const SafeString& key) const
{
    const auto decodedKvnr = model::Kvnr(std::string_view{mCodec.decode(kvnr, key)});
    const auto typedUsecase = magic_enum::enum_cast<model::TaskEvent::UseCase>(usecase);
    Expect(typedUsecase.has_value(), "unknown usecase value");

    const auto typedPrescriptionType =
        magic_enum::enum_cast<model::PrescriptionType>(gsl::narrow<std::uint8_t>(prescriptionType));
    Expect(typedPrescriptionType.has_value(), "unknown prescription type value");

    return model::BareTaskEvent{decodedKvnr, model::HashedKvnr(hashedKvnr), *typedUsecase, prescriptionId,
                                *typedPrescriptionType};
}

std::unique_ptr<model::TaskEvent> TaskEventConverter::convert(const db_model::TaskEvent& dbTaskEvent,
                                                              const SafeString& key,
                                                              const SafeString& medicationDispenseKey) const
{
    std::unique_ptr<model::TaskEvent> result;

    Expect(dbTaskEvent.healthcareProviderPrescription,
           "missing healthproviderprescription in ProvidePrescription taskevent");


    const auto kvnr = model::Kvnr(std::string_view{mCodec.decode(dbTaskEvent.kvnr, key)});
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::string hashedKvnrString(reinterpret_cast<const char*>(dbTaskEvent.hashedKvnr.data()), dbTaskEvent.hashedKvnr.size());
    model::HashedKvnr hashedKvnr(dbTaskEvent.hashedKvnr);

    const auto usecase = magic_enum::enum_cast<model::TaskEvent::UseCase>(dbTaskEvent.usecase);
    Expect(usecase.has_value(), "unknown usecase value");

    const auto state = magic_enum::enum_cast<model::TaskEvent::State>(dbTaskEvent.state);
    Expect(state.has_value(), "unknown state value");

    const auto prescriptionType =
        magic_enum::enum_cast<model::PrescriptionType>(gsl::narrow<std::uint8_t>(dbTaskEvent.prescriptionType));
    Expect(prescriptionType.has_value(), "unknown prescription type value");

    const auto decryptedProviderPrescription = mCodec.decode(*dbTaskEvent.healthcareProviderPrescription, key);
    auto prescriptionBundle = model::Binary::fromJsonNoValidation(decryptedProviderPrescription);
    const auto viewData = prescriptionBundle.data();
    Expect(viewData.has_value(), "bad healthcareProviderPrescription in task event.");

    const auto cadesBesSignature = SignedPrescription::fromBinNoVerify(std::string(*viewData));
    auto qesTelematikId = cadesBesSignature.getTelematikId();
    if (! qesTelematikId.has_value())
    {
        try
        {
            const auto sn = cadesBesSignature.getSignerSerialNumber();

            if (! sn.empty())
            {
                if (mTelematikLookup.hasSerialNumber(sn))
                {
                    qesTelematikId = std::make_optional(mTelematikLookup.serialNumber2TelematikId(sn));
                }
                else
                {
                    JsonLog(LogId::INFO, JsonLog::makeWarningLogReceiver(), false)
                        << hashedKvnr
                        << KeyValue("prescription_id", dbTaskEvent.prescriptionId.toString())
                        << KeyValue("event",
                                    "For the given subject serial number of prescription signature certificate, no "
                                    "mapping is defined in SerNo2TID.")
                        << KeyValue("issuer", cadesBesSignature.getIssuer());
                }
            }
            else
            {
                JsonLog(LogId::INFO, JsonLog::makeWarningLogReceiver(), false)
                    << hashedKvnr
                    << KeyValue("prescription_id", dbTaskEvent.prescriptionId.toString())
                    << KeyValue("event", "No subject serial number present in the prescription signature certificate.")
                    << KeyValue("issuer", cadesBesSignature.getIssuer());
            }
        }
        catch (std::exception& e)
        {
            JsonLog(LogId::INFO, JsonLog::makeWarningLogReceiver(), false)
                << hashedKvnr << KeyValue("prescription_id", dbTaskEvent.prescriptionId.toString())
                << KeyValue("event", "Error encountered during extraction or mapping of SerialNumber.")
                << KeyValue("issuer", cadesBesSignature.getIssuer());
        }
    }
    const auto& prescription = cadesBesSignature.payload();
    model::Bundle bundle = model::Bundle::fromXmlNoValidation(prescription);

    switch (*usecase)
    {
        case model::TaskEvent::UseCase::providePrescription:
            result = convertProvidePrescriptionTaskEvent(dbTaskEvent, key, *prescriptionType, kvnr, hashedKvnrString,
                                                         *usecase, *state, qesTelematikId, std::move(bundle));
            break;
        case model::TaskEvent::UseCase::provideDispensation:
            result =
                convertProvideDispensationTaskEvent(dbTaskEvent, key, medicationDispenseKey, *prescriptionType, kvnr,
                                                    hashedKvnrString, *usecase, *state, qesTelematikId, std::move(bundle));
            break;
        case model::TaskEvent::UseCase::cancelPrescription:
            result = convertCancelPrescriptionTaskEvent(dbTaskEvent, *prescriptionType, kvnr, hashedKvnrString, *usecase,
                                                        *state, std::move(bundle));
            break;
        case model::TaskEvent::UseCase::cancelDispensation:
            result = convertCancelDispensationTaskEvent(dbTaskEvent, *prescriptionType, kvnr, hashedKvnrString, *usecase,
                                                        *state, std::move(bundle));
            break;
    }
    if (! result)
    {
        Fail("unknown usecase value");
    }
    return result;
}

std::unique_ptr<model::TaskEvent> TaskEventConverter::convertProvidePrescriptionTaskEvent(
    const db_model::TaskEvent& dbTaskEvent, const SafeString& key, model::PrescriptionType prescriptionType,
    const model::Kvnr& kvnr, const std::string& hashedKvnr, model::TaskEvent::UseCase usecase,
    model::TaskEvent::State state, const std::optional<model::TelematikId>& qesDoctorId,
    model::Bundle&& kbvBundle) const
{
    Expect(dbTaskEvent.doctorIdentity, "no doctor identity in task event.");
    const auto decryptedDoctorIdentity = mCodec.decode(*dbTaskEvent.doctorIdentity, key);
    const auto doctorIdentityData = db_model::AccessTokenIdentity::fromJson(std::string(decryptedDoctorIdentity));

    return std::make_unique<model::ProvidePrescriptionTaskEvent>(
        dbTaskEvent.id, dbTaskEvent.prescriptionId, prescriptionType, kvnr, hashedKvnr, usecase, state, qesDoctorId,
        doctorIdentityData.getId(), doctorIdentityData.getName(), doctorIdentityData.getOid(), std::move(kbvBundle),
        dbTaskEvent.lastModified);
}

std::unique_ptr<model::TaskEvent> TaskEventConverter::convertCancelPrescriptionTaskEvent(
    const db_model::TaskEvent& dbTaskEvent, model::PrescriptionType prescriptionType, const model::Kvnr& kvnr,
    const std::string& hashedKvnr, model::TaskEvent::UseCase usecase, model::TaskEvent::State state,
    model::Bundle&& kbvBundle) const
{
    return std::make_unique<model::CancelPrescriptionTaskEvent>(dbTaskEvent.id, dbTaskEvent.prescriptionId,
                                                                prescriptionType, kvnr, hashedKvnr, usecase, state,
                                                                std::move(kbvBundle), dbTaskEvent.lastModified);
}


std::unique_ptr<model::TaskEvent> TaskEventConverter::convertProvideDispensationTaskEvent(
    const db_model::TaskEvent& dbTaskEvent, const SafeString& key, const SafeString& medicationDispenseKey,
    model::PrescriptionType prescriptionType, const model::Kvnr& kvnr, const std::string& hashedKvnr,
    model::TaskEvent::UseCase usecase, model::TaskEvent::State state,
    const std::optional<model::TelematikId>& qesDoctorId, model::Bundle&& kbvBundle) const
{
    Expect(dbTaskEvent.medicationDispenseBundle && dbTaskEvent.medicationDispenseBundleBlobId,
           "no medication dispense in task event");
    Expect(medicationDispenseKey.size() > 0, "medication dispense key is missing in task event");
    const auto decryptedMedicationDispense =
        mCodec.decode(*dbTaskEvent.medicationDispenseBundle, medicationDispenseKey);
    auto medicationDispenseBundle = model::Bundle::fromJsonNoValidation(decryptedMedicationDispense);

    Expect(dbTaskEvent.doctorIdentity, "no doctor identity in task event.");
    const auto decryptedDoctorIdentity = mCodec.decode(*dbTaskEvent.doctorIdentity, key);
    const auto doctorIdentityData = db_model::AccessTokenIdentity::fromJson(std::string(decryptedDoctorIdentity));

    Expect(dbTaskEvent.pharmacyIdentity, "no pharmacy identity in task event.");
    const auto decryptedPharmacyIdentity = mCodec.decode(*dbTaskEvent.pharmacyIdentity, key);
    const auto pharmacyIdentityData = db_model::AccessTokenIdentity::fromJson(std::string(decryptedPharmacyIdentity));

    return std::make_unique<model::ProvideDispensationTaskEvent>(
        dbTaskEvent.id, dbTaskEvent.prescriptionId, prescriptionType, kvnr, hashedKvnr, usecase, state, qesDoctorId,
        doctorIdentityData.getId(), doctorIdentityData.getName(), doctorIdentityData.getOid(),
        pharmacyIdentityData.getId(), pharmacyIdentityData.getName(), pharmacyIdentityData.getOid(),
        std::move(kbvBundle), std::move(medicationDispenseBundle), dbTaskEvent.lastModified);
}

std::unique_ptr<model::TaskEvent> TaskEventConverter::convertCancelDispensationTaskEvent(
    const db_model::TaskEvent& dbTaskEvent, model::PrescriptionType prescriptionType, const model::Kvnr& kvnr,
    const std::string& hashedKvnr, model::TaskEvent::UseCase usecase, model::TaskEvent::State state,
    model::Bundle&& kbvBundle) const
{
    return std::make_unique<model::CancelDispensationTaskEvent>(dbTaskEvent.id, dbTaskEvent.prescriptionId,
                                                                prescriptionType, kvnr, hashedKvnr, usecase, state,
                                                                std::move(kbvBundle), dbTaskEvent.lastModified);
}
