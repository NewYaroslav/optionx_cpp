#pragma once
#ifndef _OPTIONX_ENUM_UTILS_HPP_INCLUDED
#define _OPTIONX_ENUM_UTILS_HPP_INCLUDED

/// \file enum_utils.hpp
/// \brief Contains utility functions for working with enums.

#include <string>
#include <map>
#include <stdexcept>

namespace optionx {

    template <typename EnumType>
    EnumType to_enum(const std::string& str);

} // namespace optionx

#endif // _OPTIONX_ENUM_UTILS_HPP_INCLUDED
