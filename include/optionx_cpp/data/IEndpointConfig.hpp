#pragma once
#ifndef _OPTIONX_IENDPOINT_CONFIG_HPP_INCLUDED
#define _OPTIONX_IENDPOINT_CONFIG_HPP_INCLUDED

/// \file IEndpointConfig.hpp
/// \brief Defines the common base interface for endpoint configuration DTOs.

namespace optionx {

    /// \class IEndpointConfig
    /// \brief Common polymorphic base for configuration DTOs accepted by service endpoints.
    class IEndpointConfig {
    public:
        IEndpointConfig() = default;
        IEndpointConfig(const IEndpointConfig&) = default;
        IEndpointConfig& operator=(const IEndpointConfig&) = default;
        virtual ~IEndpointConfig() = default;
    };

} // namespace optionx

#endif // _OPTIONX_IENDPOINT_CONFIG_HPP_INCLUDED
