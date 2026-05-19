/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */
#pragma once

#include "shared/util/dosagetext/renderer/Renderer.hxx"

namespace dosagetext
{

class FreeTextRenderer : public Renderer
{
public:
    using Renderer::Renderer;
    std::string text() const override;
    std::string separator() const override;

private:
    void doRender(const std::vector<model::DosageDgMP>& dosages) override;
    std::string mText;
};

}// dosageetext
