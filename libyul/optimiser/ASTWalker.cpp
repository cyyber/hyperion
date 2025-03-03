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
 * Generic AST walker.
 */

#include <libyul/optimiser/ASTWalker.h>

#include <libyul/AST.h>

#include <range/v3/view/reverse.hpp>

using namespace hyperion;
using namespace hyperion::yul;
using namespace hyperion::util;

void ASTWalker::operator()(FunctionCall const& _funCall)
{
	// Does not visit _funCall.functionName on purpose
	walkVector(_funCall.arguments | ranges::views::reverse);
}

void ASTWalker::operator()(ExpressionStatement const& _statement)
{
	visit(_statement.expression);
}

void ASTWalker::operator()(Assignment const& _assignment)
{
	for (auto const& name: _assignment.variableNames)
		(*this)(name);
	visit(*_assignment.value);
}

void ASTWalker::operator()(VariableDeclaration const& _varDecl)
{
	if (_varDecl.value)
		visit(*_varDecl.value);
}

void ASTWalker::operator()(If const& _if)
{
	visit(*_if.condition);
	(*this)(_if.body);
}

void ASTWalker::operator()(Switch const& _switch)
{
	visit(*_switch.expression);
	for (auto const& _case: _switch.cases)
	{
		if (_case.value)
			(*this)(*_case.value);
		(*this)(_case.body);
	}
}

void ASTWalker::operator()(FunctionDefinition const& _fun)
{
	(*this)(_fun.body);
}

void ASTWalker::operator()(ForLoop const& _for)
{
	(*this)(_for.pre);
	visit(*_for.condition);
	(*this)(_for.body);
	(*this)(_for.post);
}

void ASTWalker::operator()(Block const& _block)
{
	walkVector(_block.statements);
}

void ASTWalker::visit(Statement const& _st)
{
	std::visit(*this, _st);
}

void ASTWalker::visit(Expression const& _e)
{
	std::visit(*this, _e);
}

void ASTModifier::operator()(FunctionCall& _funCall)
{
	// Does not visit _funCall.functionName on purpose
	walkVector(_funCall.arguments | ranges::views::reverse);
}

void ASTModifier::operator()(ExpressionStatement& _statement)
{
	visit(_statement.expression);
}

void ASTModifier::operator()(Assignment& _assignment)
{
	for (auto& name: _assignment.variableNames)
		(*this)(name);
	visit(*_assignment.value);
}

void ASTModifier::operator()(VariableDeclaration& _varDecl)
{
	if (_varDecl.value)
		visit(*_varDecl.value);
}

void ASTModifier::operator()(If& _if)
{
	visit(*_if.condition);
	(*this)(_if.body);
}

void ASTModifier::operator()(Switch& _switch)
{
	visit(*_switch.expression);
	for (auto& _case: _switch.cases)
	{
		if (_case.value)
			(*this)(*_case.value);
		(*this)(_case.body);
	}
}

void ASTModifier::operator()(FunctionDefinition& _fun)
{
	(*this)(_fun.body);
}

void ASTModifier::operator()(ForLoop& _for)
{
	(*this)(_for.pre);
	visit(*_for.condition);
	(*this)(_for.post);
	(*this)(_for.body);
}

void ASTModifier::operator()(Break&)
{
}

void ASTModifier::operator()(Continue&)
{
}

void ASTModifier::operator()(Leave&)
{
}

void ASTModifier::operator()(Block& _block)
{
	walkVector(_block.statements);
}

void ASTModifier::visit(Statement& _st)
{
	std::visit(*this, _st);
}

void ASTModifier::visit(Expression& _e)
{
	std::visit(*this, _e);
}
