#pragma once
#ifndef OPTIONX_HEADER_UTILS_JSON_COMMENTS_HPP_INCLUDED
#define OPTIONX_HEADER_UTILS_JSON_COMMENTS_HPP_INCLUDED

/// \file json_comments.hpp
/// \brief Helpers for removing JSONC-style comments before JSON parsing.
/// \details Supports `//`, `/* ... */`, and shell-style `#` line comments
///          outside string literals. Hash comments are recognized only at the
///          beginning of a line or after whitespace.

#include <cctype>
#include <cstddef>
#include <nlohmann/json.hpp>
#include <string>

namespace optionx::utils {

    namespace detail {

        /// \brief Returns true when a quote is escaped by an odd number of backslashes.
        inline bool is_escaped_json_quote(
                const std::string& text,
                std::size_t position) noexcept {
            std::size_t backslash_count = 0;
            while (position > 0 && text[--position] == '\\') {
                ++backslash_count;
            }
            return (backslash_count % 2) == 1;
        }

        /// \brief Appends spaces for removed comment characters while keeping line breaks.
        inline void append_spaces_preserve_newlines(
                std::string& output,
                const std::string& source,
                std::size_t begin,
                std::size_t end) {
            output.reserve(output.size() + (end - begin));
            for (std::size_t index = begin; index < end; ++index) {
                const auto ch = source[index];
                output.push_back((ch == '\n' || ch == '\r') ? ch : ' ');
            }
        }

    } // namespace detail

    /// \brief Removes JSONC-style comments from a string.
    /// \param text Input JSON-like text.
    /// \param preserve_whitespace When true, removed comment bytes become spaces
    ///        while newlines are preserved. This keeps parse-error columns closer
    ///        to the original file.
    /// \return Text with comments removed.
    inline std::string strip_json_comments(
            const std::string& text,
            bool preserve_whitespace = false) {
        enum class CommentState : unsigned char {
            None,
            Line,
            Block
        };

        if (text.empty()) {
            return {};
        }

        std::string output;
        output.reserve(text.size());

        bool in_string = false;
        bool at_line_start = true;
        CommentState comment = CommentState::None;
        std::size_t chunk_begin = 0;
        std::size_t comment_begin = 0;

        for (std::size_t index = 0; index < text.size(); ++index) {
            const auto ch = text[index];
            const auto next = index + 1 < text.size() ? text[index + 1] : '\0';

            if (comment == CommentState::None && ch == '"') {
                if (!detail::is_escaped_json_quote(text, index)) {
                    in_string = !in_string;
                }
                if (ch == '\n' || ch == '\r') {
                    at_line_start = true;
                } else if (!std::isspace(static_cast<unsigned char>(ch))) {
                    at_line_start = false;
                }
                continue;
            }

            if (in_string) {
                if (ch == '\n' || ch == '\r') {
                    at_line_start = true;
                } else if (!std::isspace(static_cast<unsigned char>(ch))) {
                    at_line_start = false;
                }
                continue;
            }

            if (comment == CommentState::None) {
                if (ch == '/' && next == '/') {
                    output.append(text, chunk_begin, index - chunk_begin);
                    comment_begin = index;
                    comment = CommentState::Line;
                    ++index;
                    continue;
                }
                if (ch == '/' && next == '*') {
                    output.append(text, chunk_begin, index - chunk_begin);
                    comment_begin = index;
                    comment = CommentState::Block;
                    ++index;
                    continue;
                }
                if (ch == '#') {
                    const bool previous_is_space = index == 0 ||
                        std::isspace(static_cast<unsigned char>(text[index - 1])) != 0;
                    if (at_line_start || previous_is_space) {
                        output.append(text, chunk_begin, index - chunk_begin);
                        comment_begin = index;
                        comment = CommentState::Line;
                        continue;
                    }
                }

                if (ch == '\n' || ch == '\r') {
                    at_line_start = true;
                } else if (!std::isspace(static_cast<unsigned char>(ch))) {
                    at_line_start = false;
                }
                continue;
            }

            if (comment == CommentState::Line) {
                if (ch == '\n') {
                    if (preserve_whitespace) {
                        detail::append_spaces_preserve_newlines(output, text, comment_begin, index);
                    }
                    output.push_back('\n');
                    chunk_begin = index + 1;
                    comment = CommentState::None;
                    at_line_start = true;
                } else if (ch == '\r' && next == '\n') {
                    if (preserve_whitespace) {
                        detail::append_spaces_preserve_newlines(output, text, comment_begin, index);
                    }
                    output.push_back('\r');
                    output.push_back('\n');
                    chunk_begin = index + 2;
                    ++index;
                    comment = CommentState::None;
                    at_line_start = true;
                }
                continue;
            }

            if (comment == CommentState::Block) {
                if (ch == '*' && next == '/') {
                    const auto comment_end = index + 2;
                    if (preserve_whitespace) {
                        detail::append_spaces_preserve_newlines(output, text, comment_begin, comment_end);
                    }
                    chunk_begin = comment_end;
                    ++index;
                    comment = CommentState::None;
                }
            }
        }

        if (comment == CommentState::None) {
            output.append(text, chunk_begin, text.size() - chunk_begin);
        } else if (preserve_whitespace) {
            detail::append_spaces_preserve_newlines(output, text, comment_begin, text.size());
        }

        return output;
    }

    /// \brief Parses JSON after removing JSONC-style comments.
    inline nlohmann::json parse_json_with_comments(
            const std::string& text,
            bool preserve_whitespace = true) {
        return nlohmann::json::parse(strip_json_comments(text, preserve_whitespace));
    }

} // namespace optionx::utils

#endif // OPTIONX_HEADER_UTILS_JSON_COMMENTS_HPP_INCLUDED
