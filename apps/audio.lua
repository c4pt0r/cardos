-- CardOS audio demo, in Lua.
-- Hold any key to record; release to stop. Shows a live input-level meter
-- and saves a 16 kHz mono WAV to /flash. Mirrors the built-in Recorder,
-- but as a hot-loadable script using the cardos.* audio bindings.
title = "Audio (Lua)"

local path = "/flash/lua-memo.wav"
local recording = false
local lvl = 0
local status = "Hold any key to record"

function on_key(code, ch, action)
  if code == "esc" then return false end  -- Esc exits; on_exit stops the mic
  if action == "press" and not recording then
    if cardos.record_start(path) then
      recording = true
      status = "recording..."
    else
      status = "mic start failed"
    end
    cardos.redraw()
    return true
  end
  if action == "release" and recording then
    cardos.record_stop()
    recording = false
    status = "saved " .. path
    cardos.redraw()
    return true
  end
  return true
end

function on_update(dt)
  if recording then
    lvl = cardos.level()
    cardos.redraw()  -- animate the meter
  end
end

function on_exit()
  if recording then
    cardos.record_stop()
    recording = false
  end
end

function on_render()
  local w = cardos.width()
  if recording then
    cardos.text(8, 22, "REC - release to stop", cardos.RED)
    cardos.rect(8, 48, w - 16, 14, cardos.GRAY)
    local fill = math.floor((w - 18) * lvl)
    if fill > 0 then cardos.fillrect(9, 49, fill, 12, cardos.GREEN) end
    cardos.text(8, 74, "level " .. math.floor(lvl * 100) .. "%", cardos.GRAY)
  else
    cardos.text(8, 22, "Hold any key to record", cardos.WHITE)
    cardos.text(8, 48, status, cardos.GRAY)
    cardos.text(8, 92, "Esc: exit", cardos.GRAY)
  end
end
