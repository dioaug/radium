---@meta radium.utils

---@class radium.utils
local utils = {}

---Runs a shell command with `/bin/sh -c`
---
---```lua
---utils.sh("firefox")
---```
---@param cmd string
---@return fun()
function utils.sh(cmd) end

return utils
