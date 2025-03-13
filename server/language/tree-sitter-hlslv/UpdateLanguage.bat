call tree-sitter generate

:: Move tree-sitter parser to source
robocopy src\ .\..\..\src\tree_sitter_hlslv *.h /E
robocopy src\ .\..\..\src\tree_sitter_hlslv *.c /E
xcopy /E /I /Y bindings\c\tree_sitter\*.h .\..\..\src\tree_sitter_hlslv\

cd .\..\..\
call PremakeVS2022.bat
cd language\tree-sitter-hlslv