/*
 * LinuxDisplay.h
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#ifndef LLGL_EMSCRIPTEN_DISPLAY_H
#define LLGL_EMSCRIPTEN_DISPLAY_H


#include <LLGL/Display.h>
#include <memory>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/val.h>
#include <emscripten/bind.h>

namespace LLGL
{


class EmscriptenDisplay : public Display
{

    public:

        EmscriptenDisplay(int screenIndex);

        bool IsPrimary() const override;

        UTF8String GetDeviceName() const override;

        Offset2D GetOffset() const override;
        float GetScale() const override;

        bool ResetDisplayMode() override;
        bool SetDisplayMode(const DisplayMode& displayMode) override;
        DisplayMode GetDisplayMode() const override;

        std::vector<DisplayMode> GetSupportedDisplayModes() const override;

    private:

    private:

        int                                     screen_             = 0;

};


} // /namespace LLGL


#endif



// ================================================================================
