-- 新模式以配置文件为 include 基准，根目录作为 Lua 值传入，不拼接路径源码。
local config_name, roots = ...
local result = {}
local current_path = ""
local function getenv(name)
	return assert(os.getenv(name), "os.getenv() failed: " .. name)
end
local function include(filename)
	filename = filename:gsub("\\", "/")
	local previous = current_path
	local path, name = filename:match("(.*[/])(.*)$")
	if path then
		if path:sub(1, 1) == "/" or path:match("^%a:/") then
			current_path = path
		else
			current_path = current_path .. path
		end
	else
		name = filename
	end
	local file = assert(io.open(current_path .. name))
	local code = file:read("*a")
	file:close()
	assert(code, "无法读取配置：" .. filename)
	code = code:gsub("%$([%w_%d]+)", getenv)
	assert(load(code, "@" .. filename, "t", result))()
	current_path = previous
end
setmetatable(result, {
	__index = function(_, key)
		if key == "include" then return include end
		return roots[key]
	end,
	__newindex = function(table, key, value)
		assert(roots[key] == nil, "禁止覆盖保留路径变量：" .. key)
		rawset(table, key, value)
	end,
})
include(config_name)
setmetatable(result, nil)
return result
