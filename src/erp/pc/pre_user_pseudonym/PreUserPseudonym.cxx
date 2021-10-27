/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/pc/pre_user_pseudonym/PreUserPseudonym.hxx"

#include "erp/util/Expect.hxx"

#include <algorithm>

static bool notHexDigit (char c)
{
    static const auto& ctype = std::use_facet<std::ctype<char>>(std::locale::classic());
    return not ctype.is(std::ctype<char>::xdigit, c);
}


PreUserPseudonym::PreUserPseudonym(const CmacSignature& preUserPseudonym)
    : mPreUserPseudonym(preUserPseudonym)
{
}

PreUserPseudonym PreUserPseudonym::fromUserPseudonym(const std::string_view& userPseudonym)
{
    Expect(userPseudonym.size() == CmacSignature::SignatureSize * 4 + 1, "Invalid User-Pseudonym.");
    Expect(userPseudonym.at(CmacSignature::SignatureSize * 2) == '-', "Invalid User-Pseudonym Format.");
    auto noHex = std::find_if(userPseudonym.begin() + CmacSignature::SignatureSize * 2 + 1, userPseudonym.end(), &notHexDigit);
    Expect(noHex == userPseudonym.end(), "Invalid Character in User-Pseudonym CMAC");
    return PreUserPseudonym(CmacSignature::fromHex(std::string(userPseudonym.substr(0, CmacSignature::SignatureSize * 2))));
}

const CmacSignature& PreUserPseudonym::getSignature() const
{
    return mPreUserPseudonym;
}
