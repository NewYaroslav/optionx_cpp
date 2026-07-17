#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_OPERATION_KEY_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_OPERATION_KEY_HPP_INCLUDED

/// \file MetaTraderFileOperationKey.hpp
/// \brief Defines compact operation key helpers for MetaTrader file clients.

namespace optionx::bridges::metatrader_file {

    /// \brief Generates a compact Base36 operation key for file-transport commands.
    /// \details The generated value is intended to be created once per logical
    /// operation and reused for JSON-RPC `id`, `context.idempotency_key`, or
    /// domain `unique_hash` on retries of that same operation. The timestamp
    /// prefix keeps keys roughly time-sortable, while the random tail and local
    /// counter avoid collisions inside one process.
    /// \param prefix Optional readable prefix. Use an empty string to omit it.
    /// \param random_tail_length Number of Base36 random characters to append.
    /// \return A compact operation key such as `mfb_lz7abc_4fz..._1`.
    inline std::string make_compact_operation_key(
            const std::string& prefix = "mfb",
            const std::size_t random_tail_length = 16) {
        static std::atomic<std::uint64_t> counter{0};

        const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto time_part = utils::Base36::encode_int(
            static_cast<long long>(now_ms));
        const auto random_part = utils::Base36::random_string(random_tail_length);
        const auto counter_part = utils::Base36::encode_int(
            static_cast<long long>(counter++));

        if (prefix.empty()) {
            return time_part + random_part + counter_part;
        }
        return prefix + "_" + time_part + "_" + random_part + "_" + counter_part;
    }

} // namespace optionx::bridges::metatrader_file

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_OPERATION_KEY_HPP_INCLUDED
