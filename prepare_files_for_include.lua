for file in io.popen([[dir "include_files" /b /A-d]]):lines() 
do 
	local isLuaFile = file:match("%.lua") ~= nil
	print(file, isLuaFile)
	if not file:match("%.inc") 
	then
		local f = io.open("include_files/"..file, "r")
		local pr = "const char " .. file:gsub("%.", "_") .. "[] = "
		
		for line in f:read("*all"):gmatch("[^\n]*") do
			if isLuaFile
			then
				line = line:gsub("\\", "\\\\")
			end
			line = line:gsub("\"", "\\\"")
			
			pr = pr .. "\"" .. line .. "\\n\""
		end
		pr = pr .. ";"	
		local pf = io.open("source/"..file..".inc", "w")
		pf:write(pr)

	end

end

--if true then return end
--
--for file in io.popen([[dir "include_files" /b /A-d]]):lines() 
--do 
--	local isLua = file:match("%.lua") != nil
--	if not file:match("%.inc") 
--	then
--		print(file  .. " -> " .. file .. ".inc")
--		local f = io.open("include_files/"..file, "r")
--		local pr = "const char " .. file:gsub("%.", "_") .. "[] = "
--		local skipNewline = false
--		for line in f:read("*all"):gmatch("([^\n]*)") do
--			if skipNewline
--			then
--				skipNewline = false
--			else
--				if isLua
--				then
--					line = line:gsub("\\", "\\\\")
--					line = line:gsub("\"", "\\\"")
--				end
--				
--				pr = pr .. "\"" .. line .. "\\n\""
--				skipNewline = true
--			end
--		end
--		pr = pr .. ";"	
--		local pf = io.open("source/"..file..".inc", "w")
--		pf:write(pr)
--	end
--
--end