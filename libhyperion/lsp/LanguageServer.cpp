/*
	This file is part of hyperion.

	hyperion is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	hyperion is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with hyperion.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
#include <libhyperion/ast/AST.h>
#include <libhyperion/ast/ASTUtils.h>
#include <libhyperion/ast/ASTVisitor.h>
#include <libhyperion/interface/ReadFile.h>
#include <libhyperion/interface/StandardCompiler.h>
#include <libhyperion/lsp/LanguageServer.h>
#include <libhyperion/lsp/HandlerBase.h>
#include <libhyperion/lsp/Utils.h>

// LSP feature implementations
#include <libhyperion/lsp/DocumentHoverHandler.h>
#include <libhyperion/lsp/GotoDefinition.h>
#include <libhyperion/lsp/RenameSymbol.h>
#include <libhyperion/lsp/SemanticTokensBuilder.h>

#include <liblangutil/SourceReferenceExtractor.h>
#include <liblangutil/CharStream.h>

#include <libhyputil/CommonIO.h>
#include <libhyputil/Visitor.h>
#include <libhyputil/JSON.h>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <ostream>
#include <string>

#include <fmt/format.h>

using namespace std::string_literals;
using namespace std::placeholders;

using namespace hyperion::lsp;
using namespace hyperion::langutil;
using namespace hyperion::frontend;

namespace fs = boost::filesystem;

namespace
{

bool resolvesToRegularFile(boost::filesystem::path _path, int maxRecursionDepth = 10)
{
	fs::file_status fileStatus = fs::status(_path);

	while (fileStatus.type() == fs::file_type::symlink_file && maxRecursionDepth > 0)
	{
		_path = boost::filesystem::read_symlink(_path);
		fileStatus = fs::status(_path);
		maxRecursionDepth--;
	}

	return fileStatus.type() == fs::file_type::regular_file;
}

int toDiagnosticSeverity(Error::Type _errorType)
{
	// 1=Error, 2=Warning, 3=Info, 4=Hint
	switch (Error::errorSeverity(_errorType))
	{
	case Error::Severity::Error: return 1;
	case Error::Severity::Warning: return 2;
	case Error::Severity::Info: return 3;
	}
	hypAssert(false);
	return -1;
}

Json::Value semanticTokensLegend()
{
	Json::Value legend = Json::objectValue;

	// NOTE! The (alphabetical) order and items must match exactly the items of
	//       their respective enum class members.

	Json::Value tokenTypes = Json::arrayValue;
	tokenTypes.append("class");
	tokenTypes.append("comment");
	tokenTypes.append("enum");
	tokenTypes.append("enumMember");
	tokenTypes.append("event");
	tokenTypes.append("function");
	tokenTypes.append("interface");
	tokenTypes.append("keyword");
	tokenTypes.append("macro");
	tokenTypes.append("method");
	tokenTypes.append("modifier");
	tokenTypes.append("number");
	tokenTypes.append("operator");
	tokenTypes.append("parameter");
	tokenTypes.append("property");
	tokenTypes.append("std::string");
	tokenTypes.append("struct");
	tokenTypes.append("type");
	tokenTypes.append("typeParameter");
	tokenTypes.append("variable");
	legend["tokenTypes"] = tokenTypes;

	Json::Value tokenModifiers = Json::arrayValue;
	tokenModifiers.append("abstract");
	tokenModifiers.append("declaration");
	tokenModifiers.append("definition");
	tokenModifiers.append("deprecated");
	tokenModifiers.append("documentation");
	tokenModifiers.append("modification");
	tokenModifiers.append("readonly");
	legend["tokenModifiers"] = tokenModifiers;

	return legend;
}

}

LanguageServer::LanguageServer(Transport& _transport):
	m_client{_transport},
	m_handlers{
		{"$/cancelRequest", [](auto, auto) {/*nothing for now as we are synchronous */}},
		{"cancelRequest", [](auto, auto) {/*nothing for now as we are synchronous */}},
		{"exit", [this](auto, auto) { m_state = (m_state == State::ShutdownRequested ? State::ExitRequested : State::ExitWithoutShutdown); }},
		{"initialize", std::bind(&LanguageServer::handleInitialize, this, _1, _2)},
		{"initialized", std::bind(&LanguageServer::handleInitialized, this, _1, _2)},
		{"$/setTrace", [this](auto, Json::Value const& args) { setTrace(args["value"]); }},
		{"shutdown", [this](auto, auto) { m_state = State::ShutdownRequested; }},
		{"textDocument/definition", GotoDefinition(*this) },
		{"textDocument/didOpen", std::bind(&LanguageServer::handleTextDocumentDidOpen, this, _2)},
		{"textDocument/didChange", std::bind(&LanguageServer::handleTextDocumentDidChange, this, _2)},
		{"textDocument/didClose", std::bind(&LanguageServer::handleTextDocumentDidClose, this, _2)},
		{"textDocument/hover", DocumentHoverHandler(*this) },
		{"textDocument/rename", RenameSymbol(*this) },
		{"textDocument/implementation", GotoDefinition(*this) },
		{"textDocument/semanticTokens/full", std::bind(&LanguageServer::semanticTokensFull, this, _1, _2)},
		{"workspace/didChangeConfiguration", std::bind(&LanguageServer::handleWorkspaceDidChangeConfiguration, this, _2)},
	},
	m_fileRepository("/" /* basePath */, {} /* no search paths */),
	m_compilerStack{m_fileRepository.reader()}
{
}

Json::Value LanguageServer::toRange(SourceLocation const& _location)
{
	return HandlerBase(*this).toRange(_location);
}

Json::Value LanguageServer::toJson(SourceLocation const& _location)
{
	return HandlerBase(*this).toJson(_location);
}

void LanguageServer::changeConfiguration(Json::Value const& _settings)
{
	// The settings item: "file-load-strategy" (enum) defaults to "project-directory" if not (or not correctly) set.
	// It can be overridden during client's handshake or at runtime, as usual.
	//
	// If this value is set to "project-directory" (default), all .hyp files located inside the project directory or reachable through symbolic links will be subject to operations.
	//
	// Operations include compiler analysis, but also finding all symbolic references or symbolic renaming.
	//
	// If this value is set to "directly-opened-and-on-import", then only currently directly opened files and
	// those files being imported directly or indirectly will be included in operations.
	if (_settings["file-load-strategy"])
	{
		auto const text = _settings["file-load-strategy"].asString();
		if (text == "project-directory")
			m_fileLoadStrategy = FileLoadStrategy::ProjectDirectory;
		else if (text == "directly-opened-and-on-import")
			m_fileLoadStrategy = FileLoadStrategy::DirectlyOpenedAndOnImported;
		else
			lspRequire(false, ErrorCode::InvalidParams, "Invalid file load strategy: " + text);
	}

	m_settingsObject = _settings;
	Json::Value jsonIncludePaths = _settings["include-paths"];

	if (jsonIncludePaths)
	{
		int typeFailureCount = 0;
		if (jsonIncludePaths.isArray())
		{
			std::vector<boost::filesystem::path> includePaths;
			for (Json::Value const& jsonPath: jsonIncludePaths)
			{
				if (jsonPath.isString())
					includePaths.emplace_back(boost::filesystem::path(jsonPath.asString()));
				else
					typeFailureCount++;
			}
			m_fileRepository.setIncludePaths(std::move(includePaths));
		}
		else
			++typeFailureCount;

		if (typeFailureCount)
			m_client.trace("Invalid JSON configuration passed. \"include-paths\" must be an array of strings.");
	}
}

std::vector<boost::filesystem::path> LanguageServer::allHyperionFilesFromProject() const
{
	std::vector<fs::path> collectedPaths{};

	// We explicitly decided against including all files from include paths but leave the possibility
	// open for a future PR to enable such a feature to be optionally enabled (default disabled).
	// Note: Newer versions of boost have deprecated symlink_option::recurse
#if (BOOST_VERSION < 107200)
	auto directoryIterator = fs::recursive_directory_iterator(m_fileRepository.basePath(), fs::symlink_option::recurse);
#else
	auto directoryIterator = fs::recursive_directory_iterator(m_fileRepository.basePath(), fs::directory_options::follow_directory_symlink);
#endif
	for (fs::directory_entry const& dirEntry: directoryIterator)
		if (
			dirEntry.path().extension() == ".hyp" &&
			(dirEntry.status().type() == fs::file_type::regular_file || resolvesToRegularFile(dirEntry.path()))
		)
			collectedPaths.push_back(dirEntry.path());

	return collectedPaths;
}

void LanguageServer::compile()
{
	// For files that are not open, we have to take changes on disk into account,
	// so we just remove all non-open files.

	FileRepository oldRepository(m_fileRepository.basePath(), m_fileRepository.includePaths());
	std::swap(oldRepository, m_fileRepository);

	// Load all hyperion files from project.
	if (m_fileLoadStrategy == FileLoadStrategy::ProjectDirectory)
		for (auto const& projectFile: allHyperionFilesFromProject())
		{
			lspDebug(fmt::format("adding project file: {}", projectFile.generic_string()));
			m_fileRepository.setSourceByUri(
				m_fileRepository.sourceUnitNameToUri(projectFile.generic_string()),
				util::readFileAsString(projectFile)
			);
		}

	// Overwrite all files as opened by the client, including the ones which might potentially have changes.
	for (std::string const& fileName: m_openFiles)
		m_fileRepository.setSourceByUri(
			fileName,
			oldRepository.sourceUnits().at(oldRepository.uriToSourceUnitName(fileName))
		);

	// TODO: optimize! do not recompile if nothing has changed (file(s) not flagged dirty).

	m_compilerStack.reset(false);
	m_compilerStack.setSources(m_fileRepository.sourceUnits());
	m_compilerStack.compile(CompilerStack::State::AnalysisSuccessful);
}

void LanguageServer::compileAndUpdateDiagnostics()
{
	compile();

	// These are the source units we will sent diagnostics to the client for sure,
	// even if it is just to clear previous diagnostics.
	std::map<std::string, Json::Value> diagnosticsBySourceUnit;
	for (std::string const& sourceUnitName: m_fileRepository.sourceUnits() | ranges::views::keys)
		diagnosticsBySourceUnit[sourceUnitName] = Json::arrayValue;
	for (std::string const& sourceUnitName: m_nonemptyDiagnostics)
		diagnosticsBySourceUnit[sourceUnitName] = Json::arrayValue;

	for (std::shared_ptr<Error const> const& error: m_compilerStack.errors())
	{
		SourceLocation const* location = error->sourceLocation();
		if (!location || !location->sourceName)
			// LSP only has diagnostics applied to individual files.
			continue;

		Json::Value jsonDiag;
		jsonDiag["source"] = "hypc";
		jsonDiag["severity"] = toDiagnosticSeverity(error->type());
		jsonDiag["code"] = Json::UInt64{error->errorId().error};
		std::string message = Error::formatErrorType(error->type()) + ":";
		if (std::string const* comment = error->comment())
			message += " " + *comment;
		jsonDiag["message"] = std::move(message);
		jsonDiag["range"] = toRange(*location);

		if (auto const* secondary = error->secondarySourceLocation())
			for (auto&& [secondaryMessage, secondaryLocation]: secondary->infos)
			{
				Json::Value jsonRelated;
				jsonRelated["message"] = secondaryMessage;
				jsonRelated["location"] = toJson(secondaryLocation);
				jsonDiag["relatedInformation"].append(jsonRelated);
			}

		diagnosticsBySourceUnit[*location->sourceName].append(jsonDiag);
	}

	if (m_client.traceValue() != TraceValue::Off)
	{
		Json::Value extra;
		extra["openFileCount"] = Json::UInt64(diagnosticsBySourceUnit.size());
		m_client.trace("Number of currently open files: " + std::to_string(diagnosticsBySourceUnit.size()), extra);
	}

	m_nonemptyDiagnostics.clear();
	for (auto&& [sourceUnitName, diagnostics]: diagnosticsBySourceUnit)
	{
		Json::Value params;
		params["uri"] = m_fileRepository.sourceUnitNameToUri(sourceUnitName);
		if (!diagnostics.empty())
			m_nonemptyDiagnostics.insert(sourceUnitName);
		params["diagnostics"] = std::move(diagnostics);
		m_client.notify("textDocument/publishDiagnostics", std::move(params));
	}
}

bool LanguageServer::run()
{
	while (m_state != State::ExitRequested && m_state != State::ExitWithoutShutdown && !m_client.closed())
	{
		MessageID id;
		try
		{
			std::optional<Json::Value> const jsonMessage = m_client.receive();
			if (!jsonMessage)
				continue;

			if ((*jsonMessage)["method"].isString())
			{
				std::string const methodName = (*jsonMessage)["method"].asString();
				id = (*jsonMessage)["id"];
				lspDebug(fmt::format("received method call: {}", methodName));

				if (auto handler = util::valueOrDefault(m_handlers, methodName))
					handler(id, (*jsonMessage)["params"]);
				else
					m_client.error(id, ErrorCode::MethodNotFound, "Unknown method " + methodName);
			}
			else
				m_client.error({}, ErrorCode::ParseError, "\"method\" has to be a string.");
		}
		catch (Json::Exception const&)
		{
			m_client.error(id, ErrorCode::InvalidParams, "JSON object access error. Most likely due to a badly formatted JSON request message."s);
		}
		catch (RequestError const& error)
		{
			m_client.error(id, error.code(), error.comment() ? *error.comment() : ""s);
		}
		catch (...)
		{
			m_client.error(id, ErrorCode::InternalError, "Unhandled exception: "s + boost::current_exception_diagnostic_information());
		}
	}
	return m_state == State::ExitRequested;
}

void LanguageServer::requireServerInitialized()
{
	lspRequire(
		m_state == State::Initialized,
		ErrorCode::ServerNotInitialized,
		"Server is not properly initialized."
	);
}

void LanguageServer::handleInitialize(MessageID _id, Json::Value const& _args)
{
	lspRequire(
		m_state == State::Started,
		ErrorCode::RequestFailed,
		"Initialize called at the wrong time."
	);

	m_state = State::Initialized;

	// The default of FileReader is to use `.`, but the path from where the LSP was started
	// should not matter.
	std::string rootPath("/");
	if (Json::Value uri = _args["rootUri"])
	{
		rootPath = uri.asString();
		lspRequire(
			boost::starts_with(rootPath, "file://"),
			ErrorCode::InvalidParams,
			"rootUri only supports file URI scheme."
		);
		rootPath = stripFileUriSchemePrefix(rootPath);
	}
	else if (Json::Value rootPath = _args["rootPath"])
		rootPath = rootPath.asString();

	if (_args["trace"])
		setTrace(_args["trace"]);

	m_fileRepository = FileRepository(rootPath, {});
	if (_args["initializationOptions"].isObject())
		changeConfiguration(_args["initializationOptions"]);

	Json::Value replyArgs;
	replyArgs["serverInfo"]["name"] = "hypc";
	replyArgs["serverInfo"]["version"] = std::string(VersionNumber);
	replyArgs["capabilities"]["definitionProvider"] = true;
	replyArgs["capabilities"]["implementationProvider"] = true;
	replyArgs["capabilities"]["textDocumentSync"]["change"] = 2; // 0=none, 1=full, 2=incremental
	replyArgs["capabilities"]["textDocumentSync"]["openClose"] = true;
	replyArgs["capabilities"]["semanticTokensProvider"]["legend"] = semanticTokensLegend();
	replyArgs["capabilities"]["semanticTokensProvider"]["range"] = false;
	replyArgs["capabilities"]["semanticTokensProvider"]["full"] = true; // XOR requests.full.delta = true
	replyArgs["capabilities"]["renameProvider"] = true;
	replyArgs["capabilities"]["hoverProvider"] = true;

	m_client.reply(_id, std::move(replyArgs));
}

void LanguageServer::handleInitialized(MessageID, Json::Value const&)
{
	if (m_fileLoadStrategy == FileLoadStrategy::ProjectDirectory)
		compileAndUpdateDiagnostics();
}

void LanguageServer::semanticTokensFull(MessageID _id, Json::Value const& _args)
{
	auto uri = _args["textDocument"]["uri"];

	compile();

	auto const sourceName = m_fileRepository.uriToSourceUnitName(uri.as<std::string>());
	SourceUnit const& ast = m_compilerStack.ast(sourceName);
	m_compilerStack.charStream(sourceName);
	Json::Value data = SemanticTokensBuilder().build(ast, m_compilerStack.charStream(sourceName));

	Json::Value reply = Json::objectValue;
	reply["data"] = data;

	m_client.reply(_id, std::move(reply));
}

void LanguageServer::handleWorkspaceDidChangeConfiguration(Json::Value const& _args)
{
	requireServerInitialized();

	if (_args["settings"].isObject())
		changeConfiguration(_args["settings"]);
}

void LanguageServer::setTrace(Json::Value const& _args)
{
	if (!_args.isString())
		// Simply ignore invalid parameter.
		return;

	std::string const stringValue = _args.asString();
	if (stringValue == "off")
		m_client.setTrace(TraceValue::Off);
	else if (stringValue == "messages")
		m_client.setTrace(TraceValue::Messages);
	else if (stringValue == "verbose")
		m_client.setTrace(TraceValue::Verbose);
}

void LanguageServer::handleTextDocumentDidOpen(Json::Value const& _args)
{
	requireServerInitialized();

	lspRequire(
		_args["textDocument"],
		ErrorCode::RequestFailed,
		"Text document parameter missing."
	);

	std::string text = _args["textDocument"]["text"].asString();
	std::string uri = _args["textDocument"]["uri"].asString();
	m_openFiles.insert(uri);
	m_fileRepository.setSourceByUri(uri, std::move(text));
	compileAndUpdateDiagnostics();
}

void LanguageServer::handleTextDocumentDidChange(Json::Value const& _args)
{
	requireServerInitialized();

	std::string const uri = _args["textDocument"]["uri"].asString();

	for (Json::Value jsonContentChange: _args["contentChanges"])
	{
		lspRequire(
			jsonContentChange.isObject(),
			ErrorCode::RequestFailed,
			"Invalid content reference."
		);

		std::string const sourceUnitName = m_fileRepository.uriToSourceUnitName(uri);
		lspRequire(
			m_fileRepository.sourceUnits().count(sourceUnitName),
			ErrorCode::RequestFailed,
			"Unknown file: " + uri
		);

		std::string text = jsonContentChange["text"].asString();
		if (jsonContentChange["range"].isObject()) // otherwise full content update
		{
			std::optional<SourceLocation> change = parseRange(m_fileRepository, sourceUnitName, jsonContentChange["range"]);
			lspRequire(
				change && change->hasText(),
				ErrorCode::RequestFailed,
				"Invalid source range: " + util::jsonCompactPrint(jsonContentChange["range"])
			);

			std::string buffer = m_fileRepository.sourceUnits().at(sourceUnitName);
			buffer.replace(static_cast<size_t>(change->start), static_cast<size_t>(change->end - change->start), std::move(text));
			text = std::move(buffer);
		}
		m_fileRepository.setSourceByUri(uri, std::move(text));
	}

	compileAndUpdateDiagnostics();
}

void LanguageServer::handleTextDocumentDidClose(Json::Value const& _args)
{
	requireServerInitialized();

	lspRequire(
		_args["textDocument"],
		ErrorCode::RequestFailed,
		"Text document parameter missing."
	);

	std::string uri = _args["textDocument"]["uri"].asString();
	m_openFiles.erase(uri);

	compileAndUpdateDiagnostics();
}

ASTNode const* LanguageServer::astNodeAtSourceLocation(std::string const& _sourceUnitName, LineColumn const& _filePos)
{
	return std::get<ASTNode const*>(astNodeAndOffsetAtSourceLocation(_sourceUnitName, _filePos));
}

std::tuple<ASTNode const*, int> LanguageServer::astNodeAndOffsetAtSourceLocation(std::string const& _sourceUnitName, LineColumn const& _filePos)
{
	if (m_compilerStack.state() < CompilerStack::AnalysisSuccessful)
		return {nullptr, -1};
	if (!m_fileRepository.sourceUnits().count(_sourceUnitName))
		return {nullptr, -1};

	std::optional<int> sourcePos = m_compilerStack.charStream(_sourceUnitName).translateLineColumnToPosition(_filePos);
	if (!sourcePos)
		return {nullptr, -1};

	return {locateInnermostASTNode(*sourcePos, m_compilerStack.ast(_sourceUnitName)), *sourcePos};
}
