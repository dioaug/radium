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

---Loads font files so they can be used by name, without the need
---to have them installed. Relative paths start at the config file's folder, and a path
---ending in / loads every font in that folder and its subfolders.
---
---```lua
---radium.load_fonts("fonts/Inter-Regular.ttf", "fonts/Inter-Bold.ttf")
---radium.load_fonts("fonts/")
---```
---@param ... string
function radium.load_fonts(...) end

return radium
