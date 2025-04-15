// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "shared/ErpRequirements.hxx"
#include "erp/util/RuntimeConfiguration.hxx"
#include "shared/util/Expect.hxx"

namespace erp
{
void RuntimeConfiguration::enableAcceptPN3(const model::Timestamp& expiry)
{
    ErpExpect(expiry <= model::Timestamp::now() + accept_pn3_max_active, HttpStatus::BadRequest,
              (std::ostringstream{} << "invalid PN3 exiry date " << expiry
                                    << ". maximum AcceptPN3 activity: " << accept_pn3_max_active)
                  .str());
    mAcceptPN3.emplace<AcceptPN3Enabled>(expiry);
}

void RuntimeConfiguration::disableAcceptPN3()
{
    mAcceptPN3.emplace<AcceptPN3Disabled>();
}

bool RuntimeConfiguration::isAcceptPN3Enabled() const
{
    return std::holds_alternative<AcceptPN3Enabled>(mAcceptPN3);
}

model::Timestamp RuntimeConfiguration::getAcceptPN3Expiry() const
{
    ModelExpect(isAcceptPN3Enabled(), "AcceptPN3 is false");
    return std::get<AcceptPN3Enabled>(mAcceptPN3).acceptPN3Expiry;
}

RuntimeConfiguration::AcceptPN3Enabled::AcceptPN3Enabled(const model::Timestamp& expiry)
    : acceptPN3Expiry(expiry)
{
}


RuntimeConfiguration::Getter::Getter(std::shared_ptr<const RuntimeConfiguration> runtimeConfiguration)
    : mRuntimeConfiguration(std::move(runtimeConfiguration))
    , mSharedLock(mRuntimeConfiguration->mSharedMutex)
{
}

bool RuntimeConfiguration::Getter::isAcceptPN3Enabled() const
{
    A_25200.start("Status accept PN3");
    return mRuntimeConfiguration->isAcceptPN3Enabled();
    A_25200.finish();
}

model::Timestamp RuntimeConfiguration::Getter::getAcceptPN3Expiry() const
{
    return mRuntimeConfiguration->getAcceptPN3Expiry();
}


RuntimeConfiguration::Setter::Setter(std::shared_ptr<RuntimeConfiguration> runtimeConfiguration)
    : mRuntimeConfiguration(std::move(runtimeConfiguration))
    , mUniqueLock(mRuntimeConfiguration->mSharedMutex)
{
}

void RuntimeConfiguration::Setter::enableAcceptPN3(const model::Timestamp& expiry)
{
    mRuntimeConfiguration->enableAcceptPN3(expiry);
}

void RuntimeConfiguration::Setter::disableAcceptPN3()
{
    mRuntimeConfiguration->disableAcceptPN3();
}

bool RuntimeConfiguration::Setter::isAcceptPN3Enabled() const
{
    return mRuntimeConfiguration->isAcceptPN3Enabled();
}

model::Timestamp RuntimeConfiguration::Setter::getAcceptPN3Expiry() const
{
    return mRuntimeConfiguration->getAcceptPN3Expiry();
}
}
