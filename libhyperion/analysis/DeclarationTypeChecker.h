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

#pragma once

#include <libhyperion/ast/ASTVisitor.h>
#include <libhyperion/ast/ASTAnnotations.h>
#include <liblangutil/ZVMVersion.h>

#include <list>
#include <map>

namespace hyperion::langutil
{
class ErrorReporter;
}

namespace hyperion::frontend
{

/**
 * Assigns types to declarations.
 */
class DeclarationTypeChecker: private ASTConstVisitor
{
public:
	DeclarationTypeChecker(
		langutil::ErrorReporter& _errorReporter,
		langutil::ZVMVersion _zvmVersion
	):
		m_errorReporter(_errorReporter),
		m_zvmVersion(_zvmVersion)
	{}

	bool check(ASTNode const& _contract);

private:

	bool visit(ElementaryTypeName const& _typeName) override;
	void endVisit(UserDefinedTypeName const& _typeName) override;
	void endVisit(IdentifierPath const& _identifierPath) override;
	bool visit(FunctionTypeName const& _typeName) override;
	void endVisit(Mapping const& _mapping) override;
	void endVisit(ArrayTypeName const& _typeName) override;
	void endVisit(VariableDeclaration const& _variable) override;
	bool visit(EnumDefinition const& _enum) override;
	bool visit(StructDefinition const& _struct) override;
	void endVisit(UserDefinedValueTypeDefinition const& _userDefined) override;
	bool visit(UsingForDirective const& _usingForDirective) override;
	bool visit(InheritanceSpecifier const& _inheritanceSpecifier) override;

	langutil::ErrorReporter& m_errorReporter;
	langutil::ZVMVersion m_zvmVersion;
	bool m_insideFunctionType = false;
	bool m_recursiveStructSeen = false;
	std::set<StructDefinition const*> m_currentStructsSeen;
};

}
