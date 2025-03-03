#include <test/tools/ossfuzz/protoToAbiV2.h>

#include <boost/preprocessor.hpp>
#include <regex>

/// Convenience macros
/// Returns a valid Hyperion integer width w such that 8 <= w <= 256.
#define INTWIDTH(z, n, _ununsed) BOOST_PP_MUL(BOOST_PP_ADD(n, 1), 8)
/// Using declaration that aliases long boost multiprecision types with
/// s(u)<width> where <width> is a valid Hyperion integer width and "s"
/// stands for "signed" and "u" for "unsigned".
#define USINGDECL(z, n, sign) \
	using BOOST_PP_CAT(BOOST_PP_IF(sign, s, u), INTWIDTH(z, n,)) =             \
	boost::multiprecision::number<                                             \
		boost::multiprecision::cpp_int_backend<                                \
			INTWIDTH(z, n,),                                                   \
			INTWIDTH(z, n,),                                                   \
			BOOST_PP_IF(                                                       \
				sign,                                                          \
				boost::multiprecision::signed_magnitude,                       \
				boost::multiprecision::unsigned_magnitude                      \
			),                                                                 \
			boost::multiprecision::unchecked,                                  \
			void                                                               \
		>                                                                      \
	>;
/// Instantiate the using declarations for signed and unsigned integer types.
BOOST_PP_REPEAT(32, USINGDECL, 1)
BOOST_PP_REPEAT(32, USINGDECL, 0)
/// Case implementation that returns an integer value of the specified type.
/// For signed integers, we divide by two because the range for boost multiprecision
/// types is double that of Hyperion integer types. Example, 8-bit signed boost
/// number range is [-255, 255] but Hyperion `int8` range is [-128, 127]
#define CASEIMPL(z, n, sign)                                                   \
	case INTWIDTH(z, n,):                                                      \
		stream << BOOST_PP_IF(                                                 \
			sign,                                                              \
			integerValue<                                                      \
				BOOST_PP_CAT(                                                  \
					BOOST_PP_IF(sign, s, u),                                   \
					INTWIDTH(z, n,)                                            \
                )>(_counter) / 2,                                              \
			integerValue<                                                      \
				BOOST_PP_CAT(                                                  \
					BOOST_PP_IF(sign, s, u),                                   \
					INTWIDTH(z, n,)                                            \
                )>(_counter)                                                   \
        );                                                                     \
		break;
/// Switch implementation that instantiates case statements for (un)signed
/// Hyperion integer types.
#define SWITCHIMPL(sign)                                                       \
	ostringstream stream;                                                      \
	switch (_intWidth)                                                         \
	{                                                                          \
	BOOST_PP_REPEAT(32, CASEIMPL, sign)	                                       \
	}	                                                                       \
	return stream.str();

using namespace std;
using namespace hyperion::util;
using namespace hyperion::test::abiv2fuzzer;

namespace
{
template <typename V>
static V integerValue(unsigned _counter)
{
	V value = V(
		u256(hyperion::util::keccak256(hyperion::util::h256(_counter))) % u256(boost::math::tools::max_value<V>())
	);
	if (boost::multiprecision::is_signed_number<V>::value && value % 2 == 0)
		return value * (-1);
	else
		return value;
}

static string signedIntegerValue(unsigned _counter, unsigned _intWidth)
{
	SWITCHIMPL(1)
}

static string unsignedIntegerValue(unsigned _counter, unsigned _intWidth)
{
	SWITCHIMPL(0)
}

static string integerValue(unsigned _counter, unsigned _intWidth, bool _signed)
{
	if (_signed)
		return signedIntegerValue(_counter, _intWidth);
	else
		return unsignedIntegerValue(_counter, _intWidth);
}
}

string ProtoConverter::getVarDecl(
	string const& _type,
	string const& _varName,
	string const& _qualifier
)
{
	// One level of indentation for state variable declarations
	// Two levels of indentation for local variable declarations
	return Whiskers(R"(
	<?isLocalVar>	</isLocalVar><type><?qual> <qualifier></qual> <varName>;)"
		)
		("isLocalVar", !m_isStateVar)
		("type", _type)
		("qual", !_qualifier.empty())
		("qualifier", _qualifier)
		("varName", _varName)
		.render() +
		"\n";
}

pair<string, string> ProtoConverter::visit(Type const& _type)
{
	switch (_type.type_oneof_case())
	{
	case Type::kVtype:
		return visit(_type.vtype());
	case Type::kNvtype:
		return visit(_type.nvtype());
	case Type::TYPE_ONEOF_NOT_SET:
		return make_pair("", "");
	}
}

pair<string, string> ProtoConverter::visit(ValueType const& _type)
{
	switch (_type.value_type_oneof_case())
	{
	case ValueType::kBoolty:
		return visit(_type.boolty());
	case ValueType::kInty:
		return visit(_type.inty());
	case ValueType::kByty:
		return visit(_type.byty());
	case ValueType::kAdty:
		return visit(_type.adty());
	case ValueType::VALUE_TYPE_ONEOF_NOT_SET:
		return make_pair("", "");
	}
}

pair<string, string> ProtoConverter::visit(NonValueType const& _type)
{
	switch (_type.nonvalue_type_oneof_case())
	{
	case NonValueType::kDynbytearray:
		return visit(_type.dynbytearray());
	case NonValueType::kArrtype:
		if (ValidityVisitor().visit(_type.arrtype()))
			return visit(_type.arrtype());
		else
			return make_pair("", "");
	case NonValueType::kStype:
		if (ValidityVisitor().visit(_type.stype()))
			return visit(_type.stype());
		else
			return make_pair("", "");
	case NonValueType::NONVALUE_TYPE_ONEOF_NOT_SET:
		return make_pair("", "");
	}
}

pair<string, string> ProtoConverter::visit(BoolType const& _type)
{
	return processType(_type, true);
}

pair<string, string> ProtoConverter::visit(IntegerType const& _type)
{
	return processType(_type, true);
}

pair<string, string> ProtoConverter::visit(FixedByteType const& _type)
{
	return processType(_type, true);
}

pair<string, string> ProtoConverter::visit(AddressType const& _type)
{
	return processType(_type, true);
}

pair<string, string> ProtoConverter::visit(DynamicByteArrayType const& _type)
{
	return processType(_type, false);
}

pair<string, string> ProtoConverter::visit(ArrayType const& _type)
{
	return processType(_type, false);
}

pair<string, string> ProtoConverter::visit(StructType const& _type)
{
	return processType(_type, false);
}

template <typename T>
pair<string, string> ProtoConverter::processType(T const& _type, bool _isValueType)
{
	ostringstream local, global;
	auto [varName, paramName] = newVarNames(getNextVarCounter(), m_isStateVar);

	// Add variable name to the argument list of coder function call
	if (m_argsCoder.str().empty())
		m_argsCoder << varName;
	else
		m_argsCoder << ", " << varName;

	string location{};
	if (!m_isStateVar && !_isValueType)
		location = "memory";

	auto varDeclBuffers = varDecl(
		varName,
		paramName,
		_type,
		_isValueType,
		location
	);
	global << varDeclBuffers.first;
	local << varDeclBuffers.second;
	auto assignCheckBuffers = assignChecker(varName, paramName, _type);
	global << assignCheckBuffers.first;
	local << assignCheckBuffers.second;

	m_structCounter += m_numStructsAdded;
	return make_pair(global.str(), local.str());
}

template <typename T>
pair<string, string> ProtoConverter::varDecl(
	string const& _varName,
	string const& _paramName,
	T _type,
	bool _isValueType,
	string const& _location
)
{
	ostringstream local, global;

	TypeVisitor tVisitor(m_structCounter);
	string typeStr = tVisitor.visit(_type);
	if (typeStr.empty())
		return make_pair("", "");

	// Append struct defs
	global << tVisitor.structDef();
	m_numStructsAdded = tVisitor.numStructs();

	// variable declaration
	if (m_isStateVar)
		global << getVarDecl(typeStr, _varName, _location);
	else
		local << getVarDecl(typeStr, _varName, _location);

	// Add typed params for calling public and external functions with said type
	appendTypedParams(
		CalleeType::PUBLIC,
		_isValueType,
		typeStr,
		_paramName,
		((m_varCounter == 1) ? Delimiter::SKIP : Delimiter::ADD)
	);
	appendTypedParams(
		CalleeType::EXTERNAL,
		_isValueType,
		typeStr,
		_paramName,
		((m_varCounter == 1) ? Delimiter::SKIP : Delimiter::ADD)
	);
	appendTypes(
		_isValueType,
		typeStr,
		((m_varCounter == 1) ? Delimiter::SKIP : Delimiter::ADD)
	);
	appendTypedReturn(
		_isValueType,
		typeStr,
		((m_varCounter == 1) ? Delimiter::SKIP : Delimiter::ADD)
	);
	appendToIsabelleTypeString(
		tVisitor.isabelleTypeString(),
		((m_varCounter == 1) ? Delimiter::SKIP : Delimiter::ADD)
	);
	// Update dyn param only if necessary
	if (tVisitor.isLastDynParamRightPadded())
		m_isLastDynParamRightPadded = true;

	return make_pair(global.str(), local.str());
}

template <typename T>
pair<string, string> ProtoConverter::assignChecker(
	string const& _varName,
	string const& _paramName,
	T _type
)
{
	ostringstream local;
	AssignCheckVisitor acVisitor(
		_varName,
		_paramName,
		m_returnValue,
		m_isStateVar,
		m_counter,
		m_structCounter
	);
	pair<string, string> assignCheckStrPair = acVisitor.visit(_type);
	m_returnValue += acVisitor.errorStmts();
	m_counter += acVisitor.counted();

	m_checks << assignCheckStrPair.second;
	appendToIsabelleValueString(
		acVisitor.isabelleValueString(),
		((m_varCounter == 1) ? Delimiter::SKIP : Delimiter::ADD)
	);

	// State variables cannot be assigned in contract-scope
	// Therefore, we buffer their assignments and
	// render them in function scope later.
	local << assignCheckStrPair.first;
	return make_pair("", local.str());
}

pair<string, string> ProtoConverter::visit(VarDecl const& _x)
{
	return visit(_x.type());
}

std::string ProtoConverter::equalityChecksAsString()
{
	return m_checks.str();
}

std::string ProtoConverter::delimiterToString(Delimiter _delimiter, bool _space)
{
	switch (_delimiter)
	{
	case Delimiter::ADD:
		return _space ? ", " : ",";
	case Delimiter::SKIP:
		return "";
	}
}

/* When a new variable is declared, we can invoke this function
 * to prepare the typed param list to be passed to callee functions.
 * We independently prepare this list for "public" and "external"
 * callee functions.
 */
void ProtoConverter::appendTypedParams(
	CalleeType _calleeType,
	bool _isValueType,
	std::string const& _typeString,
	std::string const& _varName,
	Delimiter _delimiter
)
{
	switch (_calleeType)
	{
	case CalleeType::PUBLIC:
		appendTypedParamsPublic(_isValueType, _typeString, _varName, _delimiter);
		break;
	case CalleeType::EXTERNAL:
		appendTypedParamsExternal(_isValueType, _typeString, _varName, _delimiter);
		break;
	}
}

void ProtoConverter::appendTypes(
	bool _isValueType,
	string const& _typeString,
	Delimiter _delimiter
)
{
	string qualifiedTypeString = (
		_isValueType ?
		_typeString :
		_typeString + " memory"
	);
	m_types << Whiskers(R"(<delimiter><type>)")
		("delimiter", delimiterToString(_delimiter))
		("type", qualifiedTypeString)
		.render();
}

void ProtoConverter::appendTypedReturn(
	bool _isValueType,
	string const& _typeString,
	Delimiter _delimiter
)
{
	string qualifiedTypeString = (
		_isValueType ?
		_typeString :
		_typeString + " memory"
	);
	m_typedReturn << Whiskers(R"(<delimiter><type> <varName>)")
		("delimiter", delimiterToString(_delimiter))
		("type", qualifiedTypeString)
		("varName", "lv_" + to_string(m_varCounter - 1))
		.render();
}

// Adds the qualifier "calldata" to non-value parameter of an external function.
void ProtoConverter::appendTypedParamsExternal(
	bool _isValueType,
    std::string const& _typeString,
    std::string const& _varName,
    Delimiter _delimiter
)
{
	m_externalParamsRep.push_back({_delimiter, _isValueType, _typeString, _varName});
	m_untypedParamsExternal << Whiskers(R"(<delimiter><varName>)")
		("delimiter", delimiterToString(_delimiter))
		("varName", _varName)
		.render();
}

// Adds the qualifier "memory" to non-value parameter of an external function.
void ProtoConverter::appendTypedParamsPublic(
	bool _isValueType,
	std::string const& _typeString,
	std::string const& _varName,
	Delimiter _delimiter
)
{
	std::string qualifiedTypeString = (
		_isValueType ?
		_typeString :
		_typeString + " memory"
		);
	m_typedParamsPublic << Whiskers(R"(<delimiter><type> <varName>)")
		("delimiter", delimiterToString(_delimiter))
		("type", qualifiedTypeString)
		("varName", _varName)
		.render();
}

void ProtoConverter::appendToIsabelleTypeString(
	std::string const& _typeString,
	Delimiter _delimiter
)
{
	m_isabelleTypeString << delimiterToString(_delimiter, false) << _typeString;
}

void ProtoConverter::appendToIsabelleValueString(
	std::string const& _valueString,
	Delimiter _delimiter
)
{
	m_isabelleValueString << delimiterToString(_delimiter, false) << _valueString;
}

std::string ProtoConverter::typedParametersAsString(CalleeType _calleeType)
{
	switch (_calleeType)
	{
	case CalleeType::PUBLIC:
		return m_typedParamsPublic.str();
	case CalleeType::EXTERNAL:
	{
		ostringstream typedParamsExternal;
		for (auto const& i: m_externalParamsRep)
		{
			Delimiter del = get<0>(i);
			bool valueType = get<1>(i);
			string typeString = get<2>(i);
			string varName = get<3>(i);
			bool isCalldata = randomBool(/*probability=*/0.5);
			string location = (isCalldata ? "calldata" : "memory");
			string qualifiedTypeString = (valueType ? typeString : typeString + " " + location);
			typedParamsExternal << Whiskers(R"(<delimiter><type> <varName>)")
				("delimiter", delimiterToString(del))
				("type", qualifiedTypeString)
				("varName", varName)
				.render();
		}
		return typedParamsExternal.str();
	}
	}
}

string ProtoConverter::visit(TestFunction const& _x, string const& _storageVarDefs)
{
	// TODO: Support more than one but less than N local variables
	auto localVarBuffers = visit(_x.local_vars());

	string structTypeDecl = localVarBuffers.first;
	string localVarDefs = localVarBuffers.second;

	ostringstream testBuffer;

	string testFunction = Whiskers(R"(
	function test() public returns (uint) {
		<?calldata>return test_calldata_coding();</calldata>
		<?returndata>return test_returndata_coding();</returndata>
	})")
	("calldata", m_test == Contract_Test::Contract_Test_CALLDATA_CODER)
	("returndata", m_test == Contract_Test::Contract_Test_RETURNDATA_CODER)
	.render();

	string functionDeclCalldata = "function test_calldata_coding() internal returns (uint)";
	string functionDeclReturndata = "function test_returndata_coding() internal returns (uint)";

	testBuffer << Whiskers(R"(<structTypeDecl>
<testFunction>
<?calldata>
	<functionDeclCalldata> {
<storageVarDefs>
<localVarDefs>
<calldataTestCode>
	}
<calldataHelperFuncs>
</calldata>
<?returndata>
	<functionDeclReturndata> {
<returndataTestCode>
	}

<?varsPresent>
	function coder_returndata_external() external returns (<return_types>) {
<storageVarDefs>
<localVarDefs>
		return (<return_values>);
	}
</varsPresent>
</returndata>)")
		("testFunction", testFunction)
		("calldata", m_test == Contract_Test::Contract_Test_CALLDATA_CODER)
		("returndata", m_test == Contract_Test::Contract_Test_RETURNDATA_CODER)
		("calldataHelperFuncs", calldataHelperFunctions())
		("varsPresent", !m_types.str().empty())
		("structTypeDecl", structTypeDecl)
		("functionDeclCalldata", functionDeclCalldata)
		("functionDeclReturndata", functionDeclReturndata)
		("storageVarDefs", _storageVarDefs)
		("localVarDefs", localVarDefs)
		("calldataTestCode", testCallDataFunction(static_cast<unsigned>(_x.invalid_encoding_length())))
		("returndataTestCode", testReturnDataFunction())
		("return_types", m_types.str())
		("return_values", m_argsCoder.str())
		.render();
	return testBuffer.str();
}

string ProtoConverter::testCallDataFunction(unsigned _invalidLength)
{
	return Whiskers(R"(
		uint returnVal = this.coder_calldata_public(<argumentNames>);
		if (returnVal != 0)
			return returnVal;

		returnVal = this.coder_calldata_external(<argumentNames>);
		if (returnVal != 0)
			return uint(200000) + returnVal;

		<?atLeastOneVar>
		bytes memory argumentEncoding = abi.encode(<argumentNames>);

		returnVal = checkEncodedCall(
			this.coder_calldata_public.selector,
			argumentEncoding,
			<invalidLengthFuzz>,
			<isRightPadded>
		);
		if (returnVal != 0)
			return returnVal;

		returnVal = checkEncodedCall(
			this.coder_calldata_external.selector,
			argumentEncoding,
			<invalidLengthFuzz>,
			<isRightPadded>
		);
		if (returnVal != 0)
			return uint(200000) + returnVal;
		</atLeastOneVar>
		return 0;
		)")
		("argumentNames", m_argsCoder.str())
		("invalidLengthFuzz", std::to_string(_invalidLength))
		("isRightPadded", isLastDynParamRightPadded() ? "true" : "false")
		("atLeastOneVar", m_varCounter > 0)
		.render();
}

string ProtoConverter::testReturnDataFunction()
{
	return Whiskers(R"(
<?varsPresent>
		(<varDecl>) = this.coder_returndata_external();
<equality_checks>
</varsPresent>
		return 0;
		)")
		("varsPresent", !m_typedReturn.str().empty())
		("varDecl", m_typedReturn.str())
		("equality_checks", m_checks.str())
		.render();
}

string ProtoConverter::calldataHelperFunctions()
{
	stringstream calldataHelperFuncs;
	calldataHelperFuncs << R"(
	/// Accepts function selector, correct argument encoding, and length of
	/// invalid encoding and returns the correct and incorrect abi encoding
	/// for calling the function specified by the function selector.
	function createEncoding(
		bytes4 funcSelector,
		bytes memory argumentEncoding,
		uint invalidLengthFuzz,
		bool isRightPadded
	) internal pure returns (bytes memory, bytes memory)
	{
		bytes memory validEncoding = new bytes(4 + argumentEncoding.length);
		// Ensure that invalidEncoding crops at least 32 bytes (padding length
		// is at most 31 bytes) if `isRightPadded` is true.
		// This is because shorter bytes/string values (whose encoding is right
		// padded) can lead to successful decoding when fewer than 32 bytes have
		// been cropped in the worst case. In other words, if `isRightPadded` is
		// true, then
		//  0 <= invalidLength <= argumentEncoding.length - 32
		// otherwise
		//  0 <= invalidLength <= argumentEncoding.length - 1
		uint invalidLength;
		if (isRightPadded)
			invalidLength = invalidLengthFuzz % (argumentEncoding.length - 31);
		else
			invalidLength = invalidLengthFuzz % argumentEncoding.length;
		bytes memory invalidEncoding = new bytes(4 + invalidLength);
		for (uint i = 0; i < 4; i++)
			validEncoding[i] = invalidEncoding[i] = funcSelector[i];
		for (uint i = 0; i < argumentEncoding.length; i++)
			validEncoding[i+4] = argumentEncoding[i];
		for (uint i = 0; i < invalidLength; i++)
			invalidEncoding[i+4] = argumentEncoding[i];
		return (validEncoding, invalidEncoding);
	}

	/// Accepts function selector, correct argument encoding, and an invalid
	/// encoding length as input. Returns a non-zero value if either call with
	/// correct encoding fails or call with incorrect encoding succeeds.
	/// Returns zero if both calls meet expectation.
	function checkEncodedCall(
		bytes4 funcSelector,
		bytes memory argumentEncoding,
		uint invalidLengthFuzz,
		bool isRightPadded
	) public returns (uint)
	{
		(bytes memory validEncoding, bytes memory invalidEncoding) = createEncoding(
			funcSelector,
			argumentEncoding,
			invalidLengthFuzz,
			isRightPadded
		);
		(bool success, bytes memory returnVal) = address(this).call(validEncoding);
		uint returnCode = abi.decode(returnVal, (uint));
		// Return non-zero value if call fails for correct encoding
		if (success == false || returnCode != 0)
			return 400000;
		(success, ) = address(this).call(invalidEncoding);
		// Return non-zero value if call succeeds for incorrect encoding
		if (success == true)
			return 400001;
		return 0;
	})";

	/// These are indirections to test memory-calldata codings more robustly.
	stringstream indirections;
	unsigned numIndirections = randomNumberOneToN(s_maxIndirections);
	for (unsigned i = 1; i <= numIndirections; i++)
	{
		bool finalIndirection = i == numIndirections;
		string mutability = (finalIndirection ? "pure" : "view");
		indirections << Whiskers(R"(
	function coder_calldata_external_i<N>(<parameters>) external <mutability> returns (uint) {
<?finalIndirection>
<equality_checks>
		return 0;
<!finalIndirection>
		return this.coder_calldata_external_i<NPlusOne>(<untyped_parameters>);
</finalIndirection>
	}
		)")
		("N", to_string(i))
		("parameters", typedParametersAsString(CalleeType::EXTERNAL))
		("mutability", mutability)
		("finalIndirection", finalIndirection)
		("equality_checks", equalityChecksAsString())
		("NPlusOne", to_string(i + 1))
		("untyped_parameters", m_untypedParamsExternal.str())
		.render();
	}

	// These are callee functions that encode from storage, decode to
	// memory/calldata and check if decoded value matches storage value
	// return true on successful match, false otherwise
	calldataHelperFuncs << Whiskers(R"(
	function coder_calldata_public(<parameters_memory>) public pure returns (uint) {
<equality_checks>
		return 0;
	}

	function coder_calldata_external(<parameters_calldata>) external view returns (uint) {
		return this.coder_calldata_external_i1(<untyped_parameters>);
	}
<indirections>
	)")
	("parameters_memory", typedParametersAsString(CalleeType::PUBLIC))
	("equality_checks", equalityChecksAsString())
	("parameters_calldata", typedParametersAsString(CalleeType::EXTERNAL))
	("untyped_parameters", m_untypedParamsExternal.str())
	("indirections", indirections.str())
	.render();

	return calldataHelperFuncs.str();
}

string ProtoConverter::commonHelperFunctions()
{
	stringstream helperFuncs;
	helperFuncs << R"(
	/// Compares bytes, returning true if they are equal and false otherwise.
	function bytesCompare(bytes memory a, bytes memory b) internal pure returns (bool) {
		if(a.length != b.length)
			return false;
		for (uint i = 0; i < a.length; i++)
			if (a[i] != b[i])
				return false;
		return true;
	}
	)";

	return helperFuncs.str();
}

void ProtoConverter::visit(Contract const& _x)
{
	string pragmas = R"(pragma hyperion >=0.0;
pragma experimental ABIEncoderV2;)";

	// Record test spec
	m_test = _x.test();

	// TODO: Support more than one but less than N state variables
	auto storageBuffers = visit(_x.state_vars());
	string storageVarDecls = storageBuffers.first;
	string storageVarDefs = storageBuffers.second;
	m_isStateVar = false;
	string testFunction = visit(_x.testfunction(), storageVarDefs);
	/* Structure of contract body
	 * - Storage variable declarations
	 * - Struct definitions
	 * - Test function
	 *     - Storage variable assignments
	 *     - Local variable definitions and assignments
	 *     - Test code proper (calls public and external functions)
	 * - Helper functions
	 */
	ostringstream contractBody;
	contractBody << storageVarDecls
	             << testFunction
	             << commonHelperFunctions();
	m_output << Whiskers(R"(<pragmas>
<contractStart>
<contractBody>
<contractEnd>)")
		("pragmas", pragmas)
		("contractStart", "contract C {")
		("contractBody", contractBody.str())
		("contractEnd", "}")
		.render();
}

string ProtoConverter::isabelleTypeString() const
{
	string typeString = m_isabelleTypeString.str();
	if (!typeString.empty())
		return "(" + typeString + ")";
	else
		return typeString;
}

string ProtoConverter::isabelleValueString() const
{
	string valueString = m_isabelleValueString.str();
	if (!valueString.empty())
		return "(" + valueString + ")";
	else
		return valueString;
}

string ProtoConverter::contractToString(Contract const& _input)
{
	visit(_input);
	return m_output.str();
}

/// Type visitor
void TypeVisitor::StructTupleString::addTypeStringToTuple(string& _typeString)
{
	index++;
	if (index > 1)
		stream << ",";
	stream << _typeString;
}

void TypeVisitor::StructTupleString::addArrayBracketToType(string& _arrayBracket)
{
	stream << _arrayBracket;
}

string TypeVisitor::visit(BoolType const&)
{
	m_baseType = "bool";
	m_structTupleString.addTypeStringToTuple(m_baseType);
	return m_baseType;
}

string TypeVisitor::visit(IntegerType const& _type)
{
	m_baseType = getIntTypeAsString(_type);
	m_structTupleString.addTypeStringToTuple(m_baseType);
	return m_baseType;
}

string TypeVisitor::visit(FixedByteType const& _type)
{
	m_baseType = getFixedByteTypeAsString(_type);
	m_structTupleString.addTypeStringToTuple(m_baseType);
	return m_baseType;
}

string TypeVisitor::visit(AddressType const&)
{
	m_baseType = "address";
	m_structTupleString.addTypeStringToTuple(m_baseType);
	return m_baseType;
}

string TypeVisitor::visit(ArrayType const& _type)
{
	if (!ValidityVisitor().visit(_type))
		return "";

	string baseType = visit(_type.t());
	hypAssert(!baseType.empty(), "");
	string arrayBracket = _type.is_static() ?
	                     string("[") +
	                     to_string(getStaticArrayLengthFromFuzz(_type.length())) +
	                     string("]") :
	                     string("[]");
	m_baseType += arrayBracket;
	m_structTupleString.addArrayBracketToType(arrayBracket);

	// If we don't know yet if the array will be dynamically encoded,
	// check again. If we already know that it will be, there's no
	// need to do anything.
	if (!m_isLastDynParamRightPadded)
		m_isLastDynParamRightPadded = DynParamVisitor().visit(_type);

	return baseType + arrayBracket;
}

string TypeVisitor::visit(DynamicByteArrayType const&)
{
	m_isLastDynParamRightPadded = true;
	m_baseType = "bytes";
	m_structTupleString.addTypeStringToTuple(m_baseType);
	return m_baseType;
}

void TypeVisitor::structDefinition(StructType const& _type)
{
	// Return an empty string if struct is empty
	hypAssert(ValidityVisitor().visit(_type), "");

	// Reset field counter and indentation
	unsigned wasFieldCounter = m_structFieldCounter;
	unsigned wasIndentation = m_indentation;

	m_indentation = 1;
	m_structFieldCounter = 0;

	// Commence struct declaration
	string structDef = lineString(
		"struct " +
		string(s_structNamePrefix) +
		to_string(m_structCounter) +
		" {"
	);
	// Start tuple of types with parenthesis
	m_structTupleString.start();
	// Increase indentation for struct fields
	m_indentation++;
	for (auto const& t: _type.t())
	{
		string type{};

		if (!ValidityVisitor().visit(t))
			continue;

		TypeVisitor tVisitor(m_structCounter + 1);
		type = tVisitor.visit(t);
		m_structCounter += tVisitor.numStructs();
		m_structDef << tVisitor.structDef();

		hypAssert(!type.empty(), "");

		structDef += lineString(
			Whiskers(R"(<type> <member>;)")
				("type", type)
				("member", "m" + to_string(m_structFieldCounter++))
				.render()
		);
		string isabelleTypeStr = tVisitor.isabelleTypeString();
		m_structTupleString.addTypeStringToTuple(isabelleTypeStr);
	}
	m_indentation--;
	structDef += lineString("}");
	// End tuple of types with parenthesis
	m_structTupleString.end();
	m_structCounter++;
	m_structDef << structDef;
	m_indentation = wasIndentation;
	m_structFieldCounter = wasFieldCounter;
}

string TypeVisitor::visit(StructType const& _type)
{
	if (ValidityVisitor().visit(_type))
	{
		// Add struct definition
		structDefinition(_type);
		// Set last dyn param if struct contains a dyn param e.g., bytes, array etc.
		m_isLastDynParamRightPadded = DynParamVisitor().visit(_type);
		// If top-level struct is a non-empty struct, assign the name S<suffix>
		m_baseType = s_structTypeName + to_string(m_structStartCounter);
	}
	else
		m_baseType = {};

	return m_baseType;
}

/// AssignCheckVisitor implementation
void AssignCheckVisitor::ValueStream::appendValue(string& _value)
{
	hypAssert(!_value.empty(), "Abiv2 fuzzer: Empty value");
	index++;
	if (index > 1)
		stream << ",";
	stream << _value;
}

pair<string, string> AssignCheckVisitor::visit(BoolType const& _type)
{
	string value = ValueGetterVisitor(counter()).visit(_type);
	if (!m_forcedVisit)
		m_valueStream.appendValue(value);
	return assignAndCheckStringPair(m_varName, m_paramName, value, value, DataType::VALUE);
}

pair<string, string> AssignCheckVisitor::visit(IntegerType const& _type)
{
	string value = ValueGetterVisitor(counter()).visit(_type);
	if (!m_forcedVisit)
		m_valueStream.appendValue(value);
	return assignAndCheckStringPair(m_varName, m_paramName, value, value, DataType::VALUE);
}

pair<string, string> AssignCheckVisitor::visit(FixedByteType const& _type)
{
	string value = ValueGetterVisitor(counter()).visit(_type);
	if (!m_forcedVisit)
	{
		string isabelleValue = ValueGetterVisitor{}.isabelleBytesValueAsString(value);
		m_valueStream.appendValue(isabelleValue);
	}
	return assignAndCheckStringPair(m_varName, m_paramName, value, value, DataType::VALUE);
}

pair<string, string> AssignCheckVisitor::visit(AddressType const& _type)
{
	string value = ValueGetterVisitor(counter()).visit(_type);
	if (!m_forcedVisit)
	{
		string isabelleValue = ValueGetterVisitor{}.isabelleAddressValueAsString(value);
		m_valueStream.appendValue(isabelleValue);
	}
	return assignAndCheckStringPair(m_varName, m_paramName, value, value, DataType::VALUE);
}

pair<string, string> AssignCheckVisitor::visit(DynamicByteArrayType const& _type)
{
	string value = ValueGetterVisitor(counter()).visit(_type);
	if (!m_forcedVisit)
	{
		string isabelleValue = ValueGetterVisitor{}.isabelleBytesValueAsString(value);
		m_valueStream.appendValue(isabelleValue);
	}
	DataType dataType = DataType::BYTES;
	return assignAndCheckStringPair(m_varName, m_paramName, value, value, dataType);
}

pair<string, string> AssignCheckVisitor::visit(ArrayType const& _type)
{
	if (!ValidityVisitor().visit(_type))
		return make_pair("", "");

	// Obtain type of array to be resized and initialized
	string typeStr{};

	unsigned wasStructCounter = m_structCounter;
	TypeVisitor tVisitor(m_structCounter);
	typeStr = tVisitor.visit(_type);

	pair<string, string> resizeBuffer;
	string lengthStr;
	unsigned length;

	// Resize dynamic arrays
	if (!_type.is_static())
	{
		length = getDynArrayLengthFromFuzz(_type.length(), counter());
		lengthStr = to_string(length);
		if (m_stateVar)
		{
			// Dynamic storage arrays are resized via the empty push() operation
			resizeBuffer.first = Whiskers(R"(<indentation>for (uint i = 0; i < <length>; i++) <arrayRef>.push();)")
				("indentation", indentation())
				("length", lengthStr)
				("arrayRef", m_varName)
				.render() + "\n";
			// Add a dynamic check on the resized length
			resizeBuffer.second = checkString(m_paramName + ".length", lengthStr, DataType::VALUE);
		}
		else
		{
			// Resizing memory arrays via the new operator
			string resizeOp = Whiskers(R"(new <fullTypeStr>(<length>))")
				("fullTypeStr", typeStr)
				("length", lengthStr)
				.render();
			resizeBuffer = assignAndCheckStringPair(
				m_varName,
				m_paramName + ".length",
				resizeOp,
				lengthStr,
				DataType::VALUE
				);
		}
	}
	else
	{
		length = getStaticArrayLengthFromFuzz(_type.length());
		lengthStr = to_string(length);
		// Add check on length
		resizeBuffer.second = checkString(m_paramName + ".length", lengthStr, DataType::VALUE);
	}

	// Add assignCheckBuffer and check statements
	pair<string, string> assignCheckBuffer;
	string wasVarName = m_varName;
	string wasParamName = m_paramName;
	if (!m_forcedVisit)
		m_valueStream.startArray();
	for (unsigned i = 0; i < length; i++)
	{
		m_varName = wasVarName + "[" + to_string(i) + "]";
		m_paramName = wasParamName + "[" + to_string(i) + "]";
		pair<string, string> assign = visit(_type.t());
		assignCheckBuffer.first += assign.first;
		assignCheckBuffer.second += assign.second;
		if (i < length - 1)
			m_structCounter = wasStructCounter;
	}
	// Since struct visitor won't be called for zero-length
	// arrays, struct counter will not get incremented. Therefore,
	// we need to manually force a recursive struct visit.
	if (length == 0 && TypeVisitor().arrayOfStruct(_type))
	{
		bool previousState = m_forcedVisit;
		m_forcedVisit = true;
		visit(_type.t());
		m_forcedVisit = previousState;
	}
	if (!m_forcedVisit)
		m_valueStream.endArray();

	m_varName = wasVarName;
	m_paramName = wasParamName;

	// Compose resize and initialization assignment and check
	return make_pair(
		resizeBuffer.first + assignCheckBuffer.first,
		resizeBuffer.second + assignCheckBuffer.second
	);
}

pair<string, string> AssignCheckVisitor::visit(StructType const& _type)
{
	if (!ValidityVisitor().visit(_type))
		return make_pair("", "");

	pair<string, string> assignCheckBuffer;
	unsigned i = 0;

	// Increment struct counter
	m_structCounter++;

	string wasVarName = m_varName;
	string wasParamName = m_paramName;

	if (!m_forcedVisit)
		m_valueStream.startStruct();
	for (auto const& t: _type.t())
	{
		m_varName = wasVarName + ".m" + to_string(i);
		m_paramName = wasParamName + ".m" + to_string(i);
		pair<string, string> assign = visit(t);
		// If type is not well formed continue without
		// updating state.
		if (assign.first.empty() && assign.second.empty())
			continue;
		assignCheckBuffer.first += assign.first;
		assignCheckBuffer.second += assign.second;
		i++;
	}
	if (!m_forcedVisit)
		m_valueStream.endStruct();
	m_varName = wasVarName;
	m_paramName = wasParamName;
	return assignCheckBuffer;
}

pair<string, string> AssignCheckVisitor::assignAndCheckStringPair(
	string const& _varRef,
	string const& _checkRef,
	string const& _assignValue,
	string const& _checkValue,
	DataType _type
)
{
	return make_pair(assignString(_varRef, _assignValue), checkString(_checkRef, _checkValue, _type));
}

string AssignCheckVisitor::assignString(string const& _ref, string const& _value)
{
	string assignStmt = Whiskers(R"(<ref> = <value>;)")
		("ref", _ref)
		("value", _value)
		.render();
	return indentation() + assignStmt + "\n";
}

string AssignCheckVisitor::checkString(string const& _ref, string const& _value, DataType _type)
{
	string checkPred;
	switch (_type)
	{
	case DataType::BYTES:
		checkPred = Whiskers(R"(!bytesCompare(<varName>, <value>))")
			("varName", _ref)
			("value", _value)
			.render();
		break;
	case DataType::VALUE:
		checkPred = Whiskers(R"(<varName> != <value>)")
			("varName", _ref)
			("value", _value)
			.render();
		break;
	case DataType::ARRAY:
		hypUnimplemented("Proto ABIv2 fuzzer: Invalid data type.");
	}
	string checkStmt = Whiskers(R"(if (<checkPred>) return <errCode>;)")
		("checkPred", checkPred)
		("errCode", to_string(m_errorCode++))
		.render();
	return indentation() + checkStmt + "\n";
}

/// ValueGetterVisitor
string ValueGetterVisitor::visit(BoolType const&)
{
	return counter() % 2 ? "true" : "false";
}

string ValueGetterVisitor::visit(IntegerType const& _type)
{
	return integerValue(counter(), getIntWidth(_type), _type.is_signed());
}

string ValueGetterVisitor::visit(FixedByteType const& _type)
{
	return fixedByteValueAsString(
		getFixedByteWidth(_type),
		counter()
	);
}

string ValueGetterVisitor::visit(AddressType const&)
{
	return addressValueAsString(counter());
}

string ValueGetterVisitor::visit(DynamicByteArrayType const&)
{
	return bytesArrayValueAsString(
		counter(),
		true
	);
}

std::string ValueGetterVisitor::croppedString(
	unsigned _numBytes,
	unsigned _counter,
	bool _isHexLiteral
)
{
	hypAssert(
		_numBytes > 0 && _numBytes <= 32,
		"Proto ABIv2 fuzzer: Too short or too long a cropped string"
	);

	// Number of masked nibbles is twice the number of bytes for a
	// hex literal of _numBytes bytes. For a string literal, each nibble
	// is treated as a character.
	unsigned numMaskNibbles = _isHexLiteral ? _numBytes * 2 : _numBytes;

	// Start position of substring equals totalHexStringLength - numMaskNibbles
	// totalHexStringLength = 64 + 2 = 66
	// e.g., 0x12345678901234567890123456789012 is a total of 66 characters
	//      |---------------------^-----------|
	//      <--- start position---><--numMask->
	//      <-----------total length --------->
	// Note: This assumes that maskUnsignedIntToHex() invokes toHex(..., HexPrefix::Add)
	unsigned startPos = 66 - numMaskNibbles;
	// Extracts the least significant numMaskNibbles from the result
	// of maskUnsignedIntToHex().
	return maskUnsignedIntToHex(
		_counter,
		numMaskNibbles
	).substr(startPos, numMaskNibbles);
}

std::string ValueGetterVisitor::hexValueAsString(
	unsigned _numBytes,
	unsigned _counter,
	bool _isHexLiteral,
	bool _decorate
)
{
	hypAssert(_numBytes > 0 && _numBytes <= 32,
	          "Proto ABIv2 fuzzer: Invalid hex length"
	);

	// If _decorate is set, then we return a hex"" or a "" string.
	if (_numBytes == 0)
		return Whiskers(R"(<?decorate><?isHex>hex</isHex>""</decorate>)")
			("decorate", _decorate)
			("isHex", _isHexLiteral)
			.render();

	// NOTE(rgeraldes24): this is no longer the case but the code is fine
	// This is needed because hyperion interprets a 20-byte 0x prefixed hex literal as an address
	// payable type.
	return Whiskers(R"(<?decorate><?isHex>hex</isHex>"</decorate><value><?decorate>"</decorate>)")
		("decorate", _decorate)
		("isHex", _isHexLiteral)
		("value", croppedString(_numBytes, _counter, _isHexLiteral))
		.render();
}

std::string ValueGetterVisitor::fixedByteValueAsString(unsigned _width, unsigned _counter)
{
	hypAssert(
		(_width >= 1 && _width <= 32),
		"Proto ABIv2 Fuzzer: Fixed byte width is not between 1--32"
	);
	return hexValueAsString(_width, _counter, /*isHexLiteral=*/true);
}

std::string ValueGetterVisitor::addressValueAsString(unsigned _counter)
{
	return "address(" + maskUnsignedIntToHex(_counter, 40) + ")";
}

// TODO(now.youtrack.cloud/issue/TS-18)
std::string ValueGetterVisitor::isabelleAddressValueAsString(std::string& _hypAddressString)
{
	// Isabelle encoder expects address literal to be exactly
	// 20 bytes and a hex string.
	// Example: 0x0102030405060708090a0102030405060708090a
	std::regex const addressPattern("address\\((.*)\\)");
	std::smatch match;
	hypAssert(std::regex_match(_hypAddressString, match, addressPattern), "Abiv2 fuzzer: Invalid address string");
	std::string addressHex = match[1].str();
	addressHex.erase(2, 24);
	return addressHex;
}

std::string ValueGetterVisitor::isabelleBytesValueAsString(std::string& _hypBytesString)
{
	std::regex const bytesPattern("hex\"(.*)\"");
	std::smatch match;
	hypAssert(std::regex_match(_hypBytesString, match, bytesPattern), "Abiv2 fuzzer: Invalid bytes string");
	std::string bytesHex = match[1].str();
	return "0x" + bytesHex;
}

std::string ValueGetterVisitor::variableLengthValueAsString(
	unsigned _numBytes,
	unsigned _counter,
	bool _isHexLiteral
)
{
	if (_numBytes == 0)
		return Whiskers(R"(<?isHex>hex</isHex>"")")
			("isHex", _isHexLiteral)
			.render();

	unsigned numBytesRemaining = _numBytes;
	// Stores the literal
	string output{};
	// If requested value is shorter than or exactly 32 bytes,
	// the literal is the return value of hexValueAsString.
	if (numBytesRemaining <= 32)
		output = hexValueAsString(
			numBytesRemaining,
			_counter,
			_isHexLiteral,
			/*decorate=*/false
		);
		// If requested value is longer than 32 bytes, the literal
		// is obtained by duplicating the return value of hexValueAsString
		// until we reach a value of the requested size.
	else
	{
		// Create a 32-byte value to be duplicated and
		// update number of bytes to be appended.
		// Stores the cached literal that saves us
		// (expensive) calls to keccak256.
		string cachedString = hexValueAsString(
			/*numBytes=*/32,
			             _counter,
			             _isHexLiteral,
			/*decorate=*/false
		);
		output = cachedString;
		numBytesRemaining -= 32;

		// Append bytes from cachedString until
		// we create a value of desired length.
		unsigned numAppendedBytes;
		while (numBytesRemaining > 0)
		{
			// We append at most 32 bytes at a time
			numAppendedBytes = numBytesRemaining >= 32 ? 32 : numBytesRemaining;
			output += cachedString.substr(
				0,
				// Double the substring length for hex literals since each
				// character is actually half a byte (or a nibble).
				_isHexLiteral ? numAppendedBytes * 2 : numAppendedBytes
			);
			numBytesRemaining -= numAppendedBytes;
		}
		hypAssert(
			numBytesRemaining == 0,
			"Proto ABIv2 fuzzer: Logic flaw in variable literal creation"
		);
	}

	if (_isHexLiteral)
		hypAssert(
			output.size() == 2 * _numBytes,
			"Proto ABIv2 fuzzer: Generated hex literal is of incorrect length"
		);
	else
		hypAssert(
			output.size() == _numBytes,
			"Proto ABIv2 fuzzer: Generated string literal is of incorrect length"
		);

	// Decorate output
	return Whiskers(R"(<?isHexLiteral>hex</isHexLiteral>"<value>")")
		("isHexLiteral", _isHexLiteral)
		("value", output)
		.render();
}

string ValueGetterVisitor::bytesArrayValueAsString(unsigned _counter, bool _isHexLiteral)
{
	return variableLengthValueAsString(
		getVarLength(_counter),
		_counter,
		_isHexLiteral
	);
}
