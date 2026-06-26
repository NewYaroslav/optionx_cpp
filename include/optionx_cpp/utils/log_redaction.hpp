#pragma once
#ifndef _OPTIONX_UTILS_LOG_REDACTION_HPP_INCLUDED
#define _OPTIONX_UTILS_LOG_REDACTION_HPP_INCLUDED

/// \file log_redaction.hpp
/// \brief Helpers for removing sensitive values from diagnostic logs.

#include <string>

namespace optionx::utils {

    /// \brief Replaces a sensitive value with a fixed redaction marker.
    /// \param value Secret value that must not be written to logs.
    /// \return Empty string for empty values, otherwise "***".
    inline std::string redact_secret(const std::string& value) {
        return value.empty() ? std::string() : std::string("***");
    }

} // namespace optionx::utils

#endif // _OPTIONX_UTILS_LOG_REDACTION_HPP_INCLUDED
