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
 * Optimiser component that undoes what the ExpressionSplitter did, i.e.
 * it more or less inlines variable declarations.
 */

#include <libyul/optimiser/ExpressionJoiner.h>

#include <libyul/optimiser/FunctionGrouper.h>
#include <libyul/optimiser/NameCollector.h>
#include <libyul/optimiser/OptimizerUtilities.h>
#include <libyul/Exceptions.h>
#include <libyul/AST.h>

#include <libhyputil/CommonData.h>

#include <range/v3/view/reverse.hpp>

#include <limits>

using namespace hyperion;
using namespace hyperion::yul;

void ExpressionJoiner::run(OptimiserStepContext& _context, Block& _ast)
{
	ExpressionJoiner{_ast}(_ast);
	FunctionGrouper::run(_context, _ast);
}


void ExpressionJoiner::operator()(FunctionCall& _funCall)
{
	handleArguments(_funCall.arguments);
}

void ExpressionJoiner::operator()(Block& _block)
{
	resetLatestStatementPointer();
	for (size_t i = 0; i < _block.statements.size(); ++i)
	{
		visit(_block.statements[i]);
		m_currentBlock = &_block;
		m_latestStatementInBlock = i;
	}

	removeEmptyBlocks(_block);
	resetLatestStatementPointer();
}

void ExpressionJoiner::visit(Expression& _e)
{
	if (std::holds_alternative<Identifier>(_e))
	{
		Identifier const& identifier = std::get<Identifier>(_e);
		if (isLatestStatementVarDeclJoinable(identifier))
		{
			VariableDeclaration& varDecl = std::get<VariableDeclaration>(*latestStatement());
			_e = std::move(*varDecl.value);

			// Delete the variable declaration (also get the moved-from structure back into a sane state)
			*latestStatement() = Block();

			decrementLatestStatementPointer();
		}
	}
	else
		ASTModifier::visit(_e);
}

ExpressionJoiner::ExpressionJoiner(Block& _ast)
{
	m_references = VariableReferencesCounter::countReferences(_ast);
}

void ExpressionJoiner::handleArguments(std::vector<Expression>& _arguments)
{
	// We have to fill from left to right, but we can only
	// fill if everything to the right is just an identifier
	// or a literal.
	// Also we only descend into function calls if everything
	// on the right is an identifier or literal.

	size_t i = _arguments.size();
	for (Expression const& arg: _arguments | ranges::views::reverse)
	{
		--i;
		if (!std::holds_alternative<Identifier>(arg) && !std::holds_alternative<Literal>(arg))
			break;
	}
	// i points to the last element that is neither an identifier nor a literal,
	// or to the first element if all of them are identifiers or literals.

	for (; i < _arguments.size(); ++i)
		visit(_arguments.at(i));
}

void ExpressionJoiner::decrementLatestStatementPointer()
{
	if (!m_currentBlock)
		return;
	if (m_latestStatementInBlock > 0)
		--m_latestStatementInBlock;
	else
		resetLatestStatementPointer();
}

void ExpressionJoiner::resetLatestStatementPointer()
{
	m_currentBlock = nullptr;
	m_latestStatementInBlock = std::numeric_limits<size_t>::max();
}

Statement* ExpressionJoiner::latestStatement()
{
	if (!m_currentBlock)
		return nullptr;
	else
		return &m_currentBlock->statements.at(m_latestStatementInBlock);
}

bool ExpressionJoiner::isLatestStatementVarDeclJoinable(Identifier const& _identifier)
{
	Statement const* statement = latestStatement();
	if (!statement || !std::holds_alternative<VariableDeclaration>(*statement))
		return false;
	VariableDeclaration const& varDecl = std::get<VariableDeclaration>(*statement);
	if (varDecl.variables.size() != 1 || !varDecl.value)
		return false;
	assertThrow(varDecl.variables.size() == 1, OptimizerException, "");
	assertThrow(varDecl.value, OptimizerException, "");
	return varDecl.variables.at(0).name == _identifier.name && m_references[_identifier.name] == 1;
}
