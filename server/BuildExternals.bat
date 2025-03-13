:: Build lsp-framework
cd externals\lsp-framework
mkdir build
cd build
cmake ..
cd ..\..\..

:: Build tree-sitter
cd externals\tree-sitter\lib
mkdir build
cmake -DBUILD_SHARED_LIBS=OFF -B build
cd ..\..\..