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
 * Module providing metrics for the optimizer.
 */

#pragma once

#include <libyul/optimiser/ASTWalker.h>
#include <liblangutil/ZVMVersion.h>
#include <libhyputil/Numeric.h>
#include <libzvmasm/Instruction.h>

namespace hyperion::yul
{

struct ZVMDialect;

/**
 * Gas meter for expressions only involving literals, identifiers and
 * ZVM instructions.
 *
 * Assumes that EXP is not used with exponents larger than a single byte.
 * Is not particularly exact for anything apart from arithmetic.
 *
 * Assumes that Keccak-256 is computed on a single word (rounded up).
 */
class GasMeter
{
public:
	GasMeter(ZVMDialect const& _dialect, bool _isCreation, bigint _runs):
		m_dialect(_dialect),
		m_isCreation{_isCreation},
		m_runs(_isCreation? 1 : _runs)
	{}

	/// @returns the full combined costs of deploying and evaluating the expression.
	bigint costs(Expression const& _expression) const;
	/// @returns the combined costs of deploying and running the instruction, not including
	/// the costs for its arguments.
	bigint instructionCosts(zvmasm::Instruction _instruction) const;

private:
	bigint combineCosts(std::pair<bigint, bigint> _costs) const;

	ZVMDialect const& m_dialect;
	bool m_isCreation = false;
	bigint m_runs;
};

class GasMeterVisitor: public ASTWalker
{
public:
	static std::pair<bigint, bigint> costs(
		Expression const& _expression,
		ZVMDialect const& _dialect,
		bool _isCreation
	);

	static std::pair<bigint, bigint> instructionCosts(
		zvmasm::Instruction _instruction,
		ZVMDialect const& _dialect,
		bool _isCreation = false
	);

public:
	GasMeterVisitor(ZVMDialect const& _dialect, bool _isCreation):
		m_dialect(_dialect),
		m_isCreation{_isCreation}
	{}

	void operator()(FunctionCall const& _funCall) override;
	void operator()(Literal const& _literal) override;
	void operator()(Identifier const& _identifier) override;

private:
	bigint singleByteDataGas() const;
	/// Computes the cost of storing and executing the single instruction (excluding its arguments).
	/// For EXP, it assumes that the exponent is at most 255.
	/// Does not work particularly exact for anything apart from arithmetic.
	void instructionCostsInternal(zvmasm::Instruction _instruction);

	ZVMDialect const& m_dialect;
	bool m_isCreation = false;
	bigint m_runGas = 0;
	bigint m_dataGas = 0;
};

}
