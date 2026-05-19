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

// renders [Dauer]
// of [Dauer] [Intervall / Grundrhythmus]: [optionale Einschränkung auf Wochentage] [Zeit- oder Tagesabschnittsangaben] [Dosisangabe]
class BoundsDurationRenderer : public Renderer
{
public:
    using Renderer::Renderer;
    std::string text() const override;

private:
    void doRender(const std::vector<model::DosageDgMP>& dosages) override;
    std::string mText;
};

}// dosagetext
