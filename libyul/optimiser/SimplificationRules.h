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
 * Module for applying replacement rules against Expressions.
 */

#pragma once

#include <libzvmasm/SimplificationRule.h>

#include <libyul/ASTForward.h>
#include <libyul/YulString.h>

#include <libhyputil/CommonData.h>
#include <libhyputil/Numeric.h>

#include <liblangutil/ZVMVersion.h>
#include <liblangutil/SourceLocation.h>

#include <functional>
#include <optional>
#include <vector>

namespace hyperion::yul
{
struct Dialect;
struct AssignedValue;
class Pattern;

/**
 * Container for all simplification rules.
 */
class SimplificationRules
{
public:
	/// Noncopiable.
	SimplificationRules(SimplificationRules const&) = delete;
	SimplificationRules& operator=(SimplificationRules const&) = delete;

	using Rule = zvmasm::SimplificationRule<Pattern>;

	explicit SimplificationRules(std::optional<langutil::ZVMVersion> _zvmVersion = std::nullopt);

	/// @returns a pointer to the first matching pattern and sets the match
	/// groups accordingly.
	/// @param _ssaValues values of variables that are assigned exactly once.
	static Rule const* findFirstMatch(
		Expression const& _expr,
		Dialect const& _dialect,
		std::function<AssignedValue const*(YulString)> const& _ssaValues
	);

	/// Checks whether the rulelist is non-empty. This is usually enforced
	/// by the constructor, but we had some issues with static initialization.
	bool isInitialized() const;

	static std::optional<std::pair<zvmasm::Instruction, std::vector<Expression> const*>>
	instructionAndArguments(Dialect const& _dialect, Expression const& _expr);

private:
	void addRules(std::vector<Rule> const& _rules);
	void addRule(Rule const& _rule);

	void resetMatchGroups() { m_matchGroups.clear(); }

	std::map<unsigned, Expression const*> m_matchGroups;
	std::vector<zvmasm::SimplificationRule<Pattern>> m_rules[256];
};

enum class PatternKind
{
	Operation,
	Constant,
	Any
};

/**
 * Pattern to match against an expression.
 * Also stores matched expressions to retrieve them later, for constructing new expressions using
 * ExpressionTemplate.
 */
class Pattern
{
public:
	using Builtins = zvmasm::ZVMBuiltins<Pattern>;
	static constexpr size_t WordSize = 256;
	using Word = u256;

	/// Matches any expression.
	Pattern(PatternKind _kind = PatternKind::Any): m_kind(_kind) {}
	// Matches a specific constant value.
	Pattern(unsigned _value): Pattern(u256(_value)) {}
	Pattern(int _value): Pattern(u256(_value)) {}
	Pattern(long unsigned _value): Pattern(u256(_value)) {}
	// Matches a specific constant value.
	Pattern(u256 const& _value): m_kind(PatternKind::Constant), m_data(std::make_shared<u256>(_value)) {}
	// Matches a given instruction with given arguments
	Pattern(zvmasm::Instruction _instruction, std::initializer_list<Pattern> _arguments = {});
	/// Sets this pattern to be part of the match group with the identifier @a _group.
	/// Inside one rule, all patterns in the same match group have to match expressions from the
	/// same expression equivalence class.
	void setMatchGroup(unsigned _group, std::map<unsigned, Expression const*>& _matchGroups);
	unsigned matchGroup() const { return m_matchGroup; }
	bool matches(
		Expression const& _expr,
		Dialect const& _dialect,
		std::function<AssignedValue const*(YulString)> const& _ssaValues
	) const;

	std::vector<Pattern> arguments() const { return m_arguments; }

	/// @returns the data of the matched expression if this pattern is part of a match group.
	u256 d() const;

	zvmasm::Instruction instruction() const;

	/// Turns this pattern into an actual expression. Should only be called
	/// for patterns resulting from an action, i.e. with match groups assigned.
	Expression toExpression(std::shared_ptr<DebugData const> const& _debugData, langutil::ZVMVersion _zvmVersion) const;

private:
	Expression const& matchGroupValue() const;

	PatternKind m_kind = PatternKind::Any;
	zvmasm::Instruction m_instruction; ///< Only valid if m_kind is Operation
	std::shared_ptr<u256> m_data; ///< Only valid if m_kind is Constant
	std::vector<Pattern> m_arguments;
	unsigned m_matchGroup = 0;
	std::map<unsigned, Expression const*>* m_matchGroups = nullptr;
};

}
