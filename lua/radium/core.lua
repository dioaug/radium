---@meta radium

---@class radium
local radium = {}

---Runs when Radium starts, in the order they were given. If an error
---occurs the others still runs.
---
---```lua
---radium.on_startup(utils.sh("picom"), utils.sh("nm-applet"))
---```
---@param ... fun()
function radium.on_startup(...) end

return radium
