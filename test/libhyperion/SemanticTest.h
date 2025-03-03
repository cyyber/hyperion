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

#pragma once

#include <test/libhyperion/util/TestFileParser.h>
#include <test/libhyperion/util/TestFunctionCall.h>
#include <test/libhyperion/HyperionExecutionFramework.h>
#include <test/libhyperion/AnalysisFramework.h>
#include <test/TestCase.h>
#include <liblangutil/Exceptions.h>
#include <libhyputil/AnsiColorized.h>

#include <iosfwd>
#include <string>
#include <vector>
#include <utility>

namespace hyperion::frontend::test
{

struct AnnotatedEventSignature
{
	std::string signature;
	std::vector<std::string> types;
	std::vector<std::string> indexedTypes;
	std::vector<std::string> nonIndexedTypes;
};

enum class RequiresYulOptimizer
{
	False,
	MinimalStack,
	Full,
};

std::ostream& operator<<(std::ostream& _output, RequiresYulOptimizer _requiresYulOptimizer);

/**
 * Class that represents a semantic test (or end-to-end test) and allows running it as part of the
 * boost unit test environment or ihyptest. It reads the Hyperion source and an additional comment
 * section from the given file. This comment section should define a set of functions to be called
 * and an expected result they return after being executed.
 */
class SemanticTest: public HyperionExecutionFramework, public ZVMVersionRestrictedTestCase
{
public:
	static std::unique_ptr<TestCase> create(Config const& _options)
	{
		return std::make_unique<SemanticTest>(
			_options.filename,
			_options.zvmVersion,
			_options.vmPaths,
			_options.enforceGasCost,
			_options.enforceGasCostMinValue
		);
	}

	explicit SemanticTest(
		std::string const& _filename,
		langutil::ZVMVersion _zvmVersion,
		std::vector<boost::filesystem::path> const& _vmPaths,
		bool _enforceGasCost = false,
		u256 _enforceGasCostMinValue = 100000
	);

	TestResult run(std::ostream& _stream, std::string const& _linePrefix = "", bool _formatted = false) override;
	void printSource(std::ostream &_stream, std::string const& _linePrefix = "", bool _formatted = false) const override;
	void printUpdatedExpectations(std::ostream& _stream, std::string const& _linePrefix = "") const override;
	void printUpdatedSettings(std::ostream& _stream, std::string const& _linePrefix = "") override;

	/// Instantiates a test file parser that parses the additional comment section at the end of
	/// the input stream \param _stream. Each function call is represented using a `FunctionCallTest`
	/// and added to the list of call to be executed when `run()` is called.
	/// Throws if parsing expectations failed.
	void parseExpectations(std::istream& _stream);

	/// Compiles and deploys currently held source.
	/// Returns true if deployment was successful, false otherwise.
	bool deploy(std::string const& _contractName, u256 const& _value, bytes const& _arguments, std::map<std::string, hyperion::test::Address> const& _libraries = {});

private:
	TestResult runTest(
		std::ostream& _stream,
		std::string const& _linePrefix,
		bool _formatted,
		bool _isYulRun
	);
	TestResult tryRunTestWithYulOptimizer(
		std::ostream& _stream,
		std::string const& _linePrefix,
		bool _formatted
	);
	bool checkGasCostExpectation(TestFunctionCall& io_test, bool _compileViaYul) const;
	std::map<std::string, Builtin> makeBuiltins();
	std::vector<SideEffectHook> makeSideEffectHooks() const;
	std::vector<std::string> eventSideEffectHook(FunctionCall const&) const;
	std::optional<AnnotatedEventSignature> matchEvent(util::h256 const& hash) const;
	static std::string formatEventParameter(std::optional<AnnotatedEventSignature> _signature, bool _indexed, size_t _index, bytes const& _data);

	OptimiserSettings optimizerSettingsFor(RequiresYulOptimizer _requiresYulOptimizer);

	SourceMap m_sources;
	std::size_t m_lineOffset;
	std::vector<TestFunctionCall> m_tests;
	std::map<std::string, Builtin> const m_builtins;
	std::vector<SideEffectHook> const m_sideEffectHooks;
	bool m_testCaseWantsYulRun = true;
	bool m_testCaseWantsLegacyRun = true;
	bool m_runWithABIEncoderV1Only = false;
	bool m_allowNonExistingFunctions = false;
	bool m_gasCostFailure = false;
	bool m_enforceGasCost = false;
	RequiresYulOptimizer m_requiresYulOptimizer{};
	u256 m_enforceGasCostMinValue;
};

}
