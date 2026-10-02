#pragma once
#include "Engine/Engine.h"

/** What the editor needs from whoever owns the engine. The plugin implements it over the
    processor and its APVTS; the snapshot tool over a headless engine -- which is why the UI never
    includes the processor. */
namespace mdt
{
struct Host
{
    virtual ~Host() = default;

    virtual const Patch& patch() const = 0;
    virtual void setPatch (const Patch&) = 0;
    /** Bumps whenever the patch changes behind the editor's back (state load). */
    virtual uint32_t patchVersion() const = 0;

    virtual float param (P) const = 0;
    virtual void setParam (P, float natural) = 0;
    virtual void beginGesture (P) {}
    virtual void endGesture (P) {}

    virtual const UiSnapshot& snapshot() = 0;

    virtual void noteOn (int note, float velocity) = 0;
    virtual void noteOff (int note) = 0;
    virtual void releaseHeld() = 0;
    virtual void kick() = 0;
};
} // namespace mdt
