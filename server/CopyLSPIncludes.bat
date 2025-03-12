:: Copy includes and generated includes into a separet lib/include folder that is easier to add to a project.
xcopy /E /I /Y externals\lsp-framework\build\generated\lsp\*.h externals\lsp-framework\build\lib\include\lsp
robocopy externals\lsp-framework\lsp externals\lsp-framework\build\lib\include\lsp *.h /E