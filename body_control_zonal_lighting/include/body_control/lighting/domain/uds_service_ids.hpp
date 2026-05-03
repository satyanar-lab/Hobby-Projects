#pragma once

#include <cstdint>

// UDS service IDs (ISO 14229-1), DID assignments, NRC codes, and logical
// addresses for the body control lighting DoIP diagnostic channel.
//
// These constants are shared between UdsRequestHandler (C++ server side) and
// the Python client (tools/uds_client/bcl_diagnostic_client.py) — any change
// here must be reflected in the Python table.

namespace body_control
{
namespace lighting
{
namespace domain
{
namespace uds
{

// ── Service IDs ───────────────────────────────────────────────────────────────

constexpr std::uint8_t kSidDiagnosticSessionControl    {0x10U};
constexpr std::uint8_t kSidClearDiagnosticInformation  {0x14U};
constexpr std::uint8_t kSidReadDtcInformation          {0x19U};
constexpr std::uint8_t kSidReadDataByIdentifier        {0x22U};
constexpr std::uint8_t kSidRoutineControl              {0x31U};
constexpr std::uint8_t kSidRequestDownload             {0x34U};
constexpr std::uint8_t kSidTransferData                {0x36U};
constexpr std::uint8_t kSidRequestTransferExit         {0x37U};
constexpr std::uint8_t kSidNegativeResponse            {0x7FU};

// Positive response is RequestSID + 0x40.
constexpr std::uint8_t kPositiveResponseOffset         {0x40U};

// ── DiagnosticSessionControl sub-functions ────────────────────────────────────

constexpr std::uint8_t kSessionDefault                 {0x01U};
constexpr std::uint8_t kSessionExtended               {0x03U};

// ── ReadDataByIdentifier — Data Identifiers (DIDs) ────────────────────────────

// 0xF190 — ECU identification string (ASCII, null-terminated).
constexpr std::uint16_t kDidEcuIdentification          {0xF190U};
// 0xF101 — Lamp output states: 5 bytes, one per LampFunction (kOff=1, kOn=2).
constexpr std::uint16_t kDidLampStatus                 {0xF101U};
// 0xF102 — Node health: 3 bytes (state, flags, fault_count).
constexpr std::uint16_t kDidNodeHealth                 {0xF102U};
// 0xF103 — Active DTCs: 1-byte count followed by N × 2-byte FaultCode values.
constexpr std::uint16_t kDidActiveFaults               {0xF103U};

// ── ReadDTCInformation sub-functions ─────────────────────────────────────────

constexpr std::uint8_t kDtcSubReportByStatusMask       {0x02U};
constexpr std::uint8_t kDtcSubReportSupported          {0x0AU};

// Status availability mask for our DTC records.
// Bit 0 = testFailed, bit 3 = confirmedDTC.
constexpr std::uint8_t kDtcStatusAvailabilityMask      {0x09U};
// Status byte applied to every active fault: testFailed + confirmedDTC.
constexpr std::uint8_t kDtcStatusActive                {0x09U};

// ── ClearDiagnosticInformation group ID ──────────────────────────────────────

constexpr std::uint32_t kDtcGroupAll                   {0x00FF'FFFFu};

// ── RoutineControl sub-functions ─────────────────────────────────────────────

constexpr std::uint8_t kRoutineStart                   {0x01U};
constexpr std::uint8_t kRoutineStop                    {0x02U};

// Routine identifiers map 1:1 to FaultCode values so no separate table is
// needed.  0xB001–0xB005 = left indicator through head lamp.
constexpr std::uint16_t kRoutineIdFaultBase            {0xB001U};
constexpr std::uint16_t kRoutineIdFaultMax             {0xB005U};

// ── Negative Response Codes (NRCs) ───────────────────────────────────────────

constexpr std::uint8_t kNrcServiceNotSupported              {0x11U};
constexpr std::uint8_t kNrcSubFunctionNotSupported          {0x12U};
constexpr std::uint8_t kNrcConditionsNotCorrect             {0x22U};
constexpr std::uint8_t kNrcRequestOutOfRange                {0x31U};
constexpr std::uint8_t kNrcUploadDownloadNotAccepted        {0x70U};
constexpr std::uint8_t kNrcTransferDataSuspended            {0x71U};
constexpr std::uint8_t kNrcGeneralProgrammingFailure        {0x72U};
constexpr std::uint8_t kNrcWrongBlockSequenceCounter        {0x73U};

// ── Node health state bytes (F102 DID, byte 0) ────────────────────────────────
// These mirror the NodeHealthState SOME/IP enum values used in the operational
// path; adding kUpdating for the planned OTA reprogramming state.

constexpr std::uint8_t kHealthUnknown                  {0x00U};
constexpr std::uint8_t kHealthOperational              {0x01U};
constexpr std::uint8_t kHealthDegraded                 {0x02U};
constexpr std::uint8_t kHealthFaulted                  {0x03U};
constexpr std::uint8_t kHealthUpdating                 {0x04U};

// ── DoIP logical addresses ────────────────────────────────────────────────────

constexpr std::uint16_t kDoipTesterAddress             {0x0E00U};
constexpr std::uint16_t kDoipRearNodeAddress           {0x0E01U};

// TCP port for all DoIP diagnostic traffic (ISO 13400-2).
constexpr std::uint16_t kDoipPort                      {13400U};

}  // namespace uds
}  // namespace domain
}  // namespace lighting
}  // namespace body_control
