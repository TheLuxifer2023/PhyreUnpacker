#pragma once

// Override PhyreEngine platform implementation to use D3D11 instead of Generic
// This must be included before any PhyreEngine headers

// Force D3D11 platform to be defined
#define PHYRE_RENDERING_PLATFORM_D3D11

// Undefine Generic platform to prevent it from taking priority
#ifdef PHYRE_RENDERING_PLATFORM_GENERIC
#undef PHYRE_RENDERING_PLATFORM_GENERIC
#endif

// Undefine the default Generic implementation
#ifdef PHYRE_RENDERING_PLATFORM_IMPLEMENTATION
#undef PHYRE_RENDERING_PLATFORM_IMPLEMENTATION
#endif

// Force D3D11 implementation
#define PHYRE_RENDERING_PLATFORM_IMPLEMENTATION(TYPE) TYPE##D3D11

// Also ensure D3D11 platform is defined
#ifndef PHYRE_RENDERING_PLATFORM_D3D11
#define PHYRE_RENDERING_PLATFORM_D3D11
#endif

// Force D3D11 platform loader to be used
#define PHYRE_RENDERING_PLATFORM_LOADER_D3D11

// Force D3D11 platform converter to be used
#define PHYRE_RENDERING_PLATFORM_CONVERTER_D3D11
