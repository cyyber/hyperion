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


#include <libyul/ASTForward.h>
#include <libyul/optimiser/ASTWalker.h>
#include <libyul/YulString.h>
#include <libyul/optimiser/OptimiserStep.h>

#include <map>
#include <set>
#include <string>

namespace hyperion::yul
{

struct Dialect;

/**
 * Pass to normalize identifier suffixes.
 *
 * That is, for each function scope, nested suffixes get flattened and all suffixes
 * renumbered by their base name.
 * Function names are not modified.
 *
 * NOTE: This step destroys the promise of the Disambiguator and thus cannot
 * be used in the main loop of the optimizer without running the disambiguator again.
 * Because of that, it is not included in the step list of the Optimizer Suite.
 *
 * Prerequisites: Disambiguator, FunctionHoister, FunctionGrouper
 */
class VarNameCleaner: public ASTModifier
{
public:
	static constexpr char const* name{"VarNameCleaner"};
	static void run(OptimiserStepContext& _context, Block& _ast)
	{
		VarNameCleaner{_ast, _context.dialect, _context.reservedIdentifiers}(_ast);
	}

	using ASTModifier::operator();
	void operator()(VariableDeclaration& _varDecl) override;
	void operator()(Identifier& _identifier) override;
	void operator()(FunctionDefinition& _funDef) override;

private:
	VarNameCleaner(
		Block const& _ast,
		Dialect const& _dialect,
		std::set<YulString> _namesToKeep = {}
	);

	/// Tries to rename a list of variables.
	void renameVariables(std::vector<TypedName>& _variables);

	/// @returns suffix-stripped name, if a suffix was detected, none otherwise.
	YulString stripSuffix(YulString const& _name) const;

	/// Looks out for a "clean name" the given @p name could be trimmed down to.
	/// @returns a trimmed down and "clean name" in case it found one, none otherwise.
	YulString findCleanName(YulString const& name) const;

	/// Tests whether a given name was already used within this pass
	/// or was set to be kept.
	bool isUsedName(YulString const& _name) const;

	Dialect const& m_dialect;

	/// These names will not be modified.
	std::set<YulString> m_namesToKeep;

	/// Set of names that are in use.
	std::set<YulString> m_usedNames;

	/// Maps old to new names.
	std::map<YulString, YulString> m_translatedNames;

	/// Whether the traverse is inside a function definition.
	/// Used to assert that a function definition cannot be inside another.
	bool m_insideFunction = false;
};

}
