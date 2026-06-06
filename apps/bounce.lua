-- CardOS Lua app: an animated bouncing box (uses on_update for motion).
title = "Bounce"

local x, y = 20, 30
local dx, dy = 3, 2
local size = 12

function on_update(dt)
  x = x + dx
  y = y + dy
  if x < 0 or x > cardos.width() - size then dx = -dx end
  if y < 16 or y > cardos.height() - size then dy = -dy end
  cardos.redraw()
end

function on_render()
  cardos.text(6, 2, "Bounce - Esc exits", cardos.GRAY)
  cardos.fillrect(x, y, size, size, cardos.RED)
end
