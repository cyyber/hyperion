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

#include <liblangutil/CharStreamProvider.h>
#include <liblangutil/Exceptions.h>
#include <libhyperion/ast/AST.h>
#include <libhyperion/lsp/FileRepository.h>
#include <libhyperion/lsp/Utils.h>

#include <algorithm>
#include <regex>
#include <fstream>

namespace hyperion::lsp
{

using namespace frontend;
using namespace langutil;

namespace
{

struct UTF8Character
{
	size_t byteLength;
	int utf16Length;
};

bool isUTF8ContinuationByte(unsigned char _byte)
{
	return (_byte & 0xc0) == 0x80;
}

UTF8Character nextUTF8Character(std::string const& _source, size_t _offset, size_t _end)
{
	unsigned char const firstByte = static_cast<unsigned char>(_source[_offset]);
	size_t const remaining = _end - _offset;

	if (firstByte < 0x80)
		return {1, 1};
	if (
		(firstByte & 0xe0) == 0xc0 &&
		remaining >= 2 &&
		isUTF8ContinuationByte(static_cast<unsigned char>(_source[_offset + 1]))
	)
		return {2, 1};
	if (
		(firstByte & 0xf0) == 0xe0 &&
		remaining >= 3 &&
		isUTF8ContinuationByte(static_cast<unsigned char>(_source[_offset + 1])) &&
		isUTF8ContinuationByte(static_cast<unsigned char>(_source[_offset + 2]))
	)
		return {3, 1};
	if (
		(firstByte & 0xf8) == 0xf0 &&
		remaining >= 4 &&
		isUTF8ContinuationByte(static_cast<unsigned char>(_source[_offset + 1])) &&
		isUTF8ContinuationByte(static_cast<unsigned char>(_source[_offset + 2])) &&
		isUTF8ContinuationByte(static_cast<unsigned char>(_source[_offset + 3]))
	)
		return {4, 2};

	return {1, 1};
}

std::optional<std::pair<size_t, size_t>> lineStartAndEnd(std::string const& _source, int _line)
{
	if (_line < 0)
		return std::nullopt;

	size_t lineStart = 0;
	for (int i = 0; i < _line; ++i)
	{
		lineStart = _source.find('\n', lineStart);
		if (lineStart == std::string::npos)
			return std::nullopt;
		++lineStart;
	}

	size_t lineEnd = _source.find('\n', lineStart);
	if (lineEnd == std::string::npos)
		lineEnd = _source.size();

	return std::make_pair(lineStart, lineEnd);
}

std::optional<int> lspLineColumnToByteOffset(std::string const& _source, LineColumn const& _position)
{
	if (_position.line < 0 || _position.column < 0)
		return std::nullopt;

	std::optional<std::pair<size_t, size_t>> const lineBounds = lineStartAndEnd(_source, _position.line);
	if (!lineBounds)
		return std::nullopt;

	size_t byteOffset = lineBounds->first;
	int utf16Column = 0;
	while (byteOffset < lineBounds->second)
	{
		if (utf16Column == _position.column)
			return static_cast<int>(byteOffset);

		UTF8Character const character = nextUTF8Character(_source, byteOffset, lineBounds->second);
		if (utf16Column + character.utf16Length > _position.column)
			return std::nullopt;
		utf16Column += character.utf16Length;
		byteOffset += character.byteLength;
	}

	if (utf16Column == _position.column)
		return static_cast<int>(byteOffset);
	return std::nullopt;
}

} // end anonymous namespace

std::optional<LineColumn> parseLineColumn(Json::Value const& _lineColumn)
{
	if (_lineColumn.isObject() && _lineColumn["line"].isInt() && _lineColumn["character"].isInt())
	{
		if (_lineColumn["line"].asInt() < 0 || _lineColumn["character"].asInt() < 0)
			return std::nullopt;
		return LineColumn{_lineColumn["line"].asInt(), _lineColumn["character"].asInt()};
	}
	else
		return std::nullopt;
}

LineColumn byteOffsetToByteLineColumn(std::string const& _source, int _position)
{
	using size_type = std::string::size_type;
	size_type searchPosition = std::min<size_type>(_source.size(), size_type(std::max(_position, 0)));
	int lineNumber = static_cast<int>(std::count(_source.begin(), _source.begin() + static_cast<std::string::difference_type>(searchPosition), '\n'));
	size_type lineStart = 0;
	if (searchPosition != 0)
	{
		lineStart = _source.rfind('\n', searchPosition - 1);
		lineStart = lineStart == std::string::npos ? 0 : lineStart + 1;
	}
	return LineColumn{lineNumber, static_cast<int>(searchPosition - lineStart)};
}

Json::Value toJson(LineColumn const& _pos)
{
	Json::Value json = Json::objectValue;
	json["line"] = std::max(_pos.line, 0);
	json["character"] = std::max(_pos.column, 0);

	return json;
}

LineColumn byteOffsetToLSPLineColumn(std::string const& _source, int _position)
{
	using size_type = std::string::size_type;
	size_type searchPosition = std::min<size_type>(_source.size(), size_type(std::max(_position, 0)));
	int lineNumber = static_cast<int>(std::count(_source.begin(), _source.begin() + static_cast<std::string::difference_type>(searchPosition), '\n'));
	size_type lineStart = 0;
	if (searchPosition != 0)
	{
		lineStart = _source.rfind('\n', searchPosition - 1);
		lineStart = lineStart == std::string::npos ? 0 : lineStart + 1;
	}

	int utf16Column = 0;
	size_t byteOffset = lineStart;
	while (byteOffset < searchPosition)
	{
		UTF8Character const character = nextUTF8Character(_source, byteOffset, searchPosition);
		utf16Column += character.utf16Length;
		byteOffset += character.byteLength;
	}

	return LineColumn{lineNumber, utf16Column};
}

Json::Value toJsonRange(LineColumn const& _start, LineColumn const& _end)
{
	Json::Value json;
	json["start"] = toJson(_start);
	json["end"] = toJson(_end);
	return json;
}

Json::Value sourceLocationToJsonRange(std::string const& _source, SourceLocation const& _location)
{
	if (!_location.hasText())
		return toJsonRange({}, {});
	return toJsonRange(
		byteOffsetToLSPLineColumn(_source, _location.start),
		byteOffsetToLSPLineColumn(_source, _location.end)
	);
}

Declaration const* referencedDeclaration(Expression const* _expression)
{
	if (auto const* identifier = dynamic_cast<Identifier const*>(_expression))
		if (Declaration const* referencedDeclaration = identifier->annotation().referencedDeclaration)
			return referencedDeclaration;

	if (auto const* memberAccess = dynamic_cast<MemberAccess const*>(_expression))
		if (memberAccess->annotation().referencedDeclaration)
			return memberAccess->annotation().referencedDeclaration;

	return nullptr;
}

std::optional<SourceLocation> declarationLocation(Declaration const* _declaration)
{
	if (!_declaration)
		return std::nullopt;

	if (_declaration->nameLocation().isValid())
		return _declaration->nameLocation();

	if (_declaration->location().isValid())
		return _declaration->location();

	return std::nullopt;
}

std::optional<SourceLocation> parsePosition(
	FileRepository const& _fileRepository,
	std::string const& _sourceUnitName,
	Json::Value const& _position
)
{
	if (!_fileRepository.sourceUnits().count(_sourceUnitName))
		return std::nullopt;

	if (std::optional<LineColumn> lineColumn = parseLineColumn(_position))
		if (std::optional<int> const offset = lspLineColumnToByteOffset(_fileRepository.sourceUnits().at(_sourceUnitName), *lineColumn))
			return SourceLocation{*offset, *offset, std::make_shared<std::string>(_sourceUnitName)};
	return std::nullopt;
}

std::optional<SourceLocation> parseRange(FileRepository const& _fileRepository, std::string const& _sourceUnitName, Json::Value const& _range)
{
	if (!_range.isObject())
		return std::nullopt;
	std::optional<SourceLocation> start = parsePosition(_fileRepository, _sourceUnitName, _range["start"]);
	std::optional<SourceLocation> end = parsePosition(_fileRepository, _sourceUnitName, _range["end"]);
	if (!start || !end)
		return std::nullopt;
	hypAssert(*start->sourceName == *end->sourceName);
	start->end = end->end;
	return start;
}

std::string stripFileUriSchemePrefix(std::string const& _path)
{
	std::regex const windowsDriveLetterPath("^file:///[a-zA-Z]:/");
	if (regex_search(_path, windowsDriveLetterPath))
		return _path.substr(8);
	if (_path.find("file://") == 0)
		return _path.substr(7);
	else
		return _path;
}

}
