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

#include <libqrvmasm/Instruction.h>
#include <liblangutil/SourceLocation.h>
#include <libqrvmasm/AssemblyItem.h>
#include <libqrvmasm/LinkerObject.h>
#include <libqrvmasm/Exceptions.h>

#include <liblangutil/DebugInfoSelection.h>
#include <liblangutil/QRVMVersion.h>

#include <libhyputil/Common.h>
#include <libhyputil/Assertions.h>
#include <libhyputil/Keccak256.h>

#include <libhyperion/interface/OptimiserSettings.h>

#include <json/json.h>

#include <iostream>
#include <sstream>
#include <memory>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace hyperion::qrvmasm
{

using AssemblyPointer = std::shared_ptr<Assembly>;
using CodeCopyTaintByteRanges = std::vector<std::pair<size_t, size_t>>;
using CodeCopyTaintForeignTagRanges = std::vector<std::tuple<size_t, size_t, size_t, size_t>>;
using CodeCopyTaintLocalTagRanges = std::vector<std::tuple<size_t, size_t, size_t>>;

struct CodeCopyTaintRanges
{
	CodeCopyTaintByteRanges foreignReferences;
	CodeCopyTaintForeignTagRanges foreignTags;
	CodeCopyTaintByteRanges localTags;
	CodeCopyTaintLocalTagRanges localTagReferences;
	CodeCopyTaintByteRanges currentAddresses;
	CodeCopyTaintByteRanges complementedCurrentAddresses;

	void clear()
	{
		foreignReferences.clear();
		foreignTags.clear();
		localTags.clear();
		localTagReferences.clear();
		currentAddresses.clear();
		complementedCurrentAddresses.clear();
	}
};

class Assembly
{
public:
	Assembly(langutil::QRVMVersion _qrvmVersion, bool _creation, std::string _name): m_qrvmVersion(_qrvmVersion), m_creation(_creation), m_name(std::move(_name)) { }
	AssemblyPointer clone() const;

	AssemblyItem newTag()
	{
		assertMutable();
		assertThrow(m_usedTags < 0xffffffff, AssemblyException, "");
		AssemblyItem item(Tag, m_usedTags);
		++m_usedTags;
		return item;
	}
	AssemblyItem newPushTag()
	{
		assertMutable();
		assertThrow(m_usedTags < 0xffffffff, AssemblyException, "");
		AssemblyItem item(PushTag, m_usedTags);
		++m_usedTags;
		return item;
	}
	/// Returns a tag identified by the given name. Creates it if it does not yet exist.
	AssemblyItem namedTag(std::string const& _name, size_t _params, size_t _returns, std::optional<uint64_t> _sourceID);
	AssemblyItem newData(bytes const& _data)
	{
		assertMutable();
		util::h256 h(util::keccak256(util::asString(_data)));
		AssemblyItem item(PushData, u512(u256(h)));
		auto [data, inserted] = m_data.emplace(h, _data);
		assertThrow(inserted || data->second == _data, AssemblyException, "Data hash mismatch.");
		return item;
	}
	bytes const& data(util::h256 const& _i) const
	{
		auto data = m_data.find(_i);
		assertThrow(data != m_data.end(), AssemblyException, "Reference to non-existing data.");
		return data->second;
	}
	AssemblyItem newSub(AssemblyPointer const& _sub);
	Assembly const& sub(size_t _sub) const
	{
		assertThrow(_sub < m_subs.size(), AssemblyException, "Reference to non-existing subassembly.");
		auto const& sub = m_subs[_sub];
		assertThrow(sub, AssemblyException, "Invalid sub-assembly.");
		return *sub;
	}
	Assembly& sub(size_t _sub)
	{
		assertMutable();
		assertThrow(_sub < m_subs.size(), AssemblyException, "Reference to non-existing subassembly.");
		auto const& sub = m_subs[_sub];
		assertThrow(sub, AssemblyException, "Invalid sub-assembly.");
		return *sub;
	}
	size_t numSubs() const { return m_subs.size(); }
	AssemblyItem newPushSubSize(u512 const& _subId) { return AssemblyItem(PushSubSize, _subId); }
	AssemblyItem newPushLibraryAddress(std::string const& _identifier);
	AssemblyItem newPushImmutable(std::string const& _identifier);
	AssemblyItem newImmutableAssignment(std::string const& _identifier);

	AssemblyItem const& append(AssemblyItem _i);
	AssemblyItem const& append(Instruction _i) { return append(AssemblyItem(_i)); }
	AssemblyItem const& append(AssemblyItemType _type) { return append(AssemblyItem(_type)); }
	AssemblyItem const& append(bytes const& _data);
	AssemblyItem const& append(int _value) { return append(AssemblyItem(u512(_value))); }
	AssemblyItem const& append(unsigned _value) { return append(AssemblyItem(u512(_value))); }
	AssemblyItem const& append(long _value) { return append(AssemblyItem(u512(_value))); }
	AssemblyItem const& append(unsigned long _value) { return append(AssemblyItem(u512(_value))); }
	AssemblyItem const& append(u256 const& _value) { return append(AssemblyItem(u512(_value))); }
	AssemblyItem const& append(u512 const& _value) { return append(AssemblyItem(_value)); }

	template <class T> Assembly& operator<<(T const& _d) { append(_d); return *this; }

	/// Pushes the final size of the current assembly itself. Use this when the code is modified
	/// after compilation and CODESIZE is not an option.
	void appendProgramSize() { append(AssemblyItem(PushProgramSize)); }
	void appendLibraryAddress(std::string const& _identifier);
	void appendImmutable(std::string const& _identifier);
	void appendImmutableAssignment(std::string const& _identifier);

	void appendVerbatim(bytes _data, size_t _arguments, size_t _returnVariables)
	{
		append(AssemblyItem(std::move(_data), _arguments, _returnVariables));
	}

	AssemblyItem appendJump();
	AssemblyItem appendJumpI();
	AssemblyItem appendJump(AssemblyItem const& _tag);
	AssemblyItem appendJumpI(AssemblyItem const& _tag);

	/// Adds a subroutine to the code (in the data section) and pushes its size (via a tag)
	/// on the stack. @returns the pushsub assembly item.
	AssemblyItem appendSubroutine(AssemblyPointer const& _assembly);
	void pushSubroutineSize(size_t _subRoutine) { append(newPushSubSize(_subRoutine)); }
	/// Pushes the offset of the subroutine.
	void pushSubroutineOffset(size_t _subRoutine) { append(AssemblyItem(PushSub, _subRoutine)); }

	/// Appends @a _data literally to the very end of the bytecode.
	void appendToAuxiliaryData(bytes const& _data) { assertMutable(); m_auxiliaryData += _data; }

	/// Returns the assembly items.
	AssemblyItems const& items() const { return m_items; }

	/// Returns the mutable assembly items. Use with care!
	AssemblyItems& items() { assertMutable(); return m_items; }

	static constexpr size_t StackLimit = 1024;
	int deposit() const { return m_deposit; }
	void adjustDeposit(int _adjustment) { m_deposit += _adjustment; assertThrow(m_deposit >= 0, InvalidDeposit, ""); }
	void setDeposit(int _deposit) { m_deposit = _deposit; assertThrow(m_deposit >= 0, InvalidDeposit, ""); }
	std::string const& name() const { return m_name; }

	/// Changes the source location used for each appended item.
	void setSourceLocation(langutil::SourceLocation const& _location) { m_currentSourceLocation = _location; }
	langutil::SourceLocation const& currentSourceLocation() const { return m_currentSourceLocation; }
	langutil::QRVMVersion const& qrvmVersion() const { return m_qrvmVersion; }

	/// Assembles the assembly into bytecode. The assembly should not be modified after this call, since the assembled version is cached.
	LinkerObject const& assemble() const;
	/// Assembles a code fragment that expects @a _initialStackHeight values to exist below its first item.
	LinkerObject const& assembleWithInitialStackHeight(size_t _initialStackHeight) const;
	/// Assembles a deploy-time address template. This is only valid for the runtime sub-assembly
	/// whose leading PUSHDEPLOYADDRESS placeholder is patched by creation code before deployment.
	LinkerObject const& assembleDeployTimeAddressTemplate() const;
	/// Allows a direct runtime sub-assembly to contain the deploy-time address placeholder.
	/// The caller must patch that placeholder before the sub-assembly can be deployed.
	void markDeployTimeAddressSubAssembly(size_t _subIdPath);
	/// Requires immutable references in the direct runtime sub-assembly to be assigned by creation code
	/// even if the optimizer removes the runtime copy/return path from emitted creation bytecode.
	void markImmutableValidationSubAssembly(size_t _subIdPath);

	struct OptimiserSettings
	{
		bool runInliner = false;
		bool runJumpdestRemover = false;
		bool runPeephole = false;
		bool runDeduplicate = false;
		bool runCSE = false;
		bool runConstantOptimiser = false;
		langutil::QRVMVersion qrvmVersion;
		/// This specifies an estimate on how often each opcode in this assembly will be executed,
		/// i.e. use a small value to optimise for size and a large value to optimise for runtime gas usage.
		size_t expectedExecutionsPerDeployment = frontend::OptimiserSettings{}.expectedExecutionsPerDeployment;

		static OptimiserSettings translateSettings(frontend::OptimiserSettings const& _settings, langutil::QRVMVersion const& _qrvmVersion);
	};

	/// Modify and return the current assembly such that creation and execution gas usage
	/// is optimised according to the settings in @a _settings.
	Assembly& optimise(OptimiserSettings const& _settings);

	/// Create a text representation of the assembly.
	std::string assemblyString(
		langutil::DebugInfoSelection const& _debugInfoSelection = langutil::DebugInfoSelection::Default(),
		StringMap const& _sourceCodes = StringMap()
	) const;
	void assemblyStream(
		std::ostream& _out,
		langutil::DebugInfoSelection const& _debugInfoSelection = langutil::DebugInfoSelection::Default(),
		std::string const& _prefix = "",
		StringMap const& _sourceCodes = StringMap()
	) const;

	/// Create a JSON representation of the assembly.
	Json::Value assemblyJSON(std::map<std::string, unsigned> const& _sourceIndices, bool _includeSourceList = true) const;

	/// Constructs an @a Assembly from the serialized JSON representation.
	/// @param _json JSON object containing assembly in the format produced by assemblyJSON().
	/// @param _sourceList Internal recursion parameter. Callers must leave it empty.
	/// @param _level Internal recursion parameter. Callers must leave it at zero.
	/// @returns Created @a Assembly and the source list read from the 'sourceList' field of the root
	///     assembly.
	static std::pair<std::shared_ptr<Assembly>, std::vector<std::string>> fromJSON(
		Json::Value const& _json,
		std::vector<std::string> const& _sourceList = {},
		size_t _level = 0
	);

	/// Mark this assembly as invalid. Calling ``assemble`` on it will throw.
	void markAsInvalid() { m_invalid = true; }

	std::vector<size_t> decodeSubPath(size_t _subObjectId) const;
	size_t encodeSubPath(std::vector<size_t> const& _subPath);

	bool isCreation() const { return m_creation; }

protected:
	using SubAssemblyTagReferences = std::map<std::vector<size_t>, std::set<size_t>>;

	/// Does the same operations as @a optimise, but should only be applied to a sub and
	/// returns the replaced tags. Also takes an argument containing the tags of this assembly
	/// that are referenced in a super-assembly.
	std::map<u512, u512> const& optimiseInternal(
		OptimiserSettings const& _settings,
		std::set<size_t> _tagsReferencedFromOutside,
		SubAssemblyTagReferences _subTagsReferencedFromOutside = {},
		std::set<size_t> _copiedTagsReferencedFromOutside = {},
		SubAssemblyTagReferences _copiedSubTagsReferencedFromOutside = {}
	);

	size_t codeSize(
		unsigned _tagSize,
		unsigned _dataRefSize,
		unsigned _programSizeRefSize,
		Precision _precision
	) const;
	size_t legacyCodeSizeLowerBound(unsigned _subTagSize) const;

	/// Add all assembly items from given JSON array. This function imports the items by iterating through
	/// the code array. This method only works on clean Assembly objects that don't have any items defined yet.
	/// @param _json JSON array that contains assembly items (e.g. json['.code'])
	/// @param _sourceList List of source names.
	void importAssemblyItemsFromJSON(Json::Value const& _code, std::vector<std::string> const& _sourceList);

	/// Creates an AssemblyItem from a given JSON representation.
	/// @param _json JSON object that consists a single assembly item
	/// @param _sourceList List of source names.
	/// @returns AssemblyItem of _json argument.
	AssemblyItem createAssemblyItemFromJSON(Json::Value const& _json, std::vector<std::string> const& _sourceList);

private:
	bool m_invalid = false;

	LinkerObject const& assemble(
		std::set<size_t> _tagsReferencedFromOutside,
		SubAssemblyTagReferences _subTagsReferencedFromOutside,
		bool _allowDeployTimeAddress,
		std::set<size_t> _copiedTagsReferencedFromOutside = {},
		SubAssemblyTagReferences _copiedSubTagsReferencedFromOutside = {},
		size_t _initialStackHeight = 0
	) const;
	static std::pair<std::shared_ptr<Assembly>, std::vector<std::string>> fromJSONInternal(
		Json::Value const& _json,
		std::vector<std::string> const& _sourceList,
		size_t _level
	);
	Json::Value assemblyJSON(
		std::map<std::string, unsigned> const& _sourceIndices,
		bool _includeSourceList,
		bool _isRoot
	) const;
	bool allowsDeployTimeAddressInSubAssembly(size_t _subIdPath) const;
	void assertValidDeployTimeAddressSubAssembly(size_t _subIdPath) const;
	void assertValidImmutableValidationSubAssembly(size_t _subIdPath) const;
	Assembly const* subAssemblyById(size_t _subId) const;
	std::optional<size_t> subPathId(std::vector<size_t> const& _subPath) const;
	bool declaresTag(size_t _tagId) const;
	void assertUniqueTagDeclarations() const;
	void assertValidDataSection() const;
	void assertResolvableItemReferences(AssemblyItem const& _item) const;
	void assertValidNamedTagMetadata() const;
	bool containsAssembly(Assembly const* _assembly, std::set<Assembly const*>& _visited) const;
	void validateSubReferences() const;
	void assertValidSubPathMap() const;
	void assertValidSubAssemblyTree() const;
	void assertValidSubAssemblyTree(
		std::set<Assembly const*>& _assembliesOnPath,
		std::set<Assembly const*>& _assembliesInTree,
		size_t _depth
	) const;
	std::optional<size_t> canonicalSubPathId(std::vector<size_t> const& _subPath) const;
	std::optional<std::vector<size_t>> canonicalSubPath(size_t _subObjectId) const;
		void assertMutable() const
		{
		}

	void encodeAllPossibleSubPathsInAssemblyTree(std::vector<size_t> _pathFromRoot = {}, std::vector<Assembly*> _assembliesOnPath = {});

	std::shared_ptr<std::string const> sharedSourceName(std::string const& _name) const;

protected:
	/// 0 is reserved for exception
	unsigned m_usedTags = 1;

	struct NamedTagInfo
	{
		size_t id;
		std::optional<size_t> sourceID;
		size_t params;
		size_t returns;
	};

	std::map<std::string, NamedTagInfo> m_namedTags;
	AssemblyItems m_items;
	std::map<util::h256, bytes> m_data;
	/// Data that is appended to the very end of the contract.
	bytes m_auxiliaryData;
	std::vector<std::shared_ptr<Assembly>> m_subs;
	std::map<util::h256, std::string> m_strings;
	std::map<util::h256, std::string> m_libraries; ///< Identifiers of libraries to be linked.
	std::map<util::h256, std::string> m_immutables; ///< Identifiers of immutables.

	/// Map from a vector representing a path to a particular sub assembly to sub assembly id.
	/// This map is used only for sub-assemblies which are not direct sub-assemblies (where path is having more than one value).
	mutable std::map<std::vector<size_t>, size_t> m_subPaths;

	/// Contains the tag replacements relevant for super-assemblies.
	/// If set, it means the optimizer has run and we will not run it again.
	std::optional<std::map<u512, u512>> m_tagReplacements;
	std::optional<OptimiserSettings> m_optimiserSettings;
	std::optional<std::set<size_t>> m_tagsReferencedFromOutside;
	std::optional<SubAssemblyTagReferences> m_subTagsReferencedFromOutside;
	std::optional<std::set<size_t>> m_copiedTagsReferencedFromOutside;
	std::optional<SubAssemblyTagReferences> m_copiedSubTagsReferencedFromOutside;

	mutable LinkerObject m_assembledObject;
	mutable std::map<size_t, size_t> m_tagPositionsInBytecode;
	mutable std::map<size_t, size_t> m_literalJumpTargetsInBytecode;
	mutable CodeCopyTaintRanges m_codeCopyTaintRangesInBytecode;
	mutable bool m_assembled = false;
	mutable size_t m_assembledInitialStackHeight = 0;

	langutil::QRVMVersion m_qrvmVersion;

	int m_deposit = 0;
	/// True, if the assembly contains contract creation code.
	bool const m_creation = false;
	std::set<size_t> m_deployTimeAddressSubAssemblies;
	std::set<size_t> m_immutableValidationSubAssemblies;
	/// Internal name of the assembly object, only used with the Yul backend
	/// currently
	std::string m_name;
	langutil::SourceLocation m_currentSourceLocation;
	mutable std::map<std::string, std::shared_ptr<std::string const>> m_sharedSourceNames;

public:
	size_t m_currentModifierDepth = 0;
};

inline std::ostream& operator<<(std::ostream& _out, Assembly const& _a)
{
	_a.assemblyStream(_out);
	return _out;
}

}
