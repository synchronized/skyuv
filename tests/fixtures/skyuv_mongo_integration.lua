local skynet = require "skynet"
local mongo = require "skynet.db.mongo"

skynet.start(function()
	local client = mongo.client {
		host = "127.0.0.1",
		port = 27017,
	}
	local collection = client.skyuv_ci.documents
	collection:drop()

	local inserted, insert_error = collection:safe_insert {
		key = "skyuv",
		value = "跨平台 MongoDB",
		count = 1,
	}
	assert(inserted, insert_error)

	local document = assert(collection:findOne { key = "skyuv" })
	assert(document.value == "跨平台 MongoDB")
	assert(document.count == 1)

	local updated, update_error = collection:safe_update(
		{ key = "skyuv" },
		{ ["$set"] = { count = 2 } }
	)
	assert(updated, update_error)
	document = assert(collection:findOne { key = "skyuv" })
	assert(document.count == 2)

	local index = collection:createIndex({ key = 1 }, { unique = true, name = "skyuv_key" })
	assert(index.ok == 1)
	local deleted, delete_error = collection:safe_delete { key = "skyuv" }
	assert(deleted, delete_error)
	assert(collection:findOne { key = "skyuv" } == nil)

	collection:drop()
	client:disconnect()
	skynet.error("skyuv MongoDB CRUD 与索引验证通过")
	skynet.exit()
end)
