/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_TELEMATIKLOOKUP_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_TELEMATIKLOOKUP_HXX

#include "shared/model/TelematikId.hxx"

#include <filesystem>
#include <map>
#include <string>

class TelematikLookup
{
public:
    static constexpr std::string_view delimiter = ";";

    TelematikLookup(std::istream& is);
    static TelematikLookup fromFile(const std::filesystem::path& path);

    bool hasSerialNumber(const std::string& sn) const;
    const model::TelematikId& serialNumber2TelematikId(const std::string& sn) const;
    std::size_t size() const;

private:
    std::map<std::string, model::TelematikId> mSer2Tid;
};

#endif
