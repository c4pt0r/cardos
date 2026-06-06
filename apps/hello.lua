-- CardOS Lua app: a counter that draws and reacts to keys.
title = "Hello Lua"

local count = 0

function on_key(code, ch, action)
  if action ~= "press" then return false end
  if code == "enter" then
    count = count + 1
    cardos.redraw()
    return true
  end
  return false  -- unhandled Esc falls through -> back to launcher
end

function on_render()
  cardos.text(8, 22, "Hello from Lua!", cardos.CYAN)
  cardos.text(8, 46, "Enter pressed " .. count .. " times")
  cardos.fillrect(8, 68, count * 10, 12, cardos.GREEN)
  cardos.text(8, 92, "Enter = count, Esc = exit", cardos.GRAY)
end
