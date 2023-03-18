/*
 * VKExtensionRegistry.cpp
 *
 * This file is part of the "LLGL" project (Copyright (c) 2015-2019 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "VKExtensionRegistry.h"
#include "../Vulkan.h"
#include "../../../Core/Exception.h"
#include <LLGL/Container/Strings.h>
#include <LLGL/Platform/Platform.h>


namespace LLGL
{


static bool g_VKRegisteredExtensions[static_cast<std::size_t>(VKExt::Count)] = {};

static const char* g_VKOptionalExtensions[] =
{
    VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
    VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME,
    VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
    //VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME,
    nullptr,
};

void RegisterExtension(VKExt extension)
{
    g_VKRegisteredExtensions[static_cast<std::size_t>(extension)] = true;
}

bool HasExtension(const VKExt extension)
{
    return g_VKRegisteredExtensions[static_cast<std::size_t>(extension)];
}

void AssertExtension(const VKExt extension, const char* extensionName, const char* funcName)
{
    if (!HasExtension(extension))
        ThrowVKExtensionNotSupportedExcept(funcName, extensionName);
}

const char** GetOptionalExtensions()
{
    return g_VKOptionalExtensions;
}

static bool IsVulkanInstanceExtRequired(const StringView& name)
{
    return
    (
        name == VK_KHR_SURFACE_EXTENSION_NAME
        #ifdef LLGL_OS_WIN32
        || name == VK_KHR_WIN32_SURFACE_EXTENSION_NAME
        #endif
        #ifdef LLGL_OS_LINUX
        || name == VK_KHR_XLIB_SURFACE_EXTENSION_NAME
        #endif
    );
}

static bool IsVulkanInstanceExtOptional(const StringView& name)
{
    return
    (
        name == VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    );
}

static bool IsVulkanInstanceExtDebugOnly(const StringView& name)
{
    return
    (
        name == VK_EXT_DEBUG_REPORT_EXTENSION_NAME
    );
}

VKExtSupport GetVulkanInstanceExtensionSupport(const char* extensionName)
{
    const StringView name = extensionName;
    if (IsVulkanInstanceExtRequired(name))
        return VKExtSupport::Required;
    if (IsVulkanInstanceExtOptional(name))
        return VKExtSupport::Optional;
    if (IsVulkanInstanceExtDebugOnly(name))
        return VKExtSupport::DebugOnly;
    return VKExtSupport::Unsupported;
}


} // /namespace LLGL



// ================================================================================
