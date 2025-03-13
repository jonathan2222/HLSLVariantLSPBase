# Template for a HLSL variant language VSCode extension

This is a template for creating a VSCode extension for a variant of HLSL. It is using the Language Server Protocol (LSP) to communicate with vscode client and the server.
Tree sitter is used for creating the AST of each file.

This was developed on a windows machine, thus focused on the windows platform.

Below are instructions on how to setup the project but also what steps was made from the start of the project. Hope this can help you who reads this to get started without going through the pain I did :D 

**Dependencies:** `npm`, `cmake`, `msbuild`, `Visual Studio 2022`
**Platform:** `windows`

* **Visual Studio 2022** is only needed for **PremakeVS2022.bat**, if you have another version, just change it inside that bat file.

## Build and compile server

1. Go to `server/`
2. Run `FetchExternals.bat`
3. Run `BuildAndCompileExternals.bat`
4. Run `PremakeVS2022.bat` (Need to have VS 2022 installed) *Note: All the previous setups must be done before doing this step!*
5. Open `HLSLVariantLSPServer.sln` and compile the `HLSLVServer` project in `Debug` and `Release`.

## Debug extension

1. Open the folder `hlslvariant` in VS Code. *Note: Server need to have been compile for this to work!*
2. Press **F5**

## Debug server

### Visual Studio

1. Compile the `HLSLVServer` project in `Debug`.
2. Open the folder `hlslvariant` in VS Code.
3. Press **F5**
4. Open `server/HLSLVariantLSPServer.sln` and press `Debug` -> `Attach to Process..`
    Search for `HLSLVServer` and attach.

### Logs

1. Use lsp::notifications::Window_LogMessage to send messages to the client.
2. Can be seen in the output with the same name as the one given to `LanguageClient` in `extension.ts`

## Package Extension

1. Go to `hlslsvariant/`.
2. Make sure you have `vsce` installed. If not run `npm install -g @vscode/vsce`.
3. Run `PackageExtension.bat` *Note: Remember to increase the `version` value in `package.json`*

## Creating a language using tree-sitter

1. Create the folder `server/language/tree-sitter-hlslv` and step into it.
2. Install tree-sitter by running `npm install -g tree-sitter-cli`. This will create the tree-sitter command globaly.
3. Run `tree-sitter init`. Fill out the fields after which it will have created the necessary files.
4. Run `tree-sitter generate --log`. This should generate the parser from the grammar in `grammar.js`.
5. Try the grammar by running these commands:
    `'hello' | Out-File example-file.hlslv -Encoding utf8`
    `tree-sitter example-file.hlslv`
    If you get an error that the the parser directories was not configured. Do this:
    Run `tree-sitter init-config` open the file that was generated and add the path to the directory `tree-sitter-hlslv` inside of `parser-directories`.

### Other issues that can be solved

#### Linter errors in grammar.js

Can be fixed by running `npm install` combined with `tree-sitter init` if you are missing some bindings etc.

#### If you see errors like *unresolved external symbol tree_sitter_hlslvparser_external_scanner_create HLSLVServer ..*

It is missing an external scanner. This is the file `src/scanner.c` in the `tree-sitter-cpp` repository.
Copy that file into our src and rename the functions to the correct names.

#### Inherit language grammar from CPP

Add the below code in `devDependencies` inside `tree-sitter-hlslv/package.json`

```json
    "tree-sitter-c": "git://github.com/tree-sitter/tree-sitter-c.git",
    "tree-sitter-cpp": "git://github.com/tree-sitter/tree-sitter-cpp.git"
```

Run `npm install` inside `tree-sitter-hlslv/`
Add grammar from `https://github.com/tree-sitter-grammars/tree-sitter-hlsl`
Run `tree-sitter generate --log`

## Setup VSCode Extension from scratch

<details>

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

</details>
