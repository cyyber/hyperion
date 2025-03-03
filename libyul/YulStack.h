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
/**
 * Full assembly stack that can support ZVM-assembly and Yul as input and ZVM.
 */

#pragma once

#include <liblangutil/CharStreamProvider.h>
#include <liblangutil/DebugInfoSelection.h>
#include <liblangutil/ErrorReporter.h>
#include <liblangutil/ZVMVersion.h>

#include <libyul/Object.h>
#include <libyul/ObjectParser.h>

#include <libhyperion/interface/OptimiserSettings.h>

#include <libzvmasm/LinkerObject.h>

#include <json/json.h>

#include <memory>
#include <string>

namespace hyperion::zvmasm
{
class Assembly;
}

namespace hyperion::langutil
{
class Scanner;
}

namespace hyperion::yul
{
class AbstractAssembly;


struct MachineAssemblyObject
{
	std::shared_ptr<zvmasm::LinkerObject> bytecode;
	std::string assembly;
	std::unique_ptr<std::string> sourceMappings;
};

/*
 * Full assembly stack that can support ZVM-assembly and Yul as input and ZVM as output.
 */
class YulStack: public langutil::CharStreamProvider
{
public:
	enum class Language { Yul, Assembly, StrictAssembly };
	enum class Machine { ZVM };

	YulStack():
		YulStack(
			langutil::ZVMVersion{},
			Language::Assembly,
			hyperion::frontend::OptimiserSettings::none(),
			langutil::DebugInfoSelection::Default()
		)
	{}

	YulStack(
		langutil::ZVMVersion _zvmVersion,
		Language _language,
		hyperion::frontend::OptimiserSettings _optimiserSettings,
		langutil::DebugInfoSelection const& _debugInfoSelection
	):
		m_language(_language),
		m_zvmVersion(_zvmVersion),
		m_optimiserSettings(std::move(_optimiserSettings)),
		m_debugInfoSelection(_debugInfoSelection),
		m_errorReporter(m_errors)
	{}

	/// @returns the char stream used during parsing
	langutil::CharStream const& charStream(std::string const& _sourceName) const override;

	/// Runs parsing and analysis steps, returns false if input cannot be assembled.
	/// Multiple calls overwrite the previous state.
	bool parseAndAnalyze(std::string const& _sourceName, std::string const& _source);

	/// Run the optimizer suite. Can only be used with Yul or strict assembly.
	/// If the settings (see constructor) disabled the optimizer, nothing is done here.
	void optimize();

	/// Run the assembly step (should only be called after parseAndAnalyze).
	MachineAssemblyObject assemble(Machine _machine) const;

	/// Run the assembly step (should only be called after parseAndAnalyze).
	/// In addition to the value returned by @a assemble, returns
	/// a second object that is the runtime code.
	/// Only available for ZVM.
	std::pair<MachineAssemblyObject, MachineAssemblyObject>
	assembleWithDeployed(
		std::optional<std::string_view> _deployName = {}
	) const;

	/// Run the assembly step (should only be called after parseAndAnalyze).
	/// Similar to @a assemblyWithDeployed, but returns ZVM assembly objects.
	/// Only available for ZVM.
	std::pair<std::shared_ptr<zvmasm::Assembly>, std::shared_ptr<zvmasm::Assembly>>
	assembleZVMWithDeployed(
		std::optional<std::string_view> _deployName = {}
	) const;

	/// @returns the errors generated during parsing, analysis (and potentially assembly).
	langutil::ErrorList const& errors() const { return m_errors; }

	/// Pretty-print the input after having parsed it.
	std::string print(
		langutil::CharStreamProvider const* _hyperionSourceProvider = nullptr
	) const;
	Json::Value astJson() const;
	/// Return the parsed and analyzed object.
	std::shared_ptr<Object> parserResult() const;

private:
	bool analyzeParsed();
	bool analyzeParsed(yul::Object& _object);

	void compileZVM(yul::AbstractAssembly& _assembly, bool _optimize) const;

	void optimize(yul::Object& _object, bool _isCreation);

	Language m_language = Language::Assembly;
	langutil::ZVMVersion m_zvmVersion;
	hyperion::frontend::OptimiserSettings m_optimiserSettings;
	langutil::DebugInfoSelection m_debugInfoSelection{};

	std::unique_ptr<langutil::CharStream> m_charStream;

	bool m_analysisSuccessful = false;
	std::shared_ptr<yul::Object> m_parserResult;
	langutil::ErrorList m_errors;
	langutil::ErrorReporter m_errorReporter;

	std::unique_ptr<std::string> m_sourceMappings;
};

}
