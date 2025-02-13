/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/TelematikLookup.hxx"
#include "shared/util/Expect.hxx"

#include <boost/algorithm/string.hpp>
#include <fstream>
#include <vector>


TelematikLookup::TelematikLookup(std::istream& is)
{
    for (std::string line; std::getline(is, line);)
    {
        std::vector<std::string> strs;
        boost::split(strs, line, boost::is_any_of(delimiter));
        Expect(strs.size() == 2, "Bad entry in TelematikLookup mapping File");
        boost::trim(strs[0]);
        boost::trim(strs[1]);
        model::TelematikId telematikId(strs[1]);
        Expect(telematikId.validFormat(), "Bad entry in TelematikLookup mapping File");
        mSer2Tid.emplace(std::move(strs[0]), std::move(telematikId));
    }
}

TelematikLookup TelematikLookup::fromFile(const std::filesystem::path& path)
{
    std::ifstream ifs(path);
    Expect(ifs, "Failed to load lookup mapping from file ");
    TelematikLookup telematikLookup(ifs);
    return telematikLookup;
}

bool TelematikLookup::hasSerialNumber(const std::string& sn) const
{
    return mSer2Tid.contains(sn);
}

const model::TelematikId& TelematikLookup::serialNumber2TelematikId(const std::string& sn) const
{
    const auto it = mSer2Tid.find(sn);
    Expect(it != mSer2Tid.cend(), "Unknown serialNumber for TelematikLookup requested");
    return it->second;
}

std::size_t TelematikLookup::size() const
{
    return mSer2Tid.size();
}
