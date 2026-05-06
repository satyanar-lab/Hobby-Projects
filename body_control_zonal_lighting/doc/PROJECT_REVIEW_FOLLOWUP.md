# Body Control Zonal Lighting — Project Review (Second Pass)

*Second-pass self-review of the project state after the P1 / P2 / P3 follow-up
commits. The first review (`doc/PROJECT_REVIEW.md`) was written against commit
d812918 with all P1 items still open. Twelve commits later (45575a2 at the head
of `main`), I am re-reading the code with the question: did I actually fix the
things I said I fixed, and what did the first review miss?*

*This pass is intentionally more critical than the first. I read the diff of
every fix commit, then re-read the surrounding code to find what the
patch did not touch. Every concrete claim cites a file and line number.*

---

## 1  Executive Summary

The first review tabled five P1 items, four P2 items, four P3 items, and three
P4 items. Of those, every P1 item has been addressed by a follow-up commit, two
of the P2 items have been addressed (the SOME/IP-SD entries-length guard at
`src/transport/some_ip_sd/some_ip_sd_codec.cpp:243` and the OTA CRC mandatory
check at `src/application/ota_session_manager.cpp:186`), and one P3 item has
been addressed (ASan/UBSan CI job in `.github/workflows/build.yml:94`). The
remaining items are still open.

That is the cheerful version. The harder version is this. Several of the
follow-up commits are correctly scoped patches that close the named gap and
nothing else. They do not address the second-order issues that sit one layer
underneath the named gap. The HealthThread fix
(`app/zephyr_nucleo_h753zi/main.cpp:770`) populates fault state but the same
function still hard-codes `ethernet_link_available = true` and
`service_available = true` two lines below at lines 789–790. The OTA mandatory
CRC fix correctly rejects bare 5-byte 0x37 with NRC 0x13, but the
`HandleRequestTransferExit` length-NRC path at
`src/application/ota_session_manager.cpp:186–190` does not call
`Reset()` and does not transition `state_` out of `kActive`, so a tester that
keeps spamming malformed 0x37 requests stays in OTA mode indefinitely. The
SOME/IP-SD `DecodeOffer` alignment guard rejects unaligned `entries_len`, but
the same function at line 268 still returns `true` with `remote_ip[0] = '\0'`
and `remote_port = 0` when the options section is missing — a discovery
without an endpoint, indistinguishable from a parse failure to most callers.

These are not regressions caused by the fixes. They are layers the original
review did not see, surfaced by re-reading code in the neighborhood of the
named patches.

**Verdict on portfolio readiness, second pass.** The project is in noticeably
better shape than at d812918. The P1 list was real; closing it raises the
project's credibility floor. The P2/P3 work that landed (ASan job, parser
unit tests, mandatory CRC, SD alignment) demonstrates that the review process
itself is taken seriously, which matters for the kind of senior-engineer
audience the portfolio targets. The remaining concerns are still real but are
predominantly P2/P3 in severity, not P1.

The most useful framing for the next iteration is this. The first review found
ten findings by reading 65 files at moderate depth. This pass found ten more by
reading twenty files at higher depth. There is at least one more pass of
findings still latent in the code. The diminishing-returns curve has not
flattened.

**Top three new concerns surfaced by this pass.**

1. *The OTA length-NRC path strands the session.* On `req.size() != 5U` at
   `src/application/ota_session_manager.cpp:186`, the handler returns NRC 0x13
   without resetting `state_`, the staging file descriptor, or the
   `received_size_` accumulator. The next request gets handled as if the
   session is still active. This is a denial-of-service path more than a
   correctness bug, but it has the shape of a state-machine rule violation.

2. *DoIP per-request 64 KB allocations.* `src/transport/doip_server.cpp:151`
   allocates `std::vector<std::uint8_t>(payload_len)` on every request with
   `kMaxPayloadLength = 65536` as the upper bound. A tester sending a stream of
   max-size headers but slow body bytes can keep a single 64 KB heap allocation
   live for the full TCP receive window. The Zephyr equivalent at
   `app/zephyr_nucleo_h753zi/main.cpp:901` correctly uses a static buffer with
   `kDoipMaxPayloadLen = 1024`, so the heap-pressure profile differs by a
   factor of 64x between platforms.

3. *Routing activation accepts any activation type.* At
   `src/transport/doip_server.cpp:191` the routing activation handler validates
   `length < 7U` but never checks `payload[2]`, the activation-type byte that
   ISO 13400-2 defines (default = 0x00, WWH-OBD = 0x01, central security =
   0xE0). Any value, including reserved values, is accepted. This is a spec
   conformance gap, not a vulnerability.

---

## 2  Audit of P1 Fix Commits

This section is a per-commit audit of the five P1 follow-up commits. For each
commit I describe what was changed, then ask whether the change closes the
named gap, whether it introduces new gaps, and what surrounding code was not
touched.

### 2.1  f9218c8 — Populate HealthThread fault state from FaultManager

The commit adds a `g_lamp_mgr.GetFaultStatus()` call under
`g_lamp_mgr_mutex` at `app/zephyr_nucleo_h753zi/main.cpp:770–772`, copies
`fault_status.fault_present` into `health.lamp_driver_fault_present` at line
775 and `fault_status.active_fault_count` into `health.active_fault_count` at
line 776, and derives `health.health_state` from the active fault count at
lines 782–785. The Status block at the top of `doc/PROJECT_REVIEW.md:14–38`
captures three rounds of NodeHealthEvent capture proving the fault state now
flows through the SOME/IP NodeHealthEvent.

**Does it close the named gap?** Yes. The hard-coded `false` and `0U`
assignments are gone. The hardware verification with `payload bytes: 03 01 01
01 00 01` after fault inject is concrete proof the fix landed.

**What did it not address?** Two adjacent fields are still hard-coded at
`app/zephyr_nucleo_h753zi/main.cpp:789–790`:

```
health.ethernet_link_available = true;
health.service_available       = true;
```

The inline comment at lines 787–788 acknowledges this is unfinished. The fix
correctly scopes itself to the named gap, but the `eth_link` and `svc_avail`
fields are still permanently advertised as `up` regardless of whether the PHY
is actually running or whether the SOME/IP service is reachable. A tester
reading the NodeHealth DID at `src/application/uds_request_handler.cpp:354`
sees `flags = 0x03U | fault_bit` — which, decoded, says "ethernet up AND
service up" forever. The first review's Probe 5 in section 12 of
`doc/PROJECT_REVIEW.md:961` predicted this exactly; the fix did not extend to
the predicted scope.

**Severity.** P2 follow-up. The diagnostic view of node health is partial. A
fleet-management tester querying NodeHealth would not be able to distinguish a
node with a dead Ethernet PHY from a node with a live PHY but no SOME/IP
neighbours. For a portfolio demo that runs everything on one cable, this is
invisible. For an interview probe asking "how does your tester detect a stuck
service?" the answer right now is "it does not."

### 2.2  0a9ff73 — Correct GPIO pin comment header

The commit edits the header comment in
`app/zephyr_nucleo_h753zi/boards/nucleo_h753zi.overlay` to match the actual
DTS node assignments. No DTS values changed; this was a comment-only fix.

**Does it close the named gap?** Yes, with the caveat that the gap was a
documentation accuracy issue rather than a functional issue. The DTS was
already correct; the comment header was lying.

**What did it not address?** Nothing in scope. The fix was tightly bounded by
its premise.

**Severity.** Closed.

### 2.3  b2e7b6a — Correct stale OTA references in FMEA and traceability map

The commit fixes two specific stale references in `safety/05_fmea.md` (FM-007
citing `ota_handler.cpp` instead of `ota_session_manager.cpp`) and
`safety/06_safety_mechanism_implementation_map.md` (citing
`OtaHandler::ProcessBlock()` which does not exist). The commit message
explicitly says "Sweep covered: ota_handler, rear_lighting, RearLighting,
BCL-REAR-NODE. All four patterns now return empty across safety/." That is a
broader sweep than the original P1 ticket asked for.

**Does it close the named gap?** Yes. Beyond that, the broader pattern sweep
was done and the safety/ tree no longer contains references to the
pre-rename naming.

**What did it not address?** Nothing detected. The audit was correct in scope
and slightly over-delivered.

**Severity.** Closed.

### 2.4  c6c31cd — Align vss_integration_design.md with current implementation

The commit corrects 17 namespace occurrences (`Vehicle.Body.Lights.BCL.*` →
`Vehicle.Private.BCL.Lighting.*`), 4 vspec filename occurrences, 5
struct-name occurrences (`VssLightSignals` → `VssSnapshot`), 11 field names
in section 3d, the `Translate(...)` → `Snapshot(...)` method signature, the
section 4b dataflow diagram, and the test-table contents. The commit message
explicitly notes that `doc/PROJECT_REVIEW.md` and `doc/vss_integration.md:72–82`
were intentionally left unchanged because the former describes the problem and
the latter contains a deliberate contrast pointing out why
`Vehicle.Private.*` was chosen.

**Does it close the named gap?** Yes. The first review only flagged the
namespace and struct name; the broader sweep caught method signature and field
name drift that the first review missed.

**What did it not address?** I should have flagged the method-signature drift
in the first review and did not. That is a self-criticism, not a criticism of
the patch. The patch over-delivered relative to the named scope.

**Severity.** Closed.

### 2.5  157ccb7 — Correct README test count and roadmap

The commit corrects the test count from 14 to 15 in `README.md` and adds
Phases 13 (DoIP), 14 (rear→exterior rename), and 15 (SOME/IP-SD) to the
roadmap. After the parser unit test commit (0d1f442) the count was bumped
again to 16.

**Does it close the named gap?** Yes.

**What did it not address?** A skeptical reader might ask whether a
README test count is worth a P1 priority. In the original review I argued it
was, on the grounds that an interviewer who notices a stale README test count
will discount the project's other claims. I still hold that view. The patch is
small but the reputational cost of leaving it is non-trivial.

**Severity.** Closed.

---

## 3  Audit of P2 / P3 Fix Commits

### 3.1  63e5b9c — Post-parse semantic validation for invalid enum bytes

The commit adds a `domain::IsValidLampCommand(cmd)` check at
`app/zephyr_nucleo_h753zi/main.cpp:654` after the parser returns and before
the command is enqueued onto `g_lamp_cmd_queue`. The validator is implemented
at `src/domain/domain_type_validators.cpp:55–67` and checks each enum field's
underlying byte against the maximum enumerator (`kHeadLamp = 5`, `kToggle =
3`, `kCentralZoneController = 3`).

The commit message correctly identifies that the original P2 finding ("verify
noexcept correctness on Zephyr paths that call SomeipMessageParser") was based
on a stale comment in `prj.conf` claiming the parser throws. The parser
contains zero `throw` expressions; `ReadUint8` and `ReadUint16` return a `0U`
sentinel on out-of-bounds read. The real gap was the absence of semantic
validation, not exception handling.

**Does it close the named gap?** It closes the actual underlying gap (semantic
validation) rather than the stated gap (noexcept correctness). The commit
message is honest about this.

**What did it not address?** Three issues remain.

First, `IsValidLampCommand` accepts `kUnknown = 0` because zero is in range
for all three fields (`kLampFunction::kUnknown = 0`,
`kLampCommandAction::kUnknown = 0`, `kCommandSource::kUnknown = 0`, see
`include/body_control/lighting/domain/lamp_command_types.hpp`). A frame with
all-zero command bytes parses, validates, and reaches the dispatcher as
`{kUnknown, kUnknown, kUnknown}`. The downstream `BlinkManager` switch
statements treat `kUnknown` as a no-op, so the bug is benign — but the
validator's name implies it would reject this and it does not.

Second, the validator is only called on the Zephyr UDP RX path
(`app/zephyr_nucleo_h753zi/main.cpp:654`). The Linux vsomeip and STM32
LwIP code paths do not have a corresponding call. If a malformed SOME/IP event
arrives over vsomeip, the `kUnknown` sentinel still gets into
`CommandArbitrator::OnLampCommandReceived`. Cross-platform consistency
requires the validation be moved into a higher-level shared dispatcher, not
duplicated per platform.

Third, the prj.conf comment was corrected, which is good. But
`CONFIG_CPP_EXCEPTIONS=y` is still required because libstdc++/libsupc++ runtime
needs it for the STL containers, even though no application code throws.
That tension is a real deployment concern: an unhandled exception originating
inside `std::vector` allocation (e.g., heap exhaustion) on a `noexcept`
boundary still calls `std::terminate()`. The Zephyr build at
`prj.conf:14` sets `CONFIG_HEAP_MEM_POOL_SIZE=8192`, which is small enough
that allocation failure under load is plausible. There is no `set_new_handler`
call in the codebase. This is not a fix-it-now defect, but it is a real
production concern that the comment-correction patch did not engage with.

**Severity.** P2/P3 follow-up depending on whether the cross-platform
validation gap is treated as a correctness or a hygiene issue.

### 3.2  9e38dc0 — Reject DecodeOffer frames with unaligned entries_len

The commit adds the modulo guard at
`src/transport/some_ip_sd/some_ip_sd_codec.cpp:243`:

```
if ((entries_len % kEntrySize) != 0U)
{
    return false;
}
```

A new unit test `DecodeOfferUnalignedEntriesLenReturnsFalse` at
`test/unit/test_some_ip_sd_codec.cpp:170` patches the entries_len field of a
valid frame from 16 to 17 and asserts `DecodeOffer` returns false.

**Does it close the named gap?** Yes. The arithmetic precondition for the
loop at line 250 is now enforced.

**What did it not address?** The bigger story in `DecodeOffer` is that it
quietly returns `true` with a partially-populated `DiscoveredService` in three
cases that look like errors but are silently treated as successful discovery
events:

- Line 268: `if (opts_start + 4U > sd_len) { return true; }` — options section
  missing entirely. The function returns success with `out.remote_ip[0] =
  '\0'` and `out.remote_port = 0`.
- Line 276: `if (opts_len < kOptionSize) { return true; }` — options length
  field too small to contain even one option. Same partial result.
- Line 280: `if (opt[2] != kOptTypeIpv4Ep) { return true; }` — option present
  but wrong type. Same partial result.

The caller at `app/zephyr_nucleo_h753zi/main.cpp` (and elsewhere) sees
`DecodeOffer` returned `true` and assumes it has a usable `DiscoveredService`,
when in fact the IP address is empty and the port is zero. The original
review noted this in passing; this pass calls it a P2 because it is the same
class of bug as the alignment issue (silent acceptance of malformed frames)
and the alignment fix did not extend to it.

The matching `DiscoveredService` struct at
`include/body_control/lighting/transport/some_ip_sd_types.hpp:34–41` does not
carry a `valid` flag or any indication that the endpoint fields are populated.
Adding one would let `DecodeOffer` return `true` only when the endpoint is
present, and clients could distinguish "found a service but no endpoint" from
"found a service with endpoint."

**Severity.** P2.

### 3.3  ed8367e — Mandatory CRC validation on RequestTransferExit (0x37)

The commit changes both `src/application/ota_session_manager.cpp:186` (Linux)
and `src/application/ota_session_manager_zephyr.cpp:251` (Zephyr) to require
`req.size() == 5U` and return NRC 0x13
(`kNrcIncorrectMessageLengthOrInvalidFormat`) when the request is shorter or
longer. The new NRC constant is added to the UDS service IDs header. The
existing test `FullTransfer_NoCrc_SuccessAndBecomesIdle` is renamed and
inverted to `RequestTransferExit_WithoutCrc_ReturnsNrc` at
`test/unit/test_ota_handler.cpp:199–212`. Two adjacent tests were updated to
build CRC explicitly via the new `MakeRequestTransferExitWithCrc` helper at
lines 53–65.

The commit message correctly identifies that this is hardening beyond ISO
14229-1 (which makes the parameterRecord optional) rather than a vulnerability
fix. The justification is reasonable — defense in depth on top of the
mandatory block sequence counter — but it is worth flagging that this is now
a non-standard quirk that any tester implementation must accommodate. The
Python OTA client at `tools/ota_client/ota_client.py` already sends 5 bytes,
so internal tooling is fine; an external tester written from the spec would
not be.

**Does it close the named gap?** Yes.

**What did it not address?** The length-NRC path itself is incomplete. At
`src/application/ota_session_manager.cpp:186–190`:

```
if (req.size() != 5U)
{
    return NegativeResponse(domain::uds::kSidRequestTransferExit,
                            domain::uds::kNrcIncorrectMessageLengthOrInvalidFormat);
}
```

This returns the NRC but does not transition `state_` out of `kActive`, does
not call `CloseStaging()`, does not unlink the staging file, and does not
call `Reset()`. The session remains active. A tester can repeatedly send
malformed 0x37 requests, getting NRC 0x13 each time, and the session stays
open. Compare against the size-mismatch path two checks higher at lines
173–181, which correctly transitions to `kFailed`, closes the staging file,
unlinks the staging artifact, and calls `Reset()`. The CRC-mismatch path at
lines 200–208 also does the cleanup.

The same problem exists at
`src/application/ota_session_manager_zephyr.cpp:251–255`, where the length-NRC
path also returns without cleanup. On Zephyr this is worse because the
`stream_flash_buffered_write` context at `s_stream_ctx` (file-scope at line
46) holds a partial buffer that has not been flushed. If the tester
abandons the session at this point and starts a new 0x34, the new
`stream_flash_init` will overwrite the context but the partially-buffered
bytes from the previous session may already have been written to flash by a
previous block transfer. Worst case: slot1 contains a hybrid of two
firmware images.

**Severity.** P2. This is a real state-machine bug now and was already a state
machine bug before the commit (the previous behavior of accepting bare 0x37
masked it). The fix tightened the front door without fixing the back door.

### 3.4  0d1f442 — Direct unit tests for SomeipMessageParser

The commit adds `test/unit/test_someip_message_parser.cpp` (120 lines, 7
tests) covering the parser's out-of-bounds-safe `ReadUint8` and `ReadUint16`
behaviour and the `ParseLampCommand` happy path.

**Does it close the named gap?** Yes. It also produces concrete evidence for
the claim that the parser is `noexcept`-correct (the tests pass without
catching exceptions).

**What did it not address?** The tests cover the parser, not its callers. The
real concern in the original review was about the call chain — what happens
when the parser is invoked from a Zephyr `noexcept` upper layer. Those call
chains still exist (`app/zephyr_nucleo_h753zi/main.cpp:619` calls
`SomeipMessageParser::ParseLampCommand`) but are not unit-tested because they
require a Zephyr UDP injection harness that the project does not have. The
parser commit is a useful piece of evidence; it is not a complete close-out
of the noexcept question.

**Severity.** Closed in scope; broader concern remains as a P3 (build a
Zephyr-side test harness).

### 3.5  45575a2 — Add ASan/UBSan job and fix latent narrowing bugs

The commit adds the `static-analysis-sanitizers` job at
`.github/workflows/build.yml:94–132` building all 16 unit tests under
`-fsanitize=address,undefined` at `-O1` with `ASAN_OPTIONS:
detect_leaks=1:halt_on_error=1:print_stacktrace=1:symbolize=1`. It also fixes
two latent `-Wconversion` narrowing bugs at
`test/unit/test_some_ip_sd_codec.cpp:74–75` and `91–92` that were silent under
RelWithDebInfo.

**Does it close the named gap?** Yes.

**What did it not address?** A few items.

First, the sanitizer job runs at `-O1`. Some classes of UB only manifest at
`-O0` (uninitialized stack reads with stack reuse) or `-O2/-O3` (LTO-driven
inlining shifts). `-O1` is a defensible compromise but not exhaustive.

Second, no ThreadSanitizer (TSan) job. The project has multiple threads in the
Linux target — `DoipServer::ListenerThread` at `src/transport/doip_server.cpp:96`,
`SomeIpSdSender` and `SomeIpSdListener` background threads, the main loop —
and at least one shared-state race that the original review's section 3
flagged: the `UdsRequestHandler` reads from `function_manager_` (no mutex)
while the SOME/IP callbacks write to it from the vsomeip dispatch thread. ASan
does not detect data races; TSan does. The fact that the comment at
`include/body_control/lighting/application/uds_request_handler.hpp:18–21`
acknowledges the race ("benign for this portfolio demo") and that no TSan job
exists to verify the benign-ness claim is a small but real soft spot.

Third, `detect_leaks=1` is set but no fuzzing target exists. The codecs
(`SomeIpSdCodec::DecodeOffer`, `SomeipMessageParser::ParseLampCommand`,
`LightingPayloadCodec::DecodeFromBytes`) are exactly the kind of narrow-input,
deterministic functions where libFuzzer would reach high coverage in seconds.
Adding a fuzz target would catch the kind of edge case that the
`DecodeOfferUnalignedEntriesLenReturnsFalse` test caught by manual
construction.

Fourth, the linux-build job in `.github/workflows/build.yml:14` builds at
`Release`. There is no parallel Debug build. Some warnings (unused-variable in
release-only paths, narrowing in debug-assert macros) only fire in Debug
builds. That said, the sanitizer job builds at Debug and catches some of
this incidentally.

**Severity.** Closed in scope; TSan and fuzzing gaps tracked as P3 in section
17.

### 3.6  Documentation-only commits (c1a22f5, 8260c58)

c1a22f5 adds the Status block at `doc/PROJECT_REVIEW.md:1–40` and reframes the
review as a self-review rather than a third-party review. The hardware
verification block at lines 16–38 contains real captured payload bytes from
the NUCLEO. 8260c58 converts section 12 to first-person voice.

These are voice and accuracy fixes. They have no code impact. They do correct
a misleading framing (the original review presented as if written by an
external engineer, which would have been disingenuous to leave in place for a
portfolio review document attributed to the project author).

**Severity.** Closed.

---

## 4  New Findings — What the First Review Missed

This is the most important section of this pass. Each finding here cites
file:line, names the severity I assess against the same P1/P2/P3/P4 scale used
in the first review, and is grounded in code I read this time around.

### 4.1  N-01 — DoIP per-request 64 KB heap allocations on Linux

`src/transport/doip_server.cpp:151`:

```
std::vector<std::uint8_t> payload(payload_len);
```

`payload_len` is bounded only by `kMaxPayloadLength = 65536` at line 145. A
hostile or buggy tester sending a steady stream of headers with `payload_len
= 65536` and slow body bytes can keep a 64 KB heap allocation live per
connection for the duration of the TCP receive window. The Zephyr
implementation correctly uses a static buffer at line 901 (`static
std::uint8_t payload_buf[kDoipMaxPayloadLen]` with `kDoipMaxPayloadLen =
1024U` at line 847). The 64x size mismatch between platforms is not just a
porting curiosity — it means the Linux build's memory-pressure profile is
qualitatively different from the embedded build's, and the embedded build's
profile is the one that matters for the portfolio's claim of
embedded-discipline.

**Severity:** P2. Fix: pre-allocate a per-connection buffer of size
`kMaxPayloadLength` and reuse it across requests, matching the Zephyr
pattern.

### 4.2  N-02 — DoIP routing activation type byte never validated

`src/transport/doip_server.cpp:191`:

```
if (length < 7U) { return false; }
```

The handler checks the length but never reads or validates `payload[2]`, the
activation-type byte. ISO 13400-2 defines four named values: 0x00 (default),
0x01 (WWH-OBD), 0x02 (central security), 0xE0–0xFF (vendor reserved). Any
value, including reserved values like 0x0F or 0xFE, is accepted with the same
"routing activated" response. The Zephyr code at
`app/zephyr_nucleo_h753zi/main.cpp:921` has the same gap. The first review's
section 9 (protocol/transport) discussed DoIP without flagging this.

**Severity:** P2. Spec conformance gap; not exploitable but not standards-
compliant.

### 4.3  N-03 — `SomeIpSdCodec::DecodeOffer` returns success with empty endpoint

`src/transport/some_ip_sd/some_ip_sd_codec.cpp:268`, `:276`, `:280`. Each
of these three branches returns `true` with `out.remote_ip[0] = '\0'` and
`out.remote_port = 0`. The caller, with no way to distinguish this from a
fully-decoded offer, will attempt to open a connection to "" port 0 — which
will fail with EINVAL or similar. The real bug is not the connection failure;
it is that the same return value (`true` plus a populated `service_id`)
encodes "I found the service" and "I found a service-shaped frame but no
endpoint."

The `DiscoveredService` struct at
`include/body_control/lighting/transport/some_ip_sd_types.hpp:34–41` does not
carry a TTL field either, even though the `OfferService` entry contains one
and `ServiceOffer` (the encoder side) carries `ttl_seconds {5U}` at line 27.
The decoder reads the TTL at no point — `DecodeOffer` reads `entry[8]` as
`major_version` at codec.cpp:261 but skips entry bytes 9–11 (which are the
24-bit TTL field). Without TTL the listener cannot expire stale offers,
making the implementation a one-shot service discovery rather than a real
SOME/IP-SD listener.

**Severity:** P2 (endpoint-empty case) plus P2 (TTL not parsed). Fix:
introduce a `valid` flag on `DiscoveredService`, parse TTL into the struct,
and return `false` (or a documented "found-but-incomplete" sentinel) for the
empty-endpoint cases.

### 4.4  N-04 — `g_sd_session` 16-bit counter wraps at 65536 offers

`app/zephyr_nucleo_h753zi/main.cpp:135`:

```
static std::uint16_t    g_sd_session {1U};
```

The session ID is incremented inside `SdOfferWorkHandler` at line 169
(`EncodeOffer(offer, g_sd_session++)`) and is sent at 2-second intervals. At
65536 offers / 2s = ~36.4 hours, the counter wraps. AUTOSAR
AP_PRS_SOMEIPServiceDiscovery says session ID 0 is reserved for "session
restart" — which is exactly what the post-wrap value 0 will look like to a
listener. A listener that tracks session continuity will see it as an
intentional reboot announcement, not a counter wrap. The
`kFlagsRebooting = 0xC0U` flag at codec.cpp:35 is set unconditionally on every
offer, which the first review flagged as suspicious (the spec says the
reboot flag should be set only for the first N offers after a reboot).
Combined with the session ID wrap, the listener semantics are unclear at
36-hour boundaries.

**Severity:** P3. Long uptime behavior. Fix: bump session-id counter to 32-bit
internally and modulo-65535 (skipping zero) before passing to the encoder; or
just clamp at 65535 for the simulation workload.

### 4.5  N-05 — `g_sd_fd` socket never closed

`app/zephyr_nucleo_h753zi/main.cpp:134`:

```
static int              g_sd_fd      {-1};
```

Opened lazily inside `SdOfferWorkHandler` at line 147, never closed. On a
firmware with no shutdown path (Zephyr runs forever or reboots), this is
benign in practice. The pattern is still wrong: there is no path that closes
the socket on test teardown, on `OtaSessionManager` reboot trigger
(`sys_reboot()` at `ota_session_manager_zephyr.cpp:60` does not flush
sockets), or on assertion failure. The Linux equivalent at
`src/transport/some_ip_sd/some_ip_sd_sender.cpp` and `..._listener.cpp` at
least has destructors that close the sockets.

**Severity:** P4. Not a problem in practice, but a hygiene gap that a
seasoned reviewer would call out.

### 4.6  N-06 — OTA length-NRC path strands the session (Linux and Zephyr)

Already discussed in section 3.3. Repeated here for the findings register.

`src/application/ota_session_manager.cpp:186–190` and
`src/application/ota_session_manager_zephyr.cpp:251–255` return NRC 0x13 on
malformed 0x37 length but do not call `Reset()`, do not transition `state_`,
do not close the staging file, and do not unlink. On Zephyr, the
stream_flash partial buffer is also left dangling.

**Severity:** P2.

### 4.7  N-07 — `EncodeNodeHealth` `eth_link` and `svc_avail` still hard-coded

`src/application/uds_request_handler.cpp:354`:

```
const std::uint8_t flags =
    static_cast<std::uint8_t>(0x03U | fault_bit);  // eth + svc always up
```

The comment is honest. Two of the three flag bits encode placeholder
"always-up" values. The UDS handler does not have access to the transport
adapter or the SD listener state, so the natural fix is to plumb a status
provider through `UdsRequestHandler`'s constructor or accept a callback. The
first review's Probe 5 in section 12 named this; the f9218c8 fix did not
extend to it.

**Severity:** P2.

### 4.8  N-08 — `EncodeEcuIdentification` `reinterpret_cast` still present

`src/application/uds_request_handler.cpp:389–391`:

```
return std::vector<std::uint8_t>(
    reinterpret_cast<const std::uint8_t*>(kEcuId),
    reinterpret_cast<const std::uint8_t*>(kEcuId) + len);
```

Flagged P3 in the first review. Not addressed. The fix is mechanical — replace
with a pre-sized vector and `std::memcpy`. The reason it stays open is
priority, not difficulty.

**Severity:** P3 (same as before).

### 4.9  N-09 — `IsValidLampCommand` accepts the all-zero frame

`src/domain/domain_type_validators.cpp:55–67`. Each enum's underlying maximum
is checked, but every enum has `kUnknown = 0` as its first value, so a frame
with all-zero command bytes parses successfully and reaches the dispatcher as
`{kUnknown, kUnknown, kUnknown}`. Downstream switch statements treat
`kUnknown` as a no-op so the bug is benign in practice, but the validator's
name implies it is a complete validity check. The honest implementation
would explicitly reject `kUnknown` for at least the function and action
fields.

**Severity:** P3.

### 4.10  N-10 — Validator is per-platform, not centralised

`app/zephyr_nucleo_h753zi/main.cpp:654` calls
`domain::IsValidLampCommand(cmd)` after parsing. The Linux and STM32 paths
have no equivalent call. If the validator is a real safety check, it belongs
in the dispatcher (`CommandArbitrator::OnLampCommandReceived`) where every
platform's parsed commands converge. As implemented it is per-platform and
relies on each platform's author to remember the call.

**Severity:** P3.

### 4.11  N-11 — `HandleReadDtcInformation` unbounded loop iteration

`src/application/uds_request_handler.cpp:296`:

```
for (std::size_t i = 0U; i < fault_status.active_fault_count; ++i)
```

`active_fault_count` is a `std::uint8_t` populated from
`FaultManager::PopulateHealth`. The fault array `active_faults[]` is sized at
`kMaxActiveFaults = 5` (per `domain::LampFaultStatus`). If
`active_fault_count` is ever larger than `kMaxActiveFaults` (because the
fault manager wrote past the array bound), this loop reads out of bounds.
There is no `std::min(active_fault_count, kMaxActiveFaults)` clamp. The
fault manager's bookkeeping is correct in the code I have read, but the loop
is robust only to the extent that the fault manager is bug-free.

**Severity:** P3. Defense-in-depth rather than active vulnerability.

### 4.12  N-12 — `HandleClearDiagnosticInformation` rejects per-DTC clears

`src/application/uds_request_handler.cpp:193`:

```
if (group != domain::uds::kDtcGroupAll)
```

Only `0xFFFFFF` (all DTCs) is accepted. ISO 14229-1 §11.3 also defines
group-specific clears (e.g., 0x000000 = emissions, 0x33xxxx = chassis). A
tester sending `14 33 00 00` (clear chassis DTCs) gets NRC 0x31 instead of
selectively clearing chassis-grouped lamp faults (B001–B005 are body codes,
so the clear-chassis path would arguably be a no-op anyway, but the response
should reflect that, not "out of range"). Not all UDS implementations support
per-group clears, and the `request out of range` NRC is acceptable per spec
when only `0xFFFFFF` is supported. This is a correctness-by-convention call;
the implementation is defensible. Calling it out for completeness.

**Severity:** P4 (acceptable per spec when documented).

### 4.13  N-13 — `MAX_FIRMWARE_SIZE = 10 MiB` larger than slot1 partition

`include/body_control/lighting/application/ota_session_manager.hpp:73`:

```
static constexpr std::uint32_t kMaxFirmwareSize {10U * 1024U * 1024U};
```

Slot1 on the NUCLEO-H753ZI is 768 KB (per the overlay file). A request with
size between 768 KB + 1 and 10 MB will be accepted by `HandleRequestDownload`
at `src/application/ota_session_manager_zephyr.cpp:104`, then fail at flash
write time inside `stream_flash_buffered_write` once the buffered bytes
exceed `s_flash_area->fa_size`. The failure is detected (line 204 returns
NRC) but not preempted at request time. Pre-emptive validation against
`s_flash_area->fa_size` (which we already query at line 129) would be
mechanical to add.

**Severity:** P3. The eventual error code is correct; the lack of early
rejection means we burn bandwidth and time on an OTA we know will fail.

### 4.14  N-14 — `CONFIG_WATCHDOG=y` not present on Zephyr

`app/zephyr_nucleo_h753zi/prj.conf` enables `CONFIG_REBOOT=y` at line 67 but
not `CONFIG_WATCHDOG=y`. `safety/05_fmea.md` FM-010 (independent watchdog)
is named as an open RPN-112 gap; the Zephyr build does not have software-side
watchdog support either. The STM32H7 has both an IWDG (independent) and a
WWDG (window) peripheral; Zephyr's `wdt_install_timeout` API is the standard
hook. The first review noted FM-010 as "open" but did not name the missing
prj.conf line.

**Severity:** P2 in safety language; P3 in portfolio language because the
README and FMEA both name this as a gap.

### 4.15  N-15 — MCUboot test key still in use in production-shaped artifacts

`app/zephyr_nucleo_h753zi/sysbuild/mcuboot.conf` references the MCUboot
upstream test key for image signing. `doc/security_architecture.md` gap 5.4
names this. The portfolio narrative is honest about it. Two small additional
items worth flagging:

1. The Python OTA client at `tools/ota_client/ota_client.py` does not sign
   anything — it sends the raw image, MCUboot validates the signature embedded
   in the image header at boot. So the signing happens at build time via the
   sysbuild flow, not at OTA time. A reader of the OTA flow without the
   security architecture context might miss this. The first review's section 14
   appendix touched on it at lines 936–938.
2. There is no key-rotation procedure documented anywhere. Production OTA
   programmes need a story for "what happens if the key is compromised."
   `doc/security_architecture.md` is silent on this.

**Severity:** P3 (procedural gap, not implementation gap).

### 4.16  N-16 — No fuzzing target despite codec-heavy design

The codecs (`SomeIpSdCodec::DecodeOffer`, `SomeipMessageParser::ParseLampCommand`,
`LightingPayloadCodec::DecodeFromBytes`, the DoIP frame parser inline at
`src/transport/doip_server.cpp:127–149` and
`app/zephyr_nucleo_h753zi/main.cpp:905–919`) are deterministic input-output
functions with narrow contracts — exactly the shape libFuzzer is designed
for. The CMake build supports adding a fuzz target via
`-fsanitize=fuzzer`. None exists. The
`DecodeOfferUnalignedEntriesLenReturnsFalse` test was found by manual
construction; a fuzzer would have surfaced it (and others) automatically.

**Severity:** P3.

### 4.17  N-17 — No coverage measurement in CI

The CI runs `ctest --output-on-failure` and reports pass/fail. There is no
`gcov` or `llvm-cov` job that produces a coverage percentage. The README
test-count (16) is a count of test files, not a coverage statistic. Without
coverage, a reader cannot tell whether the 16 tests exercise 30% or 90% of
the production code. A `gcovr --xml-pretty` job uploaded as a build artifact
would close this gap with about 30 lines of YAML.

**Severity:** P3.

### 4.18  N-18 — No Zephyr CI

Reaffirming the first review's finding. None of the five CI jobs at
`.github/workflows/build.yml` builds the Zephyr target. A `west build -b
qemu_cortex_m3` (or qemu_cortex_m7) job would catch Zephyr-side build
breakage that is currently invisible to the CI gate. The first review listed
this as P4 ("requires non-trivial toolchain setup"). I think it should be
upgraded to P3 because:

- Two of the recent fix commits (63e5b9c, f9218c8) modified Zephyr-only code
  that the CI does not build.
- The Zephyr-only DoIP path at `app/zephyr_nucleo_h753zi/main.cpp:894` has no
  unit-test coverage either, so neither CI nor unit tests would catch a
  regression in it.
- A QEMU build on every PR is a proven pattern (Zephyr's own CI uses it) and
  the prj.conf is small enough that the build time is bounded.

**Severity:** P3 (upgraded from P4).

---

## 5  Architecture (Re-read)

The first review's architecture section was generous. On second reading it
is mostly fair, with two refinements.

The `TransportAdapterInterface` at
`include/body_control/lighting/transport/transport_adapter_interface.hpp` is
a real seam. The first review called it a "genuine seam that lets the same
application code compile for Linux vsomeip, STM32 LwIP, and Zephyr BSD
sockets without `#ifdef` in the logic layers." That is true at the C++
language level. At the operational level, the three implementations have
qualitatively different semantics:

- **Linux vsomeip**: blocks until the vsomeip dispatch thread acknowledges,
  uses the vsomeip event/method abstraction.
- **STM32 LwIP raw API**: callback-driven, must be polled from the main loop.
- **Zephyr BSD sockets**: blocking syscalls on dedicated threads.

A senior reviewer asking "what is the tail latency profile of `SendEvent` on
each platform?" will get three answers. The interface hides the difference
but does not eliminate it. The first review did not engage with this level
of detail.

The CMake separation between `domain`, `application`, `service`, `transport`,
and `platform` static libraries is real. I confirmed by reading
`CMakeLists.txt` that `domain` has zero transport or platform dependencies,
which is the load-bearing claim. That is more than most portfolio projects
manage.

The shared-state concurrency model is the soft underbelly. The
`UdsRequestHandler` reads from `function_manager_` from the DoIP thread
without locking, while SOME/IP callbacks write to the same object from the
vsomeip dispatch thread. The header acknowledges this at lines 18–21 of
`include/body_control/lighting/application/uds_request_handler.hpp`. For a
portfolio piece, this is acceptable with the disclosure. For an
interview-probe answer to "how would you make this race-free?" the answer
should be "introduce a command-queue at `function_manager_` boundary,
serialize all writes through it." That fix is not in the codebase.

---

## 6  Code Quality

The `[[nodiscard]]` discipline is consistent across the public API surface I
read: `OtaSessionManager::HandleRequestDownload` at line 45 of the header,
`UdsRequestHandler::HandleRequest` at line 38 of its header,
`SomeIpSdCodec::EncodeOffer` (return type used unconditionally by callers).
This is a small but real quality signal.

The `noexcept` discipline is also consistent on the application path. The
parser, codecs, validators, dispatchers are all `noexcept`. The exception is
the destructor of `OtaSessionManager` at
`include/body_control/lighting/application/ota_session_manager.hpp:36`, which
is correctly marked `noexcept` despite the `// NOLINT(performance-trivially-
destructible)` comment that hints it should be defaulted-trivial. Reading
the implementation at `src/application/ota_session_manager.cpp:19–27`, the
destructor calls `CloseStaging()` (closes the fd) and may unlink the staging
file. It cannot be defaulted-trivial; the NOLINT is correct.

What I would flag this pass that the first review did not:

The `static` storage of `s_flash_area`, `s_stream_ctx`, and `s_stream_buf` at
`src/application/ota_session_manager_zephyr.cpp:45–50` is at file scope.
That is correct for a one-instance-per-device system, but it makes the
class non-reentrant in a subtle way: two `OtaSessionManager` objects on the
same Zephyr build would share `s_flash_area` and corrupt each other. The
header does not make this clear; the constructor would happily allow two
instances. A `static_assert` on construction count, or a singleton pattern,
would close the ambiguity.

The first review's section 14 correctly noted no TODO/FIXME/XXX/HACK in the
codebase. I confirmed this remains true at HEAD. Discipline preserved.

---

## 7  Test Coverage

Test count is now 16 (per
`README.md` and `test/unit/CMakeLists.txt`). Test files cover:

- `test_command_arbitrator.cpp` — command arbitration logic
- `test_exterior_lighting_function_manager.cpp` — function manager
- `test_fault_manager.cpp` — fault state
- `test_lamp_state_manager.cpp` — lamp state machine
- `test_lighting_payload_codec.cpp` — wire-format codec
- `test_operator_service_get_all_lamp_states.cpp` — service request handler
- `test_ota_handler.cpp` — OTA session manager (16 tests after ed8367e)
- `test_some_ip_sd_codec.cpp` — SOME/IP-SD encode/decode (12 tests after
  9e38dc0)
- `test_someip_message_parser.cpp` — parser (7 tests after 0d1f442)
- `test_uds_request_handler.cpp` — UDS request handler

What is tested well: codec round-trips, parser bounds, OTA state transitions,
fault state propagation. The new
`DecodeOfferUnalignedEntriesLenReturnsFalse` and
`RequestTransferExit_WithoutCrc_ReturnsNrc` are good targeted tests.

What is not tested:

- `DoipServer` end-to-end (Linux). No test connects to the listening socket
  and exercises the routing-activation + diagnostic-message flow.
- The Zephyr DoIP path at `app/zephyr_nucleo_h753zi/main.cpp:894`. No
  injection harness exists.
- `SomeIpSdSender` and `SomeIpSdListener` thread lifecycle (start, stop,
  socket teardown).
- `OperatorServiceProvider` and `OperatorServiceConsumer` UDP path.
- Cross-platform validator parity. `IsValidLampCommand` is invoked only on
  Zephyr; no test enforces the invocation contract elsewhere.
- OTA failure cleanup. The new tests cover the success and CRC-mismatch
  paths but not the new length-NRC path's session-leak behavior — which is
  the bug I describe in section 3.3.

A test for `RequestTransferExit_WithoutCrc_DoesNotResetState` would fail
today, which is exactly why it should be written.

---

## 8  CI and Static Analysis

Five jobs at `.github/workflows/build.yml`:

1. `linux-build` (ubuntu-24.04) — Release build + ctest.
2. `static-analysis-cppcheck` — cppcheck on src/ and app/.
3. `static-analysis-sanitizers` (post 45575a2) — ASan + UBSan at -O1.
4. `static-analysis-clang-tidy` — clang-tidy on selected directories.
5. `vss-build-test` — VSS=ON build + ctest + `--vss-snapshot` flag check.

What works:

- `cache-vsomeip` keyed at `vsomeip-3.4.10-ubuntu24` is consistent across
  all jobs that need vsomeip.
- The `Verify --vss-snapshot flag wired in` step at line 258 is a small
  integration check that catches regressions between the diagnostic console
  CLI and the VSS code path.
- ASan has `detect_leaks=1:halt_on_error=1` set, which catches both leaks
  and memory errors.

What does not:

- No TSan job. (See section 4 N-18.)
- No coverage. (See N-17.)
- No fuzzing. (See N-16.)
- No Zephyr build. (See N-18.)
- The `clang-tidy` job at line 178 finds files via `find -name "*.cpp"`
  piped to `xargs`. The list of directories is hand-curated. New
  directories (e.g., `src/vss/`) require manual updates. A `find src/ app/
  -name "*.cpp"` would auto-discover.
- The cppcheck step at line 84 uses `--suppressions-list` but the
  suppressions file (`cppcheck-suppressions.txt`) is a flat list with no
  per-suppression rationale. Some entries may be stale. A periodic audit
  would be reasonable.

---

## 9  Security

The first review's security section (section 6) covered eight gaps from
`doc/security_architecture.md`. After the OTA mandatory-CRC fix the picture
has shifted:

- **MCUboot test key (gap 5.4)**: Still open. The fix is procedural (rotate
  to a real key in the build pipeline) and gated on a production deployment.
  See N-15.
- **OTA CRC bypass**: Closed by ed8367e. Now the front door is locked.
- **OTA session leakage**: Newly identified in section 3.3 (N-06). The
  state-machine cleanup gap is a defense-against-incompetent-tester issue,
  not a real attack vector.
- **DoIP routing activation type unchecked (N-02)**: Spec conformance gap;
  not exploitable but means a malformed tester could establish a routing
  session without selecting an activation type.
- **No DoIP authentication**: Per ISO 14229-1, security access (0x27) is
  the right place. Not implemented. Out of scope for this portfolio
  iteration; named in `doc/security_architecture.md`.
- **No transport encryption**: SOME/IP and DoIP run unencrypted. AUTOSAR
  covers this with TLS 1.2+. Out of scope; documented gap.

The honest position remains: the project demonstrates security-aware
design (signed images, mandatory CRC after fix, FIDO-style test-then-confirm
boot) but does not implement transport-layer authentication. That is fine
for a portfolio. It is a known gap and named as such.

---

## 10  Safety

`safety/05_fmea.md` and `safety/06_safety_mechanism_implementation_map.md`
are now consistent with the codebase after b2e7b6a. FM-007 cites
`ota_session_manager.cpp` and the safety mechanism row cites
`OtaSessionManager::HandleTransferData()` at lines 124–152. Both references
match HEAD.

Open RPN items in `safety/05_fmea.md`:
- FM-002 (GPIO stuck-high, RPN 120) — open, unchanged.
- FM-005 (network flood, RPN 96) — open, unchanged.
- FM-010 (absent IWDG, RPN 112) — open, unchanged. See N-14.

The lack of `CONFIG_WATCHDOG=y` (N-14) is the most concrete safety gap.
Adding it would not make the project ASIL B; it would close a named FMEA
item and raise the confidence floor.

The safety/ tree does not have a `safety/changelog.md` or equivalent
recording when FMEA rows were last reviewed. For a portfolio piece this is
acceptable. For a real ASIL B program it would be required (ISO 26262-2 §6
demands traceable change history). Not a defect of this iteration; an
escape-hatch flag for the production-quality discussion.

---

## 11  SDV / VSS

After c6c31cd the `doc/vss_integration_design.md` document is consistent with
`src/vss/vss_lamp_overlay.hpp`, `src/vss/vss_lamp_overlay.cpp`, and the
diagnostic console wiring. I spot-checked 4 of the 17 namespace edits and 3
of the 11 field-name edits against the live code. They match.

Two small follow-ups.

First, `src/vss/vss_lamp_overlay.cpp:18` and `:43` carry
`// cppcheck-suppress useStlAlgorithm` comments. The first review's appendix
flagged these as legitimate suppressions. They still are. The
linear-scan loops are the natural form for the operation; converting to
`std::find_if` would be marginally cleaner but the suppressions are
defensible.

Second, the VSS overlay uses `Vehicle.Private.BCL.Lighting.*` rather than
the public `Vehicle.Body.Lights.*` taxonomy. `doc/vss_integration.md:72–82`
explains the choice (avoid unilaterally proposing changes to the public VSS
catalog). That is the right answer for a portfolio. A production deployment
would either upstream the extension to COVESA or document the namespace as
a vehicle-private extension, both of which the doc covers.

---

## 12  Protocol / Transport

After 9e38dc0 and ed8367e the codec-side hardening is real. Three concerns
remain that the first review did not name:

**SOME/IP-SD TTL not parsed (N-03 second clause).** `DecodeOffer` reads the
major version from `entry[8]` at codec.cpp:261 but skips the 24-bit TTL
field at `entry[9..11]`. Without TTL, the listener cannot expire stale
offers. A service that goes away (provider crashes, network partitions)
remains "discovered" forever as far as this listener is concerned.

**SOME/IP-SD reboot flag set on every offer.** `kFlagsRebooting = 0xC0U` at
codec.cpp:35 is set unconditionally on every encoded offer. AUTOSAR says
the reboot flag should toggle per session-id epoch. The first review noted
this in passing. A listener that respects the spec would interpret every
offer as a reboot announcement.

**DoIP routing activation type not validated (N-02).** Section 4 already
covers this.

The first review's section 9 was correct that the DoIP and SOME/IP-SD
implementations are traceable to the specs. This pass refines the picture:
they are traceable, but not fully spec-conformant. The gaps are documented
here but not acknowledged in the code comments.

---

## 13  OTA

Three items beyond what the first review covered.

**Length-NRC path strands the session (N-06).** Section 3.3 has the detail.
This is the most concrete OTA bug at HEAD.

**Pre-emptive size validation against slot1 size (N-13).** The 10 MiB
`kMaxFirmwareSize` in the header is generous; the actual slot1 is 768 KB.
Add `if (size > s_flash_area->fa_size) return NRC` in the Zephyr handler.

**100 ms reboot delay is bare-minimum on Zephyr.**
`src/application/ota_session_manager_zephyr.cpp:312` schedules
`k_work_schedule(&s_reboot_work, K_MSEC(100))`. The comment at lines 310–311
correctly notes this is to let DoIP TCP transmit the positive response.
On a busy LAN, 100 ms may be insufficient if there is retransmission.
500 ms or 1000 ms would be safer. The trade-off is "tester sees the OTA
hung" vs "system reboots before tester sees positive response." 100 ms is
defensible; longer would be safer.

The MCUboot test-then-confirm pattern at HealthThread (boot_write_img_confirmed
called once after first successful health TX, lines 808–816) is correct
and demonstrably understood. This is a strong piece of the project.

---

## 14  Documentation

After c1a22f5, c6c31cd, b2e7b6a, 0a9ff73, 157ccb7, 8260c58 the documentation
is in better shape than at d812918. The Status block at the top of
`doc/PROJECT_REVIEW.md:1–40` is unusual and worth retaining: it explicitly
lists which findings have been addressed and which are still open, which is
exactly what a hiring manager would look for in a "tracked" project.

Two doc gaps that this pass surfaces but did not warrant their own finding
in section 4:

- `doc/security_architecture.md` does not document key rotation. (N-15
  second sub-bullet.)
- `safety/` does not have a changelog. Section 10 covers this.

Neither is a P1 or P2. Both are signals that would matter in a senior
review.

---

## 15  Portfolio Positioning (Self-criticism)

This pass is supposed to be more critical than the first. Here is the
self-criticism that is hardest to write.

The first review's section 12 ("Five most likely senior-interviewer
probes") predicted Probe 5 verbatim about the `eth_link` and `svc_avail`
hard-coding. The fix commit (f9218c8) closed the named gap (fault state)
but not the predicted probe (link state). The probe will still land in an
interview today. That is a self-discipline failure: knowing what an
interviewer will ask and not closing the loop.

The first review under-estimated the depth of the OTA state-machine bugs.
It named the CRC bypass but did not look at the cleanup paths. Section 3.3's
length-NRC stranding bug was visible in the same function the CRC gap was
in; I missed it. That is not a mistake about priority, it is a mistake about
reading depth.

The first review classified Zephyr CI as P4. After the fix-commit history I
think P3 is correct (see N-18). Two of the last twelve commits modified
Zephyr-only code that the CI does not build. The probability of a Zephyr-side
regression escaping into HEAD is now non-zero. Upgrading to P3 is honest.

What this pass got right that the first review did not:

- Reading every fix commit's diff before judging whether the fix closed the
  named gap. The first review judged severity from intent; this pass judges
  closure from diff. The two are different.
- Looking at the second-order code in the neighborhood of each patch. The
  HealthThread `eth_link` issue, the OTA cleanup issues, the
  `DiscoveredService` empty-endpoint issue all live in the same files that
  were modified by the P1/P2 commits.
- Calling the platform-specific implementation gap of the validator
  (N-10). The first review treated 63e5b9c as a Zephyr-only fix because the
  bug was Zephyr-only. The fix is, but the architectural gap is not.

What this pass still does not catch (by my own honest estimate):

- LwIP-side semantics in `app/stm32_nucleo_h753zi/main.cpp`. I did not
  re-read that file in this pass. The original review's section 9
  observations about it stand, but I did not audit its DoIP implementation
  the same way I did for Zephyr. That is a gap in this pass's coverage.
- VSS overlay test coverage. `test/test_vss_lamp_overlay.cpp` (referenced
  in the c6c31cd commit message) was not re-read. The doc alignment is
  correct; I did not verify the test alignment.
- Wire-format hex captures. `doc/captures/` exists and the README references
  it. I did not open the captures themselves to verify they match the
  current codec output. The first review's section 9 vouched for them; I am
  re-vouching by reference, not by re-inspection.

Each of these is a correctness-by-trust assertion that a third pass should
audit.

---

## 16  Comparative Severity Rebaseline

The first review's section 13 had a flat priority ranking with priorities
P1 through P4. This pass rebases the priorities against the post-fix state.

| Item | First-review pri | Status | Second-review pri |
|---|---|---|---|
| HealthThread fault state | P1 | Fixed (f9218c8) | Closed |
| GPIO comment vs DTS | P1 | Fixed (0a9ff73) | Closed |
| FMEA stale OTA reference | P1 | Fixed (b2e7b6a) | Closed |
| VSS design doc namespace | P1 | Fixed (c6c31cd) | Closed |
| README test count | P1 | Fixed (157ccb7) | Closed |
| OTA mandatory CRC | P2 | Fixed (ed8367e) | Closed (front door) |
| SD entries_len alignment | P2 | Fixed (9e38dc0) | Closed |
| Parser noexcept verification | P2 | Reframed + tests added (63e5b9c, 0d1f442) | Closed in scope |
| vsomeip CI cache unification | P2 | Not done | P3 |
| ASan/UBSan CI | P3 | Done (45575a2) | Closed |
| SD malformed-frame fuzz tests | P3 | Partial (one new test) | P3 |
| Test EncodeNodeHealth OTA mode | P3 | Not done | P3 |
| Replace reinterpret_cast in EncodeEcuIdentification | P3 | Not done | P3 |
| Replace SIZE_MAX sentinel | P3 | Not done | P3 |
| Zephyr QEMU CI | P4 | Not done | **P3** (upgraded) |
| main.cpp decomposition | P4 | Not done | P4 |
| Document signature verification timing | P4 | Not done | P4 |
| **NEW** Eth_link / svc_avail still hard-coded | — | — | P2 (N-07) |
| **NEW** DoIP per-request 64KB allocation | — | — | P2 (N-01) |
| **NEW** DoIP routing activation type unchecked | — | — | P2 (N-02) |
| **NEW** SD endpoint-empty returns true | — | — | P2 (N-03) |
| **NEW** SD TTL not parsed | — | — | P2 (N-03) |
| **NEW** OTA length-NRC strands session | — | — | P2 (N-06) |
| **NEW** OTA size > slot1 not pre-validated | — | — | P3 (N-13) |
| **NEW** No CONFIG_WATCHDOG | — | — | P2 (N-14) |
| **NEW** SD session-id wraps at 36h | — | — | P3 (N-04) |
| **NEW** SD socket never closed | — | — | P4 (N-05) |
| **NEW** Validator accepts all-zero | — | — | P3 (N-09) |
| **NEW** Validator per-platform | — | — | P3 (N-10) |
| **NEW** ReadDtcInformation unbounded loop | — | — | P3 (N-11) |
| **NEW** No TSan job | — | — | P3 |
| **NEW** No coverage measurement | — | — | P3 (N-17) |
| **NEW** No fuzzing target | — | — | P3 (N-16) |
| **NEW** MCUboot key rotation not documented | — | — | P3 (N-15) |

Net delta: 5 P1 closed, 3 P2 closed, 1 P3 closed; 6 new P2, 9 new P3, 1
new P4. The total open-issue count is similar to before but the severity
distribution has shifted from "blocking before showing to hiring panel" to
"discoverable in interview deep-dive." That is the right direction.

---

## 17  Prioritized Recommendations

Priority codes: **P1** = blocks portfolio readiness; **P2** = fix before
senior interview; **P3** = nice-to-have hardening; **P4** = defer.

| Pri | Title | Rationale | Effort | Files |
|---|---|---|---|---|
| P2 | Fix OTA length-NRC session leak | Both Linux and Zephyr managers fail to clean up state on the new mandatory-CRC NRC path. Section 3.3 / N-06. | 30 min | `src/application/ota_session_manager.cpp:186–190`, `src/application/ota_session_manager_zephyr.cpp:251–255` |
| P2 | Make `eth_link` and `svc_avail` reflect actual transport state | Currently hard-coded to true in both EncodeNodeHealth and HealthThread. Pass a status provider into UdsRequestHandler. N-07. | 2 hours | `src/application/uds_request_handler.cpp:354`, `app/zephyr_nucleo_h753zi/main.cpp:789–790` |
| P2 | Validate DoIP routing activation type byte | `payload[2]` never checked. Reject reserved values; accept 0x00, 0x01, 0xE0. N-02. | 30 min | `src/transport/doip_server.cpp:191`, `app/zephyr_nucleo_h753zi/main.cpp:921` |
| P2 | Bound DoIP per-request allocations to a per-connection buffer | Linux build allocates up to 64 KB per request; Zephyr correctly uses 1 KB static. Match the Zephyr pattern on Linux. N-01. | 1 hour | `src/transport/doip_server.cpp:151` |
| P2 | Parse SOME/IP-SD TTL field and add `valid` flag to DiscoveredService | TTL is skipped at codec.cpp:261; struct cannot signal partial decode. N-03. | 1 hour | `src/transport/some_ip_sd/some_ip_sd_codec.cpp:259–293`, `include/body_control/lighting/transport/some_ip_sd_types.hpp:34–41` |
| P2 | Enable CONFIG_WATCHDOG and install a Zephyr `wdt_install_timeout` | Closes FMEA FM-010 (RPN 112). N-14. | 1 hour | `app/zephyr_nucleo_h753zi/prj.conf`, `app/zephyr_nucleo_h753zi/main.cpp` |
| P3 | Add Zephyr QEMU build to CI | Two recent fix commits modified Zephyr-only code; CI does not build it. N-18. | 4 hours | `.github/workflows/build.yml`, Zephyr SDK Docker setup |
| P3 | Pre-validate OTA size against slot1 size | Currently fails late inside `stream_flash_buffered_write`. N-13. | 15 min | `src/application/ota_session_manager_zephyr.cpp:104` |
| P3 | Reject `kUnknown` in `IsValidLampCommand` for function and action fields | All-zero frame currently passes validation. N-09. | 15 min | `src/domain/domain_type_validators.cpp:55–67` |
| P3 | Move validator invocation into `CommandArbitrator::OnLampCommandReceived` | Cross-platform consistency. N-10. | 1 hour | `src/application/command_arbitrator.cpp` |
| P3 | Add TSan job to CI | UdsRequestHandler / function_manager_ race is documented but unverified. | 1 hour | `.github/workflows/build.yml` |
| P3 | Add fuzzing target for codecs | `-fsanitize=fuzzer` on `DecodeOffer`, `ParseLampCommand`, `LightingPayloadCodec::DecodeFromBytes`, and the inline DoIP frame parser. N-16. | 4 hours | new `test/fuzz/` tree |
| P3 | Add coverage measurement to CI | `gcovr --xml-pretty` artifact on linux-build. N-17. | 1 hour | `.github/workflows/build.yml` |
| P3 | Replace `reinterpret_cast` in `EncodeEcuIdentification` with `std::memcpy` | Cosmetic, but the only non-platform `reinterpret_cast` left. N-08. | 15 min | `src/application/uds_request_handler.cpp:389–391` |
| P3 | Extend SD session-id counter to 32-bit internal | Avoid 36-hour wrap to zero. N-04. | 30 min | `app/zephyr_nucleo_h753zi/main.cpp:135` |
| P3 | Bound `HandleReadDtcInformation` loop to `kMaxActiveFaults` | Defense-in-depth; current loop trusts FaultManager bookkeeping. N-11. | 15 min | `src/application/uds_request_handler.cpp:296` |
| P3 | Document MCUboot key rotation procedure in security_architecture.md | Production-deployment story currently silent. N-15. | 1 hour | `doc/security_architecture.md` |
| P3 | Test `EncodeNodeHealth` with OTA mode active | Carried from first review; still uncovered. | 30 min | `test/unit/test_uds_request_handler.cpp` |
| P3 | Replace `static_cast<std::size_t>(-1)` sentinel with `std::optional` | Carried from first review; still open. | 45 min | `src/application/fault_manager.cpp` |
| P3 | Unify vsomeip cache `path` across CI jobs | Carried from first review; still open. | 15 min | `.github/workflows/build.yml` |
| P4 | Close SD socket on shutdown | Hygiene, not correctness. N-05. | 15 min | `app/zephyr_nucleo_h753zi/main.cpp:134` |
| P4 | Decompose Zephyr `main.cpp` | Carried from first review. | 4 hours | `app/zephyr_nucleo_h753zi/main.cpp` |
| P4 | Document that signature verification is at boot | Carried from first review. | 15 min | `doc/security_architecture.md` |
| P4 | Add `safety/changelog.md` | Required for ASIL B; not for portfolio. | 30 min | new `safety/changelog.md` |
| P4 | Stop setting `kFlagsRebooting` unconditionally on every offer | Spec says reboot flag should toggle per session-id epoch; listener-side semantics unclear. | 1 hour | `src/transport/some_ip_sd/some_ip_sd_codec.cpp:118` |

**Must-fix before next portfolio iteration (P2 items).** Six items totaling
roughly 6 hours of work. The OTA session leak and the DoIP routing-activation
gap are the two most likely to surface in a senior-interviewer probe of the
form "walk me through what happens when X goes wrong."

**Nice-to-have hardening (P3).** Fourteen items. The Zephyr QEMU CI and the
fuzzing target are the highest-leverage. The validator centralisation and
the per-platform parity items reflect architectural judgement that an
interviewer will probe.

**Defer (P4).** Five items. Most are doc / decomposition; none change
demonstrable capability.

---

## 18  Appendix

### A. Commit-by-commit map of fix coverage

| Commit | Date | Type | Closed | Opened | Net |
|---|---|---|---|---|---|
| f9218c8 | 2026-05-06 | fix | P1 #1 | N-07 (already-foreseen) | 0 P1, +1 P2 |
| 0a9ff73 | 2026-05-06 | docs | P1 #2 | none | -1 P1 |
| b2e7b6a | 2026-05-06 | docs | P1 #3 | none | -1 P1 |
| c6c31cd | 2026-05-06 | docs | P1 #4 | none | -1 P1 |
| 157ccb7 | 2026-05-06 | docs | P1 #5 | none | -1 P1 |
| c1a22f5 | 2026-05-06 | docs | P0 framing | none | reframe |
| 63e5b9c | 2026-05-06 | fix | P2 (parser) | N-09, N-10 | 0 P2, +2 P3 |
| 9e38dc0 | 2026-05-06 | fix | P2 (SD align) | N-03 (TTL, valid flag) | 0 P2, +1 P2 |
| 0d1f442 | 2026-05-06 | test | P2 evidence | none | 0 |
| ed8367e | 2026-05-06 | fix | P2 (CRC) | N-06 | 0 P2, +1 P2 |
| 8260c58 | 2026-05-06 | docs | tone | none | 0 |
| 45575a2 | 2026-05-06 | ci | P3 (ASan) | N-16, N-17, P3 TSan | 0 P3, +3 P3 |

**Read this table as:** the fix-and-tighten cycle is working as designed —
each fix commit closed its named scope and exposed neighbouring issues that
the next iteration can address. The "opened" column is not new bugs caused
by the fix; it is bugs that became visible because the fix narrowed the
focus of the surrounding code.

### B. Files re-read in this pass

Roughly 25 files, including:

- `src/application/ota_session_manager.cpp` (Linux)
- `src/application/ota_session_manager_zephyr.cpp` (Zephyr)
- `include/body_control/lighting/application/ota_session_manager.hpp`
- `src/transport/some_ip_sd/some_ip_sd_codec.cpp`
- `include/body_control/lighting/transport/some_ip_sd_types.hpp`
- `src/transport/doip_server.cpp`
- `src/application/uds_request_handler.cpp`
- `include/body_control/lighting/application/uds_request_handler.hpp`
- `src/domain/domain_type_validators.cpp`
- `app/zephyr_nucleo_h753zi/main.cpp` (selected ranges)
- `app/zephyr_nucleo_h753zi/prj.conf`
- `app/zephyr_nucleo_h753zi/boards/nucleo_h753zi.overlay`
- `.github/workflows/build.yml`
- `test/unit/test_ota_handler.cpp`
- `test/unit/test_some_ip_sd_codec.cpp`
- `doc/PROJECT_REVIEW.md`
- `safety/05_fmea.md`
- `safety/06_safety_mechanism_implementation_map.md`
- `doc/vss_integration_design.md`

`git show --stat` was run for each of the 12 fix commits between d812918 and
HEAD (45575a2).

### C. Honest limits of this pass

- I did not re-read `app/stm32_nucleo_h753zi/main.cpp` (LwIP side). The
  first review's findings on that file stand; I do not have new findings.
- I did not re-execute the unit tests. CTest output was inferred from
  commit messages claiming "16/16 pass."
- I did not re-run cppcheck or clang-tidy locally.
- I did not open the `doc/captures/` Wireshark files.
- I did not measure code coverage. The "no coverage" finding (N-17) is
  about the absence of tooling, not a measured number.
- I did not connect to a NUCLEO and verify the hardware-level claims in
  the Status block. I am trusting the captured payload bytes there.

A third pass should address at least the LwIP file, the unit-test
re-execution, and the capture verification.

---

*End of second-pass review.*
