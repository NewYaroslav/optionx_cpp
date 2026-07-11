#pragma once
#ifndef OPTIONX_HEADER_DATA_BRIDGE_BRIDGE_SIGNAL_REPORT_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_BRIDGE_BRIDGE_SIGNAL_REPORT_HPP_INCLUDED

/// \file BridgeSignalReport.hpp
/// \brief Defines diagnostics emitted while bridge input is converted to signals.

namespace optionx {

    /// \enum BridgeSignalReportStatus
    /// \brief Broad classification for a bridge intake diagnostic.
    enum class BridgeSignalReportStatus {
        UNKNOWN = 0, ///< Unknown report status.
        REJECTED,    ///< Input was understood but rejected by bridge rules.
        IGNORED,     ///< Input was intentionally ignored as non-trading data.
        INVALID,     ///< Input was malformed or lacked required fields.
        DUPLICATE,   ///< Input duplicated a previously handled event.
        INTAKE_ERROR,///< Bridge accepted the input but could not publish it.
        SUSPICIOUS   ///< Input is useful for diagnostics but should not trade.
    };

    /// \brief Converts BridgeSignalReportStatus to a stable string.
    /// \param value Status value.
    /// \return String representation.
    inline const char* to_str(BridgeSignalReportStatus value) noexcept {
        switch (value) {
        case BridgeSignalReportStatus::REJECTED:
            return "REJECTED";
        case BridgeSignalReportStatus::IGNORED:
            return "IGNORED";
        case BridgeSignalReportStatus::INVALID:
            return "INVALID";
        case BridgeSignalReportStatus::DUPLICATE:
            return "DUPLICATE";
        case BridgeSignalReportStatus::INTAKE_ERROR:
            return "INTAKE_ERROR";
        case BridgeSignalReportStatus::SUSPICIOUS:
            return "SUSPICIOUS";
        case BridgeSignalReportStatus::UNKNOWN:
        default:
            return "UNKNOWN";
        }
    }

    /// \struct BridgeSignalReport
    /// \brief Diagnostic event produced before or instead of TradeSignal publishing.
    ///
    /// Reports are not trade commands. They are meant for logs, UI diagnostics,
    /// monitoring, and strategy debugging when bridge input is rejected,
    /// ignored, duplicated, malformed, or otherwise suspicious.
    struct BridgeSignalReport {
        BridgeId bridge_id = 0;                         ///< Source bridge ID.
        BridgeType bridge_type = BridgeType::UNKNOWN;   ///< Source bridge type.
        BridgeSignalReportStatus status =
            BridgeSignalReportStatus::UNKNOWN;          ///< Diagnostic category.

        std::string reason_code;    ///< Machine-readable reason, for example `unmapped_level_alert`.
        std::string message;        ///< Human-readable context.
        std::string connection_id;  ///< Optional client/connection identifier.
        std::string event_id;       ///< External event ID when available.
        std::string dedupe_key;     ///< Duplicate-detection key when available.
        std::string symbol;         ///< Parsed symbol when available.
        std::string signal_name;    ///< Parsed signal or alert name when available.

        std::shared_ptr<const TradeSignal> candidate_signal; ///< Candidate signal, if one was built.
        nlohmann::json raw_payload;       ///< Sanitized source payload when available.
        nlohmann::json parsed_payload;    ///< Normalized fields extracted from the source payload.
        nlohmann::json context;           ///< Bridge-specific details such as bar lag or parse errors.

        std::int64_t received_time_ms = 0; ///< Local receive timestamp in Unix milliseconds.
        std::int64_t source_time_ms = 0;   ///< Source event timestamp in Unix milliseconds, if known.
    };

    /// \brief Serializes a bridge signal report for logging or diagnostics.
    inline void to_json(nlohmann::json& j, const BridgeSignalReport& report) {
        j = nlohmann::json{
            {"bridge_id", report.bridge_id},
            {"bridge_type", report.bridge_type},
            {"status", to_str(report.status)},
            {"reason_code", report.reason_code},
            {"message", report.message},
            {"connection_id", report.connection_id},
            {"event_id", report.event_id},
            {"dedupe_key", report.dedupe_key},
            {"symbol", report.symbol},
            {"signal_name", report.signal_name},
            {"candidate_signal", report.candidate_signal ? nlohmann::json(*report.candidate_signal) : nlohmann::json(nullptr)},
            {"raw_payload", report.raw_payload},
            {"parsed_payload", report.parsed_payload},
            {"context", report.context},
            {"received_time_ms", report.received_time_ms},
            {"source_time_ms", report.source_time_ms}
        };
    }

} // namespace optionx

#endif // OPTIONX_HEADER_DATA_BRIDGE_BRIDGE_SIGNAL_REPORT_HPP_INCLUDED
