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

#include <libhyperion/ast/Types.h>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <utility>

namespace hyperion::frontend
{

/**
 * API for accessing the Hyperion Type System.
 *
 * This is the Hyperion Compiler's type provider. Use it to request for types. The caller does
 * <b>not</b> own the types.
 *
 * It is not recommended to explicitly instantiate types unless you really know what and why
 * you are doing it.
 */
class TypeProvider
{
public:
	TypeProvider() = default;
	TypeProvider(TypeProvider&&) = default;
	TypeProvider(TypeProvider const&) = delete;
	TypeProvider& operator=(TypeProvider&&) = default;
	TypeProvider& operator=(TypeProvider const&) = delete;
	~TypeProvider() = default;

	/// Resets state of this TypeProvider to initial state, wiping all mutable types.
	/// This invalidates all dangling pointers to types provided by this TypeProvider.
	static void reset();

	/// @name Factory functions
	/// Factory functions that convert an AST @ref TypeName to a Type.
	static Type const* fromElementaryTypeName(ElementaryTypeNameToken const& _type, std::optional<StateMutability> _stateMutability = {});

	/// Converts a given elementary type name with optional data location
	/// suffix " storage", " calldata" or " memory" to a type pointer. If suffix not given, defaults to " storage".
	static Type const* fromElementaryTypeName(std::string const& _name);

	/// @returns boolean type.
	static BoolType const* boolean() noexcept { return &m_boolean; }

	static FixedBytesType const* byte() { return fixedBytes(1); }
	static FixedBytesType const* fixedBytes(unsigned m) { return m_bytesM.at(m - 1).get(); }

	static ArrayType const* bytesStorage();
	static ArrayType const* bytesMemory();
	static ArrayType const* bytesCalldata();
	static ArrayType const* stringStorage();
	static ArrayType const* stringMemory();

	/// Constructor for a byte array ("bytes") and string.
	static ArrayType const* array(DataLocation _location, bool _isString = false);

	/// Constructor for a dynamically sized array type ("type[]")
	static ArrayType const* array(DataLocation _location, Type const* _baseType);

	/// Constructor for a fixed-size array type ("type[20]")
	static ArrayType const* array(DataLocation _location, Type const* _baseType, u256 const& _length);

	static ArraySliceType const* arraySlice(ArrayType const& _arrayType);

	static AddressType const* payableAddress() noexcept { return &m_payableAddress; }
	static AddressType const* address() noexcept { return &m_address; }

	static IntegerType const* integer(unsigned _bits, IntegerType::Modifier _modifier)
	{
		hypAssert((_bits % 8) == 0, "");
		if (_modifier == IntegerType::Modifier::Unsigned)
			return m_uintM.at(_bits / 8 - 1).get();
		else
			return m_intM.at(_bits / 8 - 1).get();
	}
	static IntegerType const* uint(unsigned _bits) { return integer(_bits, IntegerType::Modifier::Unsigned); }

	static IntegerType const* uint256() { return uint(256); }
	static IntegerType const* int256() { return integer(256, IntegerType::Modifier::Signed); }

	static FixedPointType const* fixedPoint(unsigned m, unsigned n, FixedPointType::Modifier _modifier);

	static StringLiteralType const* stringLiteral(std::string const& literal);

	/// @param members the member types the tuple type must contain. This is passed by value on purspose.
	/// @returns a tuple type with the given members.
	static TupleType const* tuple(std::vector<Type const*> members);

	static TupleType const* emptyTuple() noexcept { return &m_emptyTuple; }

	static ReferenceType const* withLocation(ReferenceType const* _type, DataLocation _location, bool _isPointer);

	/// @returns a copy of @a _type having the same location as this (and is not a pointer type)
	///          if _type is a reference type and an unmodified copy of _type otherwise.
	///          This function is mostly useful to modify inner types appropriately.
	static Type const* withLocationIfReference(DataLocation _location, Type const* _type, bool _isPointer = false)
	{
		if (auto refType = dynamic_cast<ReferenceType const*>(_type))
			return withLocation(refType, _location, _isPointer);

		return _type;
	}

	static bool isReferenceWithLocation(Type const* _type, DataLocation _location)
	{
		if (auto const* refType = dynamic_cast<ReferenceType const*>(_type))
			if (refType->location() == _location)
				return true;
		return false;
	}

	/// @returns the internally-facing or externally-facing type of a function or the type of a function declaration.
	static FunctionType const* function(FunctionDefinition const& _function, FunctionType::Kind _kind = FunctionType::Kind::Declaration);

	/// @returns the accessor function type of a state variable.
	static FunctionType const* function(VariableDeclaration const& _varDecl);

	/// @returns the function type of an event.
	static FunctionType const* function(EventDefinition const& _event);

	static FunctionType const* function(ErrorDefinition const& _error);

	/// @returns the type of a function type name.
	static FunctionType const* function(FunctionTypeName const& _typeName);

	/// @returns the function type to be used for a plain type (not derived from a declaration).
	static FunctionType const* function(
		strings const& _parameterTypes,
		strings const& _returnParameterTypes,
		FunctionType::Kind _kind = FunctionType::Kind::Internal,
		StateMutability _stateMutability = StateMutability::NonPayable,
		FunctionType::Options _options = {}
	);

	/// @returns a highly customized FunctionType, use with care.
	static FunctionType const* function(
		TypePointers const& _parameterTypes,
		TypePointers const& _returnParameterTypes,
		strings _parameterNames = strings{},
		strings _returnParameterNames = strings{},
		FunctionType::Kind _kind = FunctionType::Kind::Internal,
		StateMutability _stateMutability = StateMutability::NonPayable,
		Declaration const* _declaration = nullptr,
		FunctionType::Options _options = {}
	);

	/// Auto-detect the proper type for a literal. @returns an empty pointer if the literal does
	/// not fit any type.
	static Type const* forLiteral(Literal const& _literal);
	static RationalNumberType const* rationalNumber(Literal const& _literal);

	static RationalNumberType const* rationalNumber(
		rational const& _value,
		Type const* _compatibleBytesType = nullptr
	);

	static ContractType const* contract(ContractDefinition const& _contract, bool _isSuper = false);

	static InaccessibleDynamicType const* inaccessibleDynamic() noexcept { return &m_inaccessibleDynamic; }

	/// @returns the type of an enum instance for given definition, there is one distinct type per enum definition.
	static EnumType const* enumType(EnumDefinition const& _enum);

	/// @returns special type for imported modules. These mainly give access to their scope via members.
	static ModuleType const* module(SourceUnit const& _source);

	static TypeType const* typeType(Type const* _actualType);

	static StructType const* structType(StructDefinition const& _struct, DataLocation _location);

	static ModifierType const* modifier(ModifierDefinition const& _modifierDef);

	static MagicType const* magic(MagicType::Kind _kind);

	static MagicType const* meta(Type const* _type);

	static MappingType const* mapping(Type const* _keyType, ASTString _keyName, Type const* _valueType, ASTString _valueName);

	static UserDefinedValueType const* userDefinedValueType(UserDefinedValueTypeDefinition const& _definition);

private:
	/// Global TypeProvider instance.
	static TypeProvider& instance()
	{
		static TypeProvider _provider;
		return _provider;
	}

	template <typename T, typename... Args>
	static inline T const* createAndGet(Args&& ... _args);

	static BoolType const m_boolean;
	static InaccessibleDynamicType const m_inaccessibleDynamic;

	/// These are lazy-initialized because they depend on `byte` being available.
	static std::unique_ptr<ArrayType> m_bytesStorage;
	static std::unique_ptr<ArrayType> m_bytesMemory;
	static std::unique_ptr<ArrayType> m_bytesCalldata;
	static std::unique_ptr<ArrayType> m_stringStorage;
	static std::unique_ptr<ArrayType> m_stringMemory;

	static TupleType const m_emptyTuple;
	static AddressType const m_payableAddress;
	static AddressType const m_address;
	static std::array<std::unique_ptr<IntegerType>, 32> const m_intM;
	static std::array<std::unique_ptr<IntegerType>, 32> const m_uintM;
	static std::array<std::unique_ptr<FixedBytesType>, 32> const m_bytesM;
	static std::array<std::unique_ptr<MagicType>, 4> const m_magics;        ///< MagicType's except MetaType

	std::map<std::pair<unsigned, unsigned>, std::unique_ptr<FixedPointType>> m_ufixedMxN{};
	std::map<std::pair<unsigned, unsigned>, std::unique_ptr<FixedPointType>> m_fixedMxN{};
	std::map<std::string, std::unique_ptr<StringLiteralType>> m_stringLiteralTypes{};
	std::vector<std::unique_ptr<Type>> m_generalTypes{};
};

}
