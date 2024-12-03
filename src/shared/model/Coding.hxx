/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_SHARED_MODEL_CODING_HXX
#define ERP_PROCESSING_CONTEXT_SRC_SHARED_MODEL_CODING_HXX

#include <rapidjson/fwd.h>
#include <optional>
#include <string>

namespace model
{

class Coding
{
public:
    explicit Coding(const rapidjson::Value& object);
    Coding(const std::string_view& system, const std::string_view& code);

    void setSystem(const std::string& system);
    void setVersion(const std::string& version);
    void setCode(const std::string& code);
    void setDisplay(const std::string& display);
    void setUserSelected(bool userSelected);

    const std::optional<std::string>& getSystem() const;
    const std::optional<std::string>& getVersion() const;
    const std::optional<std::string>& getCode() const;
    const std::optional<std::string>& getDisplay() const;
    const std::optional<bool>& getUserSelected() const;

private:
    std::optional<std::string> mSystem;
    std::optional<std::string> mVersion;
    std::optional<std::string> mCode;
    std::optional<std::string> mDisplay;
    std::optional<bool> mUserSelected;
};

}// model

#endif//ERP_PROCESSING_CONTEXT_SRC_SHARED_MODEL_CODING_HXX
