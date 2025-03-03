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

#include <libhyperion/formal/Predicate.h>

#include <libhyperion/formal/SMTEncoder.h>

#include <liblangutil/CharStreamProvider.h>
#include <liblangutil/CharStream.h>
#include <libhyperion/ast/AST.h>
#include <libhyperion/ast/TypeProvider.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>

#include <range/v3/view.hpp>
#include <utility>

using boost::algorithm::starts_with;
using namespace hyperion;
using namespace hyperion::smtutil;
using namespace hyperion::frontend;
using namespace hyperion::frontend::smt;

std::map<std::string, Predicate> Predicate::m_predicates;

Predicate const* Predicate::create(
	SortPointer _sort,
	std::string _name,
	PredicateType _type,
	EncodingContext& _context,
	ASTNode const* _node,
	ContractDefinition const* _contractContext,
	std::vector<ScopeOpener const*> _scopeStack
)
{
	smt::SymbolicFunctionVariable predicate{_sort, std::move(_name), _context};
	std::string functorName = predicate.currentName();
	hypAssert(!m_predicates.count(functorName), "");
	return &m_predicates.emplace(
		std::piecewise_construct,
		std::forward_as_tuple(functorName),
		std::forward_as_tuple(std::move(predicate), _type, _node, _contractContext, std::move(_scopeStack))
	).first->second;
}

Predicate::Predicate(
	smt::SymbolicFunctionVariable&& _predicate,
	PredicateType _type,
	ASTNode const* _node,
	ContractDefinition const* _contractContext,
	std::vector<ScopeOpener const*> _scopeStack
):
	m_predicate(std::move(_predicate)),
	m_type(_type),
	m_node(_node),
	m_contractContext(_contractContext),
	m_scopeStack(_scopeStack)
{
}

Predicate const* Predicate::predicate(std::string const& _name)
{
	return &m_predicates.at(_name);
}

void Predicate::reset()
{
	m_predicates.clear();
}

smtutil::Expression Predicate::operator()(std::vector<smtutil::Expression> const& _args) const
{
	return m_predicate(_args);
}

smtutil::Expression Predicate::functor() const
{
	return m_predicate.currentFunctionValue();
}

smtutil::Expression Predicate::functor(unsigned _idx) const
{
	return m_predicate.functionValueAtIndex(_idx);
}

void Predicate::newFunctor()
{
	m_predicate.increaseIndex();
}

ASTNode const* Predicate::programNode() const
{
	return m_node;
}

ContractDefinition const* Predicate::contextContract() const
{
	return m_contractContext;
}

ContractDefinition const* Predicate::programContract() const
{
	if (auto const* contract = dynamic_cast<ContractDefinition const*>(m_node))
		if (!contract->constructor())
			return contract;

	return nullptr;
}

FunctionDefinition const* Predicate::programFunction() const
{
	if (auto const* contract = dynamic_cast<ContractDefinition const*>(m_node))
	{
		if (contract->constructor())
			return contract->constructor();
		return nullptr;
	}

	if (auto const* fun = dynamic_cast<FunctionDefinition const*>(m_node))
		return fun;

	return nullptr;
}

FunctionCall const* Predicate::programFunctionCall() const
{
	return dynamic_cast<FunctionCall const*>(m_node);
}

VariableDeclaration  const* Predicate::programVariable() const
{
	return dynamic_cast<VariableDeclaration const*>(m_node);
}

std::optional<std::vector<VariableDeclaration const*>> Predicate::stateVariables() const
{
	if (m_contractContext)
		return SMTEncoder::stateVariablesIncludingInheritedAndPrivate(*m_contractContext);

	return std::nullopt;
}

bool Predicate::isSummary() const
{
	return isFunctionSummary() ||
		isInternalCall() ||
		isExternalCallTrusted() ||
		isExternalCallUntrusted() ||
		isConstructorSummary();
}

bool Predicate::isFunctionSummary() const
{
	return m_type == PredicateType::FunctionSummary;
}

bool Predicate::isFunctionBlock() const
{
	return m_type == PredicateType::FunctionBlock;
}

bool Predicate::isFunctionErrorBlock() const
{
	return m_type == PredicateType::FunctionErrorBlock;
}

bool Predicate::isInternalCall() const
{
	return m_type == PredicateType::InternalCall;
}

bool Predicate::isExternalCallTrusted() const
{
	return m_type == PredicateType::ExternalCallTrusted;
}

bool Predicate::isExternalCallUntrusted() const
{
	return m_type == PredicateType::ExternalCallUntrusted;
}

bool Predicate::isConstructorSummary() const
{
	return m_type == PredicateType::ConstructorSummary;
}

bool Predicate::isInterface() const
{
	return m_type == PredicateType::Interface;
}

bool Predicate::isNondetInterface() const
{
	return m_type == PredicateType::NondetInterface;
}

std::string Predicate::formatSummaryCall(
	std::vector<smtutil::Expression> const& _args,
	langutil::CharStreamProvider const& _charStreamProvider,
	bool _appendTxVars
) const
{
	hypAssert(isSummary(), "");

	if (programVariable())
		return {};

	if (auto funCall = programFunctionCall())
	{
		if (funCall->location().hasText())
			return std::string(_charStreamProvider.charStream(*funCall->location().sourceName).text(funCall->location()));
		else
			return {};
	}

	/// The signature of a function summary predicate is: summary(error, this, abiFunctions, cryptoFunctions, txData, preBlockChainState, preStateVars, preInputVars, postBlockchainState, postStateVars, postInputVars, outputVars).
	/// Here we are interested in preInputVars to format the function call.

	std::string txModel;

	if (_appendTxVars)
	{
		std::set<std::string> txVars;
		if (isFunctionSummary())
		{
			hypAssert(programFunction(), "");
			if (programFunction()->isPayable())
				txVars.insert("msg.value");
		}
		else if (isConstructorSummary())
		{
			FunctionDefinition const* fun = programFunction();
			if (fun && fun->isPayable())
				txVars.insert("msg.value");
		}

		struct TxVarsVisitor: public ASTConstVisitor
		{
			bool visit(MemberAccess const& _memberAccess)
			{
				Expression const* memberExpr = SMTEncoder::innermostTuple(_memberAccess.expression());

				Type const* exprType = memberExpr->annotation().type;
				hypAssert(exprType, "");
				if (exprType->category() == Type::Category::Magic)
					if (auto const* identifier = dynamic_cast<Identifier const*>(memberExpr))
					{
						ASTString const& name = identifier->name();
						auto memberName = _memberAccess.memberName();

						if (name == "block" || name == "msg" || name == "tx")
							txVars.insert(name + "." + memberName);
					}

				return true;
			}

			std::set<std::string> txVars;
		} txVarsVisitor;

		if (auto fun = programFunction())
		{
			fun->accept(txVarsVisitor);
			txVars += txVarsVisitor.txVars;
		}

		// Here we are interested in txData from the summary predicate.
		auto txValues = readTxVars(_args.at(4));
		std::vector<std::string> values;
		for (auto const& _var: txVars)
			if (auto v = txValues.at(_var))
				values.push_back(_var + ": " + *v);

		if (!values.empty())
			txModel = "{ " + boost::algorithm::join(values, ", ") + " }";
	}

	if (auto contract = programContract())
		return contract->name() + ".constructor()" + txModel;

	auto stateVars = stateVariables();
	hypAssert(stateVars.has_value(), "");
	auto const* fun = programFunction();
	hypAssert(fun, "");

	auto first = _args.begin() + 6 + static_cast<int>(stateVars->size());
	auto last = first + static_cast<int>(fun->parameters().size());
	hypAssert(first >= _args.begin() && first <= _args.end(), "");
	hypAssert(last >= _args.begin() && last <= _args.end(), "");
	auto inTypes = SMTEncoder::replaceUserTypes(FunctionType(*fun).parameterTypes());
	std::vector<std::optional<std::string>> functionArgsCex = formatExpressions(std::vector<smtutil::Expression>(first, last), inTypes);
	std::vector<std::string> functionArgs;

	auto const& params = fun->parameters();
	hypAssert(params.size() == functionArgsCex.size(), "");
	for (unsigned i = 0; i < params.size(); ++i)
		if (params.at(i) && functionArgsCex.at(i))
			functionArgs.emplace_back(*functionArgsCex.at(i));
		else
			functionArgs.emplace_back(params[i]->name());

	std::string fName = fun->isConstructor() ? "constructor" :
		fun->isFallback() ? "fallback" :
		fun->isReceive() ? "receive" :
		fun->name();

	std::string prefix;
	if (fun->isFree())
		prefix = !fun->sourceUnitName().empty() ? (fun->sourceUnitName() + ":") : "";
	else
	{
		hypAssert(fun->annotation().contract, "");
		prefix = fun->annotation().contract->name() + ".";
	}
	return prefix + fName + "(" + boost::algorithm::join(functionArgs, ", ") + ")" + txModel;
}

std::vector<std::optional<std::string>> Predicate::summaryStateValues(std::vector<smtutil::Expression> const& _args) const
{
	/// The signature of a function summary predicate is: summary(error, this, abiFunctions, cryptoFunctions, txData, preBlockchainState, preStateVars, preInputVars, postBlockchainState, postStateVars, postInputVars, outputVars).
	/// The signature of the summary predicate of a contract without constructor is: summary(error, this, abiFunctions, cryptoFunctions, txData, preBlockchainState, postBlockchainState, preStateVars, postStateVars).
	/// Here we are interested in postStateVars.
	auto stateVars = stateVariables();
	hypAssert(stateVars.has_value(), "");

	std::vector<smtutil::Expression>::const_iterator stateFirst;
	std::vector<smtutil::Expression>::const_iterator stateLast;
	if (auto const* function = programFunction())
	{
		stateFirst = _args.begin() + 6 + static_cast<int>(stateVars->size()) + static_cast<int>(function->parameters().size()) + 1;
		stateLast = stateFirst + static_cast<int>(stateVars->size());
	}
	else if (programContract())
	{
		stateFirst = _args.begin() + 7 + static_cast<int>(stateVars->size());
		stateLast = stateFirst + static_cast<int>(stateVars->size());
	}
	else if (programVariable())
		return {};
	else
		hypAssert(false, "");

	hypAssert(stateFirst >= _args.begin() && stateFirst <= _args.end(), "");
	hypAssert(stateLast >= _args.begin() && stateLast <= _args.end(), "");

	std::vector<smtutil::Expression> stateArgs(stateFirst, stateLast);
	hypAssert(stateArgs.size() == stateVars->size(), "");
	auto stateTypes = util::applyMap(*stateVars, [&](auto const& _var) { return _var->type(); });
	return formatExpressions(stateArgs, stateTypes);
}

std::vector<std::optional<std::string>> Predicate::summaryPostInputValues(std::vector<smtutil::Expression> const& _args) const
{
	/// The signature of a function summary predicate is: summary(error, this, abiFunctions, cryptoFunctions, txData, preBlockchainState, preStateVars, preInputVars, postBlockchainState, postStateVars, postInputVars, outputVars).
	/// Here we are interested in postInputVars.
	auto const* function = programFunction();
	hypAssert(function, "");

	auto stateVars = stateVariables();
	hypAssert(stateVars.has_value(), "");

	auto const& inParams = function->parameters();

	auto first = _args.begin() + 6 + static_cast<int>(stateVars->size()) * 2 + static_cast<int>(inParams.size()) + 1;
	auto last = first + static_cast<int>(inParams.size());

	hypAssert(first >= _args.begin() && first <= _args.end(), "");
	hypAssert(last >= _args.begin() && last <= _args.end(), "");

	std::vector<smtutil::Expression> inValues(first, last);
	hypAssert(inValues.size() == inParams.size(), "");
	auto inTypes = SMTEncoder::replaceUserTypes(FunctionType(*function).parameterTypes());
	return formatExpressions(inValues, inTypes);
}

std::vector<std::optional<std::string>> Predicate::summaryPostOutputValues(std::vector<smtutil::Expression> const& _args) const
{
	/// The signature of a function summary predicate is: summary(error, this, abiFunctions, cryptoFunctions, txData, preBlockchainState, preStateVars, preInputVars, postBlockchainState, postStateVars, postInputVars, outputVars).
	/// Here we are interested in outputVars.
	auto const* function = programFunction();
	hypAssert(function, "");

	auto stateVars = stateVariables();
	hypAssert(stateVars.has_value(), "");

	auto const& inParams = function->parameters();

	auto first = _args.begin() + 6 + static_cast<int>(stateVars->size()) * 2 + static_cast<int>(inParams.size()) * 2 + 1;

	hypAssert(first >= _args.begin() && first <= _args.end(), "");

	std::vector<smtutil::Expression> outValues(first, _args.end());
	hypAssert(outValues.size() == function->returnParameters().size(), "");
	auto outTypes = SMTEncoder::replaceUserTypes(FunctionType(*function).returnParameterTypes());
	return formatExpressions(outValues, outTypes);
}

std::pair<std::vector<std::optional<std::string>>, std::vector<VariableDeclaration const*>> Predicate::localVariableValues(std::vector<smtutil::Expression> const& _args) const
{
	/// The signature of a local block predicate is:
	/// block(error, this, abiFunctions, cryptoFunctions, txData, preBlockchainState, preStateVars, preInputVars, postBlockchainState, postStateVars, postInputVars, outputVars, localVars).
	/// Here we are interested in localVars.
	auto const* function = programFunction();
	hypAssert(function, "");

	auto const& localVars = SMTEncoder::localVariablesIncludingModifiers(*function, m_contractContext);
	auto first = _args.end() - static_cast<int>(localVars.size());
	std::vector<smtutil::Expression> outValues(first, _args.end());

	auto mask = util::applyMap(
		localVars,
		[this](auto _var) {
			auto varScope = dynamic_cast<ScopeOpener const*>(_var->scope());
			return find(begin(m_scopeStack), end(m_scopeStack), varScope) != end(m_scopeStack);
		}
	);
	auto localVarsInScope = util::filter(localVars, mask);
	auto outValuesInScope = util::filter(outValues, mask);

	auto outTypes = util::applyMap(localVarsInScope, [](auto _var) { return _var->type(); });
	return {formatExpressions(outValuesInScope, outTypes), localVarsInScope};
}

std::map<std::string, std::string> Predicate::expressionSubstitution(smtutil::Expression const& _predExpr) const
{
	std::map<std::string, std::string> subst;
	std::string predName = functor().name;

	hypAssert(contextContract(), "");
	auto const& stateVars = SMTEncoder::stateVariablesIncludingInheritedAndPrivate(*contextContract());

	auto nArgs = _predExpr.arguments.size();

	// The signature of an interface predicate is
	// interface(this, abiFunctions, cryptoFunctions, blockchainState, stateVariables).
	// An invariant for an interface predicate is a contract
	// invariant over its state, for example `x <= 0`.
	if (isInterface())
	{
		hypAssert(starts_with(predName, "interface"), "");
		subst[_predExpr.arguments.at(0).name] = "address(this)";
		hypAssert(nArgs == stateVars.size() + 4, "");
		for (size_t i = nArgs - stateVars.size(); i < nArgs; ++i)
			subst[_predExpr.arguments.at(i).name] = stateVars.at(i - 4)->name();
	}
	// The signature of a nondet interface predicate is
	// nondet_interface(error, this, abiFunctions, cryptoFunctions, blockchainState, stateVariables, blockchainState', stateVariables').
	// An invariant for a nondet interface predicate is a reentrancy property
	// over the pre and post state variables of a contract, where pre state vars
	// are represented by the variable's name and post state vars are represented
	// by the primed variable's name, for example
	// `(x <= 0) => (x' <= 100)`.
	else if (isNondetInterface())
	{
		hypAssert(starts_with(predName, "nondet_interface"), "");
		subst[_predExpr.arguments.at(0).name] = "<errorCode>";
		subst[_predExpr.arguments.at(1).name] = "address(this)";
		hypAssert(nArgs == stateVars.size() * 2 + 6, "");
		for (size_t i = nArgs - stateVars.size(), s = 0; i < nArgs; ++i, ++s)
			subst[_predExpr.arguments.at(i).name] = stateVars.at(s)->name() + "'";
		for (size_t i = nArgs - (stateVars.size() * 2 + 1), s = 0; i < nArgs - (stateVars.size() + 1); ++i, ++s)
			subst[_predExpr.arguments.at(i).name] = stateVars.at(s)->name();
	}

	return subst;
}

std::vector<std::optional<std::string>> Predicate::formatExpressions(std::vector<smtutil::Expression> const& _exprs, std::vector<Type const*> const& _types) const
{
	hypAssert(_exprs.size() == _types.size(), "");
	std::vector<std::optional<std::string>> strExprs;
	for (unsigned i = 0; i < _exprs.size(); ++i)
		strExprs.push_back(expressionToString(_exprs.at(i), _types.at(i)));
	return strExprs;
}

std::optional<std::string> Predicate::expressionToString(smtutil::Expression const& _expr, Type const* _type) const
{
	if (smt::isNumber(*_type))
	{
		hypAssert(_expr.sort->kind == Kind::Int, "");
		hypAssert(_expr.arguments.empty(), "");

		if (
			_type->category() == Type::Category::Address ||
			_type->category() == Type::Category::FixedBytes
		)
		{
			try
			{
				if (_expr.name == "0")
					return "0x0";
				// For some reason the code below returns "0x" for "0".
				return util::toHex(toCompactBigEndian(bigint(_expr.name)), util::HexPrefix::Add, util::HexCase::Lower);
			}
			catch (std::out_of_range const&)
			{
			}
			catch (std::invalid_argument const&)
			{
			}
		}

		return _expr.name;
	}
	if (smt::isBool(*_type))
	{
		hypAssert(_expr.sort->kind == Kind::Bool, "");
		hypAssert(_expr.arguments.empty(), "");
		hypAssert(_expr.name == "true" || _expr.name == "false", "");
		return _expr.name;
	}
	if (smt::isFunction(*_type))
	{
		hypAssert(_expr.arguments.empty(), "");
		return _expr.name;
	}
	if (smt::isArray(*_type))
	{
		auto const& arrayType = dynamic_cast<ArrayType const&>(*_type);
		if (_expr.name != "tuple_constructor")
			return {};

		auto const& tupleSort = dynamic_cast<TupleSort const&>(*_expr.sort);
		hypAssert(tupleSort.components.size() == 2, "");

		unsigned long length;
		try
		{
			length = stoul(_expr.arguments.at(1).name);
		}
		catch(std::out_of_range const&)
		{
			return {};
		}
		catch(std::invalid_argument const&)
		{
			return {};
		}

		// Limit this counterexample size to 1k.
		// Some OSs give you "unlimited" memory through swap and other virtual memory,
		// so purely relying on bad_alloc being thrown is not a good idea.
		// In that case, the array allocation might cause OOM and the program is killed.
		if (length >= 1024)
			return {};
		try
		{
			std::vector<std::string> array(length);
			if (!fillArray(_expr.arguments.at(0), array, arrayType))
				return {};
			return "[" + boost::algorithm::join(array, ", ") + "]";
		}
		catch (std::bad_alloc const&)
		{
			// Solver gave a concrete array but length is too large.
		}
	}
	if (smt::isNonRecursiveStruct(*_type))
	{
		auto const& structType = dynamic_cast<StructType const&>(*_type);
		hypAssert(_expr.name == "tuple_constructor", "");
		auto const& tupleSort = dynamic_cast<TupleSort const&>(*_expr.sort);
		auto members = structType.structDefinition().members();
		hypAssert(tupleSort.components.size() == members.size(), "");
		hypAssert(_expr.arguments.size() == members.size(), "");
		std::vector<std::string> elements;
		for (unsigned i = 0; i < members.size(); ++i)
		{
			std::optional<std::string> elementStr = expressionToString(_expr.arguments.at(i), members[i]->type());
			elements.push_back(members[i]->name() + (elementStr.has_value() ?  ": " + elementStr.value() : ""));
		}
		return "{" + boost::algorithm::join(elements, ", ") + "}";
	}

	return {};
}

bool Predicate::fillArray(smtutil::Expression const& _expr, std::vector<std::string>& _array, ArrayType const& _type) const
{
	// Base case
	if (_expr.name == "const_array")
	{
		auto length = _array.size();
		std::optional<std::string> elemStr = expressionToString(_expr.arguments.at(1), _type.baseType());
		if (!elemStr)
			return false;
		_array.clear();
		_array.resize(length, *elemStr);
		return true;
	}

	// Recursive case.
	if (_expr.name == "store")
	{
		if (!fillArray(_expr.arguments.at(0), _array, _type))
			return false;
		std::optional<std::string> indexStr = expressionToString(_expr.arguments.at(1), TypeProvider::uint256());
		if (!indexStr)
			return false;
		// Sometimes the solver assigns huge lengths that are not related,
		// we should catch and ignore those.
		unsigned long index;
		try
		{
			index = stoul(*indexStr);
		}
		catch (std::out_of_range const&)
		{
			return true;
		}
		catch (std::invalid_argument const&)
		{
			return true;
		}
		std::optional<std::string> elemStr = expressionToString(_expr.arguments.at(2), _type.baseType());
		if (!elemStr)
			return false;
		if (index < _array.size())
			_array.at(index) = *elemStr;
		return true;
	}

	// Special base case, not supported yet.
	if (_expr.name.rfind("(_ as-array") == 0)
	{
		// Z3 expression representing reinterpretation of a different term as an array
		return false;
	}

	hypAssert(false, "");
}

std::map<std::string, std::optional<std::string>> Predicate::readTxVars(smtutil::Expression const& _tx) const
{
	std::map<std::string, Type const*> const txVars{
		{"block.basefee", TypeProvider::uint256()},
		{"block.chainid", TypeProvider::uint256()},
		{"block.coinbase", TypeProvider::address()},
		{"block.prevrandao", TypeProvider::uint256()},
		{"block.gaslimit", TypeProvider::uint256()},
		{"block.number", TypeProvider::uint256()},
		{"block.timestamp", TypeProvider::uint256()},
		{"blockhash", TypeProvider::array(DataLocation::Memory, TypeProvider::uint256())},
		{"msg.data", TypeProvider::array(DataLocation::CallData)},
		{"msg.sender", TypeProvider::address()},
		{"msg.sig", TypeProvider::fixedBytes(4)},
		{"msg.value", TypeProvider::uint256()},
		{"tx.gasprice", TypeProvider::uint256()},
		{"tx.origin", TypeProvider::address()}
	};
	std::map<std::string, std::optional<std::string>> vars;
	for (auto&& [i, v]: txVars | ranges::views::enumerate)
		vars.emplace(v.first, expressionToString(_tx.arguments.at(i), v.second));
	return vars;
}
