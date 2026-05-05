-- bcz_someip.lua — Wireshark Lua dissector for the Body Control Zonal
-- Lighting (BCZ) custom SOME/IP-shaped UDP transport.
--
-- Wire format (20-byte header, big-endian):
--   bytes  0-1   version          uint16  always 0x0001
--   bytes  2-3   message_kind     uint16  1=REQUEST 2=RESPONSE 3=EVENT
--   bytes  4-5   service_id       uint16  0x5100=ExteriorLighting 0x5200=Operator
--   bytes  6-7   instance_id      uint16  always 0x0001
--   bytes  8-9   method_or_event_id uint16
--   bytes 10-11  client_id        uint16
--   bytes 12-13  session_id       uint16
--   bytes 14-15  flags            uint16  bit0=is_event bit1=is_reliable
--   bytes 16-19  payload_length   uint32
--   bytes 20+    payload          (payload_length bytes)
--
-- UDP ports decoded: 41000 41001 41002 41003

local bcz = Proto("bcz_someip", "BCZ SOME/IP Transport")

-- ── Header fields ──────────────────────────────────────────────────────────

local f_version    = ProtoField.uint16("bcz_someip.version",    "Version",           base.HEX)
local f_msg_kind   = ProtoField.uint16("bcz_someip.msg_kind",   "Message Kind",      base.DEC)
local f_svc_id     = ProtoField.uint16("bcz_someip.service_id", "Service ID",        base.HEX)
local f_inst_id    = ProtoField.uint16("bcz_someip.instance_id","Instance ID",       base.HEX)
local f_method_id  = ProtoField.uint16("bcz_someip.method_id",  "Method/Event ID",   base.HEX)
local f_client_id  = ProtoField.uint16("bcz_someip.client_id",  "Client ID",         base.HEX)
local f_session_id = ProtoField.uint16("bcz_someip.session_id", "Session ID",        base.HEX)
local f_flags      = ProtoField.uint16("bcz_someip.flags",      "Flags",             base.HEX)
local f_flag_event = ProtoField.bool("bcz_someip.flags.is_event",   "Is Event",    16, nil, 0x0001)
local f_flag_rel   = ProtoField.bool("bcz_someip.flags.is_reliable","Is Reliable", 16, nil, 0x0002)
local f_pay_len    = ProtoField.uint32("bcz_someip.payload_length","Payload Length", base.DEC)

-- ── Payload fields ─────────────────────────────────────────────────────────

local f_lamp_func      = ProtoField.uint8("bcz_someip.lamp_function",    "Lamp Function",       base.DEC)
local f_cmd_action     = ProtoField.uint8("bcz_someip.command_action",   "Command Action",      base.DEC)
local f_cmd_source     = ProtoField.uint8("bcz_someip.command_source",   "Command Source",      base.DEC)
local f_seq_ctr        = ProtoField.uint16("bcz_someip.sequence_counter","Sequence Counter",    base.DEC)
local f_output_state   = ProtoField.uint8("bcz_someip.output_state",     "Output State",        base.DEC)
local f_cmd_applied    = ProtoField.uint8("bcz_someip.command_applied",  "Command Applied",     base.DEC)
local f_last_seq       = ProtoField.uint16("bcz_someip.last_seq_counter","Last Sequence Counter",base.DEC)
local f_health_state   = ProtoField.uint8("bcz_someip.health_state",     "Health State",        base.DEC)
local f_eth_link       = ProtoField.uint8("bcz_someip.eth_link",         "Ethernet Link Available", base.DEC)
local f_svc_avail      = ProtoField.uint8("bcz_someip.svc_available",    "Service Available",   base.DEC)
local f_fault_present_flag = ProtoField.uint8("bcz_someip.driver_fault","Lamp Driver Fault",    base.DEC)
local f_active_faults  = ProtoField.uint16("bcz_someip.active_fault_count","Active Fault Count",base.DEC)
local f_fault_present  = ProtoField.uint8("bcz_someip.fault_present",   "Fault Present",        base.DEC)
local f_fault_code     = ProtoField.uint16("bcz_someip.fault_code",      "Fault Code",           base.HEX)
local f_raw_payload    = ProtoField.bytes("bcz_someip.payload",          "Payload")

bcz.fields = {
    f_version, f_msg_kind, f_svc_id, f_inst_id, f_method_id,
    f_client_id, f_session_id, f_flags, f_flag_event, f_flag_rel, f_pay_len,
    f_lamp_func, f_cmd_action, f_cmd_source, f_seq_ctr,
    f_output_state, f_cmd_applied, f_last_seq,
    f_health_state, f_eth_link, f_svc_avail, f_fault_present_flag, f_active_faults,
    f_fault_present, f_fault_code, f_raw_payload,
}

-- ── Enum name tables ───────────────────────────────────────────────────────

local MSG_KIND = { [1]="REQUEST", [2]="RESPONSE", [3]="EVENT" }

local SERVICE_NAME = {
    [0x5100] = "ExteriorLightingService",
    [0x5200] = "OperatorService",
}

-- ExteriorLightingService (0x5100) method/event IDs
local ELS_METHOD = {
    [0x0001] = "SetLampCommand",
    [0x0002] = "GetLampStatus",
    [0x0003] = "GetNodeHealth",
    [0x0004] = "InjectLampFault",
    [0x0005] = "ClearLampFault",
    [0x0006] = "GetFaultStatus",
    [0x8001] = "LampStatusEvent",
    [0x8002] = "NodeHealthEvent",
    [0x8003] = "FaultStatusEvent",
}

-- OperatorService (0x5200) method/event IDs
local OPS_METHOD = {
    [0x0001] = "RequestLampToggle",
    [0x0002] = "RequestLampActivate",
    [0x0003] = "RequestLampDeactivate",
    [0x0004] = "RequestNodeHealth",
    [0x0005] = "RequestInjectFault",
    [0x0006] = "RequestClearFault",
    [0x0007] = "RequestGetFaultStatus",
    [0x8001] = "LampStatusEvent",
    [0x8002] = "NodeHealthEvent",
}

local LAMP_FUNCTION = {
    [0] = "Unknown",
    [1] = "LeftIndicator",
    [2] = "RightIndicator",
    [3] = "HazardLamp",
    [4] = "ParkLamp",
    [5] = "HeadLamp",
}

local CMD_ACTION = {
    [0] = "Unknown",
    [1] = "Activate",
    [2] = "Deactivate",
    [3] = "Toggle",
}

local CMD_SOURCE = { [0] = "Unknown" }

local OUTPUT_STATE = {
    [0] = "Unknown",
    [1] = "Off",
    [2] = "On",
}

local HEALTH_STATE = {
    [0] = "Unknown",
    [1] = "Operational",
    [2] = "Degraded",
}

local BOOL_VAL = { [0] = "false", [1] = "true" }

local function lookup(tbl, key)
    return tbl[key] or string.format("0x%02X", key)
end

-- ── Payload decoders ───────────────────────────────────────────────────────

local function decode_lamp_command(tree, tvb, offset)
    local func_val = tvb(offset,   1):uint()
    local act_val  = tvb(offset+1, 1):uint()
    local src_val  = tvb(offset+2, 1):uint()
    local seq_val  = tvb(offset+3, 2):uint()
    tree:add(f_lamp_func,  tvb(offset,   1)):append_text(" (" .. lookup(LAMP_FUNCTION, func_val) .. ")")
    tree:add(f_cmd_action, tvb(offset+1, 1)):append_text(" (" .. lookup(CMD_ACTION,    act_val)  .. ")")
    tree:add(f_cmd_source, tvb(offset+2, 1)):append_text(" (" .. lookup(CMD_SOURCE,    src_val)  .. ")")
    tree:add(f_seq_ctr,    tvb(offset+3, 2)):append_text(" (seq=" .. seq_val .. ")")
end

local function decode_lamp_status(tree, tvb, offset)
    local func_val  = tvb(offset,   1):uint()
    local state_val = tvb(offset+1, 1):uint()
    local appl_val  = tvb(offset+2, 1):uint()
    local seq_val   = tvb(offset+3, 2):uint()
    tree:add(f_lamp_func,    tvb(offset,   1)):append_text(" (" .. lookup(LAMP_FUNCTION, func_val)  .. ")")
    tree:add(f_output_state, tvb(offset+1, 1)):append_text(" (" .. lookup(OUTPUT_STATE,  state_val) .. ")")
    tree:add(f_cmd_applied,  tvb(offset+2, 1)):append_text(" (" .. lookup(BOOL_VAL,      appl_val)  .. ")")
    tree:add(f_last_seq,     tvb(offset+3, 2)):append_text(" (seq=" .. seq_val .. ")")
end

local function decode_node_health(tree, tvb, offset)
    local hs_val   = tvb(offset,   1):uint()
    local eth_val  = tvb(offset+1, 1):uint()
    local svc_val  = tvb(offset+2, 1):uint()
    local flt_val  = tvb(offset+3, 1):uint()
    local cnt_val  = tvb(offset+4, 2):uint()
    tree:add(f_health_state,      tvb(offset,   1)):append_text(" (" .. lookup(HEALTH_STATE, hs_val)  .. ")")
    tree:add(f_eth_link,          tvb(offset+1, 1)):append_text(" (" .. lookup(BOOL_VAL,     eth_val) .. ")")
    tree:add(f_svc_avail,         tvb(offset+2, 1)):append_text(" (" .. lookup(BOOL_VAL,     svc_val) .. ")")
    tree:add(f_fault_present_flag,tvb(offset+3, 1)):append_text(" (" .. lookup(BOOL_VAL,     flt_val) .. ")")
    tree:add(f_active_faults,     tvb(offset+4, 2)):append_text(" faults")
end

local function decode_lamp_function_only(tree, tvb, offset)
    local func_val = tvb(offset, 1):uint()
    tree:add(f_lamp_func, tvb(offset, 1)):append_text(" (" .. lookup(LAMP_FUNCTION, func_val) .. ")")
end

local function decode_fault_status(tree, tvb, offset, pay_len)
    local present_val = tvb(offset,   1):uint()
    local count_val   = tvb(offset+1, 1):uint()
    tree:add(f_fault_present,  tvb(offset,   1)):append_text(" (" .. lookup(BOOL_VAL, present_val) .. ")")
    tree:add(f_active_faults,  tvb(offset+1, 1)):append_text(" faults")
    local codes_len = pay_len - 2
    local i = 0
    while (i + 2) <= codes_len do
        tree:add(f_fault_code, tvb(offset + 2 + i, 2))
        i = i + 2
    end
end

-- ── Main dissector ─────────────────────────────────────────────────────────

local HEADER_SIZE = 20

function bcz.dissector(tvb, pinfo, root)
    if tvb:len() < HEADER_SIZE then return 0 end

    local version = tvb(0, 2):uint()
    if version ~= 1 then return 0 end

    local msg_kind = tvb(2,  2):uint()
    local svc_id   = tvb(4,  2):uint()
    local inst_id  = tvb(6,  2):uint()
    local meth_id  = tvb(8,  2):uint()
    local pay_len  = tvb(16, 4):uint()

    if tvb:len() ~= (HEADER_SIZE + pay_len) then return 0 end

    pinfo.cols.protocol:set("BCZ-SOMEIP")

    local svc_name  = SERVICE_NAME[svc_id] or string.format("0x%04X", svc_id)
    local meth_tbl  = (svc_id == 0x5100) and ELS_METHOD or OPS_METHOD
    local meth_name = meth_tbl[meth_id] or string.format("0x%04X", meth_id)
    local kind_name = MSG_KIND[msg_kind] or tostring(msg_kind)

    pinfo.cols.info:set(
        string.format("%s  %s  [%s]  pay=%d",
            svc_name, meth_name, kind_name, pay_len))

    local tree = root:add(bcz, tvb(0))
    tree:set_text(string.format("BCZ SOME/IP — %s %s", svc_name, meth_name))

    -- Header subtree
    local hdr = tree:add(bcz, tvb(0, HEADER_SIZE), "Header")
    hdr:add(f_version,    tvb(0,  2))
    hdr:add(f_msg_kind,   tvb(2,  2)):append_text(" (" .. kind_name .. ")")
    hdr:add(f_svc_id,     tvb(4,  2)):append_text(" (" .. svc_name  .. ")")
    hdr:add(f_inst_id,    tvb(6,  2))
    hdr:add(f_method_id,  tvb(8,  2)):append_text(" (" .. meth_name .. ")")
    hdr:add(f_client_id,  tvb(10, 2))
    hdr:add(f_session_id, tvb(12, 2))
    local flags_item = hdr:add(f_flags, tvb(14, 2))
    flags_item:add(f_flag_event, tvb(14, 2))
    flags_item:add(f_flag_rel,   tvb(14, 2))
    hdr:add(f_pay_len,    tvb(16, 4))

    if pay_len == 0 then return tvb:len() end

    -- Payload subtree
    local pay_tvb = tvb(HEADER_SIZE, pay_len)
    local pay = tree:add(bcz, pay_tvb, string.format("Payload (%d bytes)", pay_len))

    -- ExteriorLightingService payloads
    if svc_id == 0x5100 then
        if meth_id == 0x0001 and pay_len >= 5 then
            decode_lamp_command(pay, tvb, HEADER_SIZE)
        elseif meth_id == 0x0002 and pay_len >= 1 then
            decode_lamp_function_only(pay, tvb, HEADER_SIZE)
        elseif (meth_id == 0x0004 or meth_id == 0x0005) and pay_len >= 5 then
            decode_lamp_command(pay, tvb, HEADER_SIZE)
        elseif meth_id == 0x8001 and pay_len >= 5 then
            decode_lamp_status(pay, tvb, HEADER_SIZE)
        elseif meth_id == 0x8002 and pay_len >= 6 then
            decode_node_health(pay, tvb, HEADER_SIZE)
        elseif meth_id == 0x8003 and pay_len >= 2 then
            decode_fault_status(pay, tvb, HEADER_SIZE, pay_len)
        else
            pay:add(f_raw_payload, pay_tvb)
        end
    -- OperatorService payloads
    elseif svc_id == 0x5200 then
        if (meth_id == 0x0001 or meth_id == 0x0002 or meth_id == 0x0003
            or meth_id == 0x0005 or meth_id == 0x0006) and pay_len >= 1 then
            decode_lamp_function_only(pay, tvb, HEADER_SIZE)
        elseif meth_id == 0x8001 and pay_len >= 5 then
            decode_lamp_status(pay, tvb, HEADER_SIZE)
        elseif meth_id == 0x8002 and pay_len >= 6 then
            decode_node_health(pay, tvb, HEADER_SIZE)
        else
            pay:add(f_raw_payload, pay_tvb)
        end
    else
        pay:add(f_raw_payload, pay_tvb)
    end

    return tvb:len()
end

-- ── Register on BCZ UDP ports ──────────────────────────────────────────────

local udp_table = DissectorTable.get("udp.port")
for _, port in ipairs({ 41000, 41001, 41002, 41003 }) do
    udp_table:add(port, bcz)
end
