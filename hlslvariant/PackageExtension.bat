:: Compile the server project (the premake5.lua tells it to copy the exe to the client bin folder when compiling in release as we do now)
msbuild .\..\server\HLSLVariantLSPServer.sln /t:HLSLVServer /p:Configuration=Release

:: Package extension
mkdir packages
vsce package --out .\packages\