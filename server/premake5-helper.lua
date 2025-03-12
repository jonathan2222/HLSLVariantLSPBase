function ExternalsIncludes()
	-- External files
    externalincludedirs
	{
		"%{includeDir.lsp}",
		"%{includeDir.treeSitter}"
	}
end

function ExternalsLinks()
    filter "configurations:Debug"
		links
		{
			"%{libDir.lsp}Debug/lsp.lib",
			"%{libDir.treeSitter}Debug/tree-sitter.lib"
		}

	filter "configurations:Release"
		links
		{
			"%{libDir.lsp}Release/lsp.lib",
			"%{libDir.treeSitter}Release/tree-sitter.lib"
		}
	filter {}
end

function GetFiles(path)
	return path .. "**.hpp", path .. "**.h", path .. "**.cpp", path .. "**.c"
end
