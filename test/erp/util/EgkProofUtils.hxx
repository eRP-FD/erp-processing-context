/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */
#pragma once

#include <optional>
#include <string>

class PcServiceContext;
class VsdmHmacKey;


std::string createEncodedPnw(std::optional<std::string> pnwPzNumberInput = std::nullopt);
void enrolKey(PcServiceContext& serviceContext, VsdmHmacKey& keyPackage);
