# Setup

This was developed on a windows machine, thus focused on the windows platform.

Below are instructions on how to setup the project but also what steps was made from the start of the project. Hope this can help the one reading this to get started without going through the pain I did :D 

## Setup VSCode Extension from scratch

1. Update node to latest and npm to latest
2. Inside hlslvarian Run `npm install --global yo generator-code` to install **Yeoman** and the **VS Code Extension Generator**
3. Run `yo code`, this will start initialization of a new vs code extension setup.
4. It will display these:
    ```
    # ? What type of extension do you want to create? New Extension (TypeScript)
    # ? What's the name of your extension? HelloWorld
    ### Press <Enter> to choose default for all options below ###

    # ? What's the identifier of your extension? helloworld
    # ? What's the description of your extension? LEAVE BLANK
    # ? Initialize a git repository? Y
    # ? Which bundler to use? unbundled
    # ? Which package manager to use? npm

    # ? Do you want to open the new folder with Visual Studio Code? Open with `code`
    ```

5. Add a language id inside `"languages"` (which you also need to add inside `"contributes"` of the file `package.json`).
6. ~~Change .vscode launch.json to have `/hlslvariant` after `${workspaceFolder}`. This will enable you to press F5 when in the root folder (meaning the parent folder of hlslvariant)~~
7. Add the below code inside `package.json`
    ```json
    "dependencies": {
        "vscode-languageclient": "9.0.1"
    },
    ```
    Run `npm install` inside the hlslvariant folder, this will install the necessary files for the vscode-languageclient libaray.
8. Add client side code in `extension.ts` as the lsp-sample in `https://code.visualstudio.com/api/language-extensions/language-server-extension-guide` but change `serverOptions` to:
    ```typescript
        const serverExe: Executable = {
            command: serverModule,
            transport: TransportKind.stdio,
            args: [],
            options: {shell: true, detached: false }
        };
        const serverOptions: ServerOptions = serverExe;
    ```
    This will start the server exe as long as the `serverModule` is the path to that exe. Note: The server exe is using stdin/stdout to communicate with the extension, thus cannot use it to log messages from the server!

## Setup LSP Server

### First setup of the LSP library

1. If not fetched, run this inside `externals`: `git clone https://github.com/leon-bckl/lsp-framework.git` (From: `https://github.com/leon-bckl/lsp-framework`)
2. Run `server/GenerateExternalsLSPVS2022Solution.bat` (Need to have cmake installed and a compiler that supports c++20)
3. Start `server/externals/slp-framework/build/slp.sln` and compile the `lsp` project in `Debug` and `Release`
    or run `server/CompileLSPLibrary.bat` (need to have msbuild in your systems path)
4. Run `server/CopyLSPIncludes.bat` to move the includes to the right folder.

### First setup of the tree-sitter library

1. If not fetched, run this inside `externals`: `git clone https://github.com/tree-sitter/tree-sitter.git` (From: `https://github.com/tree-sitter/tree-sitter/tree/master`)
2. Run `server/GenerateExternalsTreeSitterVS2022Solution.bat` (Need to have cmake installed and a compiler that supports c++20)
3. Start `server/externals/tree-sitter/lib/build/slp.sln` and compile the `tree-sitter` project in `Debug` and `Release`
    or run `server/CompileTreeSitterLibrary.bat` (need to have msbuild in your systems path)

### First setup of the tree-sitter-cpp parser

1. If not fetched, run this inside `externals`: `git clone https://github.com/tree-sitter/tree-sitter-cpp.git` (From: `https://github.com/tree-sitter/tree-sitter-cpp`)
2. Run `MoveTreeSitterCPPToSrc.bat`

### Generate LSP Server solution

1. Run `server/PremakeVS2022.bat` (Need to have VS 2022 installed) Note: All the previous setups must be done before doing this step!
2. Open `server/HLSLVariantLSPServer.sln` and compile the `HLSLVServer` project in `Debug` and `Release`.

## Debug extension

1. Open the folder `hlslvariant` in VS Code.
2. Press **F5**

## Debug server

### Visual Studio

1. Compile the `HLSLVServer` project in `Debug`.
2. Open the folder `hlslvariant` in VS Code.
3. Press **F5**
4. Open `server/HLSLVariantLSPServer.sln` and press Debug -> Attach to Process..
    Search for **HLSLVServer** and attach.

### Logs

1. Use lsp::notifications::Window_LogMessage to send messages to the client.
2. Can be seen in the output with the same name as the one given to `LanguageClient` in `extension.ts`
