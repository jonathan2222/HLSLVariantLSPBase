// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
import * as path from 'path';
import * as vscode from 'vscode';

import {
	Executable,
	LanguageClient,
	LanguageClientOptions,
	ServerOptions,
	TransportKind
} from 'vscode-languageclient/node';

let client: LanguageClient;

// This method is called when your extension is activated
// Your extension is activated the very first time the command is executed
export function activate(context: vscode.ExtensionContext) {

	// Use the console to output diagnostic information (console.log) and errors (console.error)
	// This line of code will only be executed once when your extension is activated
	console.log('Avtivating HLSLV extension');

	// The command has been defined in the package.json file
	// Now provide the implementation of the command with registerCommand
	// The commandId parameter must match the command field in package.json
	const disposable = vscode.commands.registerCommand('hlslvariant.helloWorld', () => {
		// The code you place here will be executed every time your command is executed
		// Display a message box to the user
		vscode.window.showInformationMessage('Hello World from HLSLVariant!');
	});

	context.subscriptions.push(disposable);

	// ------------------- LSP -------------------

	var serverModule : string = "";

	const isDebug = process.env.VSCODE_DEBUG_MODE === "true" || process.env.VSCODE_INSPECTOR_OPTIONS !== undefined;

	if (isDebug) {
		serverModule = context.asAbsolutePath(
			path.join('./..', 'server', 'Build', 'bin', 'Debug-windows-x86_64-x64', 'HLSLVServer', 'HLSLVServer.exe')
		);
		console.log("Starting in Debug");
	} else {
		serverModule = context.asAbsolutePath(
			path.join('./', 'bin', 'HLSLVServer.exe')
		);
		console.log("Starting in Release");
	}

	console.log('Server: ' + serverModule);

	// If the extension is launched in debug mode then the debug server options are used
	// Otherwise the run options are used
	const serverExe: Executable = {
		command: serverModule,
		transport: TransportKind.stdio,
		args: [],
		options: {shell: true, detached: false }
	};
	const serverOptions: ServerOptions = serverExe;

	// Options to control the language client
	const clientOptions: LanguageClientOptions = {
		// Register the server for plain text documents
		documentSelector: [{ scheme: 'file', language: 'hlslv' }],
		synchronize: {
			// Notify the server about file changes to '.clientrc files contained in the workspace
			fileEvents: vscode.workspace.createFileSystemWatcher('**/*.hlslv')
		}
	};

	// Create the language client and start the client.
	client = new LanguageClient(
		'hlslVariantServer',
		'HLSLVariant Server',
		serverOptions,
		clientOptions
	);

	// Start the client. This will also launch the server
	client.start();
}

// This method is called when your extension is deactivated
export function deactivate() {
	if (!client) {
		return undefined;
	}
	return client.stop();
}
