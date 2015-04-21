-- Distributed voice service protocol plugin

local proto = Proto("dvs", "Distributed voice service")

function proto.dissector(buffer, info, tree)

    info.cols.protocol = "DVS"

    local function wrap(x, n) return x:len() < n and tostring(x) or tostring(x(0, n)) .. "..." end
    local subtree = tree:add(proto, buffer())
    local id = buffer(0, 1):uint()

    if id == 0 then
        local srcid = buffer(1, 20):bytes()
        local dstid = buffer(21, 20):bytes()

        info.cols.info = "Route request  " .. wrap(srcid, 8) .. " -> " .. wrap(dstid, 8)
        subtree:add(buffer(0, 1), "Message ID: Route request (1)")
        subtree:add(buffer(1, 20), "Source ID: " .. tostring(srcid))
        subtree:add(buffer(21, 20), "Destination ID: " .. tostring(dstid))

    elseif id == 1 then
        local srcid = buffer(1, 20):bytes()
        local dstid = buffer(21, 20):bytes()

        info.cols.info = "Route response " .. wrap(srcid, 8) .. " -> " .. wrap(dstid, 8)
        subtree:add(buffer(0, 1), "Message ID: Route response (1)")
        subtree:add(buffer(1, 20), "Source ID: " .. tostring(srcid))
        subtree:add(buffer(21, 20), "Destination ID: " .. tostring(dstid))

        for i = 41, buffer:len() - 1, 26 do
            local list = subtree:add(buffer(i, 26), "Route entry")
            list:add(buffer(i, 20), "ID: " .. tostring(buffer(i, 20):bytes()))
            list:add(buffer(i + 20, 4), "Address: " .. tostring(buffer(i + 20, 4):ipv4()))
            list:add(buffer(i + 24, 2), "Port: " .. buffer(i + 24, 2):uint())
        end

    elseif id == 2 then

        info.cols.info = "Key request"
        subtree:add(buffer(0, 1), "Message ID: Key request (2)")
        subtree:add(buffer(1), "X.509 SubjectPublicKeyInfo: " .. wrap(buffer(1):bytes(), 32))

    elseif id == 3 then

        info.cols.info = "Key response"
        subtree:add(buffer(0, 1), "Message ID: Key response (3)")
        subtree:add(buffer(1), "X.509 SubjectPublicKeyInfo: " .. wrap(buffer(1):bytes(), 32))

    elseif id == 4 then
        local seqnum = tostring(buffer(1, 4):uint())

        info.cols.info = "Payload seqnum=" .. seqnum
        subtree:add(buffer(0, 1), "Message ID: Payload (4)")
        subtree:add(buffer(1, 4), "Sequence number: " .. seqnum)
        subtree:add(buffer(5, 8), "Security tag: " .. tostring(buffer(5, 8):bytes()))
        subtree:add(buffer(13), "Ciphertext: " .. wrap(buffer(13):bytes(), 32))

    elseif id == 5 then

        info.cols.info = "Hangup"
        subtree:add(buffer(0, 1), "Message ID: Hangup (5)")

    end
end

local udp_table = DissectorTable.get("udp.port")
udp_table:add(5004, proto)
udp_table:add(5005, proto)
