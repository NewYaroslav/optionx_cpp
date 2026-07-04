#pragma once
#ifndef _OPTIONX_CONFIG_HPP_INCLUDED
#define _OPTIONX_CONFIG_HPP_INCLUDED

/// \file config.hpp
/// \brief Defines project-wide compile-time defaults.

#ifndef OPTIONX_DEFAULT_BROWSER_USER_AGENT
#define OPTIONX_DEFAULT_BROWSER_USER_AGENT \
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
    "(KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36"
#endif

#ifndef OPTIONX_DEFAULT_ACCEPT_LANGUAGE
#define OPTIONX_DEFAULT_ACCEPT_LANGUAGE "ru,ru-RU;q=0.9,en;q=0.8,en-US;q=0.7"
#endif

#endif // _OPTIONX_CONFIG_HPP_INCLUDED
