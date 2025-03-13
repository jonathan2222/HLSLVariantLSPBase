msbuild externals\lsp-framework\build\lsp.sln /t:lsp /p:Configuration=Debug
msbuild externals\lsp-framework\build\lsp.sln /t:lsp /p:Configuration=Release

:: Copy includes and generated includes into a separet lib/include folder that is easier to add to a project.
xcopy /E /I /Y externals\lsp-framework\build\generated\lsp\*.h externals\lsp-framework\build\lib\include\lsp
robocopy externals\lsp-framework\lsp externals\lsp-framework\build\lib\include\lsp *.h /E

msbuild externals\tree-sitter\lib\build\tree-sitter.sln /t:tree-sitter /p:Configuration=Debug
msbuild externals\tree-sitter\lib\build\tree-sitter.sln /t:tree-sitter /p:Configuration=Release

:: Move tree-sitter parser to source
robocopy externals\tree-sitter-cpp\src src\tree_sitter_cpp *.h /E
robocopy externals\tree-sitter-cpp\src src\tree_sitter_cpp *.c /E
xcopy /E /I /Y externals\tree-sitter-cpp\bindings\c\*.h src\tree_sitter_cpp\