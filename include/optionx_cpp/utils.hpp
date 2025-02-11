#pragma once
#ifndef _OPTIONX_UTILS_HPP_INCLUDED
#define _OPTIONX_UTILS_HPP_INCLUDED

/// \file utils.hpp
/// \brief Master utilities header aggregating all utility modules.
/// \details This header provides access to cross-platform utilities for:
///          - Enum and string manipulation
///          - Filesystem operations
///          - Numeric conversions
///          - Data encoding/decoding
///          - Asynchronous task management
///          - Pub/sub messaging patterns
///          - Cryptographic functions

// Core utilities
#include "utils/enum_utils.hpp"     ///< Enum serialization/deserialization tools
#include "utils/string_utils.hpp"   ///< String encoding/formatting/parsing utilities
#include "utils/http_utils.hpp"     ///< HTTP request/response handling utilities.
#include "utils/path_utils.hpp"     ///< Filesystem path manipulation helpers
#include "utils/fixed_point.hpp"    ///< Fixed-point arithmetic for financial calculations
#include "utils/time_utils.hpp"     ///< Time-related utilities, including timestamp retrieval.

// Data encoding
#include "utils/Base36.hpp"         ///< Base36 encoding/decoding implementation
#include "utils/Base64.hpp"         ///< Base64 encoding/decoding implementation

// Concurrency patterns
#include "utils/tasks.hpp"          ///< Task queues and asynchronous job management
#include "utils/pubsub.hpp"         ///< Publish-subscribe messaging system

// Cryptographic utilities
#include "utils/crypto.hpp"			///< Cryptographic functions including encryption and secure key management.

#endif // _OPTIONX_UTILS_HPP_INCLUDED
