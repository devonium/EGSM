timer.Simple(0, function()
	http.Fetch("https://raw.githubusercontent.com/devonium/EGSM/master/source/version.h", function(body)
		local version = tonumber(body:match("#[^%d]*([%d]*)"))
		if version == nil then return end
		if version > EGSM.Version
		then
			Derma_Query(
				"Your EGSM version is outdated.\nYou can still play, but some things may not work the right way.",
				"",
				"Yes"
			)
		end
	end)
end)