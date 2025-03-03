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
 * Compiler that transforms Yul Objects to ZVM bytecode objects.
 */

#include <libyul/backends/zvm/ZVMObjectCompiler.h>

#include <libyul/backends/zvm/ZVMCodeTransform.h>
#include <libyul/backends/zvm/ZVMDialect.h>
#include <libyul/backends/zvm/OptimizedZVMCodeTransform.h>

#include <libyul/optimiser/FunctionCallFinder.h>

#include <libyul/Object.h>
#include <libyul/Exceptions.h>

#include <boost/algorithm/string.hpp>

using namespace hyperion::yul;

void ZVMObjectCompiler::compile(
	Object& _object,
	AbstractAssembly& _assembly,
	ZVMDialect const& _dialect,
	bool _optimize
)
{
	ZVMObjectCompiler compiler(_assembly, _dialect);
	compiler.run(_object, _optimize);
}

void ZVMObjectCompiler::run(Object& _object, bool _optimize)
{
	BuiltinContext context;
	context.currentObject = &_object;


	for (auto const& subNode: _object.subObjects)
		if (auto* subObject = dynamic_cast<Object*>(subNode.get()))
		{
			bool isCreation = !boost::ends_with(subObject->name.str(), "_deployed");
			auto subAssemblyAndID = m_assembly.createSubAssembly(isCreation, subObject->name.str());
			context.subIDs[subObject->name] = subAssemblyAndID.second;
			subObject->subId = subAssemblyAndID.second;
			compile(*subObject, *subAssemblyAndID.first, m_dialect, _optimize);
		}
		else
		{
			Data const& data = dynamic_cast<Data const&>(*subNode);
			// Special handling of metadata.
			if (data.name.str() == Object::metadataName())
				m_assembly.appendToAuxiliaryData(data.data);
			else
				context.subIDs[data.name] = m_assembly.appendData(data.data);
		}

	yulAssert(_object.analysisInfo, "No analysis info.");
	yulAssert(_object.code, "No code.");
	if (_optimize)
	{
		auto stackErrors = OptimizedZVMCodeTransform::run(
			m_assembly,
			*_object.analysisInfo,
			*_object.code,
			m_dialect,
			context,
			OptimizedZVMCodeTransform::UseNamedLabels::ForFirstFunctionOfEachName
		);
		if (!stackErrors.empty())
		{
			std::vector<FunctionCall*> memoryGuardCalls = FunctionCallFinder::run(
				*_object.code,
				"memoryguard"_yulstring
			);
			auto stackError = stackErrors.front();
			std::string msg = stackError.comment() ? *stackError.comment() : "";
			if (memoryGuardCalls.empty())
				msg += "\nNo memoryguard was present. "
					"Consider using memory-safe assembly only and annotating it via "
					"'assembly (\"memory-safe\") { ... }'.";
			else
				msg += "\nmemoryguard was present.";
			stackError << util::errinfo_comment(msg);
			BOOST_THROW_EXCEPTION(stackError);
		}
	}
	else
	{
		// We do not catch and re-throw the stack too deep exception here because it is a YulException,
		// which should be native to this part of the code.
		CodeTransform transform{
			m_assembly,
			*_object.analysisInfo,
			*_object.code,
			m_dialect,
			context,
			_optimize,
			{},
			CodeTransform::UseNamedLabels::ForFirstFunctionOfEachName
		};
		transform(*_object.code);
		if (!transform.stackErrors().empty())
			BOOST_THROW_EXCEPTION(transform.stackErrors().front());
	}
}
