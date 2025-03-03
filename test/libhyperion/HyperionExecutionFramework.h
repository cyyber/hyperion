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
 * @author Christian <c@ethdev.com>
 * @date 2014
 * Framework for executing Hyperion contracts and testing them against C++ implementation.
 */

#pragma once

#include <functional>

#include <test/ExecutionFramework.h>

#include <libhyperion/interface/CompilerStack.h>
#include <libhyperion/interface/DebugSettings.h>

#include <libyul/YulStack.h>

namespace hyperion::frontend::test
{

class HyperionExecutionFramework: public hyperion::test::ExecutionFramework
{

public:
	HyperionExecutionFramework(): m_showMetadata(hyperion::test::CommonOptions::get().showMetadata) {}
	explicit HyperionExecutionFramework(
		langutil::ZVMVersion _zvmVersion,
		std::vector<boost::filesystem::path> const& _vmPaths,
		bool _appendCBORMetadata = true
	):
		ExecutionFramework(_zvmVersion, _vmPaths),
		m_showMetadata(hyperion::test::CommonOptions::get().showMetadata),
		m_appendCBORMetadata(_appendCBORMetadata)
	{}

	bytes const& compileAndRunWithoutCheck(
		std::map<std::string, std::string> const& _sourceCode,
		u256 const& _value = 0,
		std::string const& _contractName = "",
		bytes const& _arguments = {},
		std::map<std::string, hyperion::test::Address> const& _libraryAddresses = {},
		std::optional<std::string> const& _sourceName = std::nullopt
	) override
	{
		bytes bytecode = multiSourceCompileContract(_sourceCode, _sourceName, _contractName, _libraryAddresses);
		sendMessage(bytecode + _arguments, true, _value);
		return m_output;
	}

	bytes compileContract(
		std::string const& _sourceCode,
		std::string const& _contractName = "",
		std::map<std::string, hyperion::test::Address> const& _libraryAddresses = {}
	);

	bytes multiSourceCompileContract(
		std::map<std::string, std::string> const& _sources,
		std::optional<std::string> const& _mainSourceName = std::nullopt,
		std::string const& _contractName = "",
		std::map<std::string, hyperion::test::Address> const& _libraryAddresses = {}
	);

protected:
	using CompilerStack = hyperion::frontend::CompilerStack;
	CompilerStack m_compiler;
	bool m_compileViaYul = false;
	bool m_showMetadata = false;
	bool m_appendCBORMetadata = true;
	CompilerStack::MetadataHash m_metadataHash = CompilerStack::MetadataHash::IPFS;
	RevertStrings m_revertStrings = RevertStrings::Default;
};

} // end namespaces
