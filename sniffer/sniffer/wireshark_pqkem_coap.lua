
-- pqkem_coap.lua
-- Simple Wireshark post-dissector for PQ KEM over CoAP on UDP/5683
-- It doesn't replace the built-in CoAP dissector; it just adds a subtree
-- that understands the small KEM/DATA payload structs.

local p_pqkem = Proto("pqkem", "Post-Quantum KEM over CoAP")

local f_role      = ProtoField.string("pqkem.role", "Role")
local f_type      = ProtoField.uint8("pqkem.type", "MsgType", base.DEC)
local f_alg       = ProtoField.uint8("pqkem.alg", "Alg", base.DEC)
local f_ct_len    = ProtoField.uint16("pqkem.ct_len", "Ciphertext Length", base.DEC)
local f_nonce     = ProtoField.bytes("pqkem.nonce", "Nonce")
local f_cipher    = ProtoField.bytes("pqkem.cipher", "Ciphertext")
local f_tag       = ProtoField.bytes("pqkem.tag", "Tag")

p_pqkem.fields = { f_role, f_type, f_alg, f_ct_len, f_nonce, f_cipher, f_tag }

local MSG_KEM_REQ  = 1
local MSG_KEM_RESP = 2
local MSG_DATA     = 3

function p_pqkem.dissector(tvb, pinfo, tree)
    -- Only look at CoAP/UDP 5683
    if pinfo.dst_port ~= 5683 and pinfo.src_port ~= 5683 then
        return
    end

    if tvb:len() < 8 then
        return
    end

    -- Very minimal CoAP parsing: skip header + token + options until payload marker 0xFF
    local b0 = tvb(0,1):uint()
    local tkl = bit.band(b0, 0x0F)
    local offset = 4 + tkl
    if offset >= tvb:len() then return end

    -- Skip options until we see 0xFF
    while offset < tvb:len() do
        local b = tvb(offset,1):uint()
        if b == 0xFF then
            offset = offset + 1
            break
        end

        local delta = bit.rshift(b, 4)
        local length = bit.band(b, 0x0F)
        offset = offset + 1

        if delta == 13 then
            if offset >= tvb:len() then return end
            offset = offset + 1
        elseif delta == 14 then
            if offset + 1 >= tvb:len() then return end
            offset = offset + 2
        elseif delta == 15 then
            return
        end

        if length == 13 then
            if offset >= tvb:len() then return end
            offset = offset + 1
        elseif length == 14 then
            if offset + 1 >= tvb:len() then return end
            offset = offset + 2
        elseif length == 15 then
            return
        end

        offset = offset + length
    end

    if offset >= tvb:len() then
        return
    end

    local payload_len = tvb:len() - offset
    if payload_len < 4 then
        return
    end

    local subtree = tree:add(p_pqkem, tvb(offset, payload_len), "PQKEM over CoAP")

    local msg_type = tvb(offset,1):uint()
    subtree:add(f_type, tvb(offset,1))
    offset = offset + 1

    if msg_type == MSG_KEM_REQ then
        subtree:add(f_role, "Sender->Gateway (KEM_REQ)")
        if payload_len < 4 then return end
        subtree:add(f_alg, tvb(offset,1))
        local alg = tvb(offset,1):uint()
        offset = offset + 1
        subtree:add(f_ct_len, tvb(offset,2))
        local ct_len = tvb(offset,2):uint()
        offset = offset + 2

        local remaining = tvb:len() - offset
        if ct_len > 0 and ct_len <= remaining then
            subtree:add(f_cipher, tvb(offset, ct_len))
        end

    elseif msg_type == MSG_DATA then
        subtree:add(f_role, "Sender->Gateway (DATA)")
        offset = offset + 1 -- reserved
        subtree:add(f_ct_len, tvb(offset,2))
        local ct_len = tvb(offset,2):uint()
        offset = offset + 2

        if payload_len < 4 + 12 then return end
        subtree:add(f_nonce, tvb(offset,12))
        offset = offset + 12

        local remaining = tvb:len() - offset
        if ct_len > 0 and ct_len <= remaining then
            subtree:add(f_cipher, tvb(offset, ct_len))
            if remaining >= ct_len + 16 then
                subtree:add(f_tag, tvb(offset + ct_len, 16))
            end
        end
    else
        subtree:add(f_role, "Unknown/Other")
    end
end

register_postdissector(p_pqkem)
