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
#include <libhyperion/lsp/Transport.h>
#include <libhyperion/lsp/Utils.h>

#include <libhyputil/JSON.h>
#include <libhyputil/Visitor.h>
#include <libhyputil/CommonIO.h>
#include <liblangutil/Exceptions.h>

#include <fmt/format.h>

#include <boost/algorithm/string.hpp>

#include <charconv>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>


#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif

using namespace hyperion::lsp;

namespace
{

constexpr size_t c_maxContentLength = 16 * 1024 * 1024;
constexpr size_t c_maxHeaderLineLength = 8 * 1024;
constexpr size_t c_maxHeaderBytes = 64 * 1024;
constexpr size_t c_maxHeaderCount = 64;

std::optional<size_t> parseContentLength(std::string const& _value)
{
	if (_value.empty())
		return std::nullopt;

	size_t length = 0;
	char const* begin = _value.data();
	char const* end = begin + _value.size();
	auto const [ptr, ec] = std::from_chars(begin, end, length);
	if (ec != std::errc{} || ptr != end)
		return std::nullopt;
	return length;
}

std::optional<std::string> readLineFromStream(std::istream& _input, size_t _maxLength)
{
	std::string line;
	char c = 0;
	while (_input.get(c))
	{
		if (c == '\n')
			return line;
		if (line.size() >= _maxLength)
			return std::nullopt;
		line += c;
	}
	return line;
}

}

// {{{ Transport
std::optional<Json::Value> Transport::receive()
{
	auto const headers = parseHeaders();
	if (!headers)
	{
		error({}, ErrorCode::ParseError, "Could not parse RPC headers.");
		return std::nullopt;
	}

	if (!headers->count("content-length"))
	{
		error({}, ErrorCode::ParseError, "No content-length header found.");
		return std::nullopt;
	}

	auto const contentLength = parseContentLength(headers->at("content-length"));
	if (!contentLength)
	{
		error({}, ErrorCode::ParseError, "Invalid Content-Length header.");
		return std::nullopt;
	}
	if (*contentLength > c_maxContentLength)
	{
		error(
			{},
			ErrorCode::ParseError,
			fmt::format("Content-Length exceeds maximum supported size of {} bytes.", c_maxContentLength)
		);
		return std::nullopt;
	}

	std::string const data = readBytes(*contentLength);
	if (data.size() != *contentLength)
	{
		error({}, ErrorCode::ParseError, "Unexpected end of input while reading RPC payload.");
		return std::nullopt;
	}

	Json::Value jsonMessage;
	std::string jsonParsingErrors;
	hyperion::util::jsonParseStrict(data, jsonMessage, &jsonParsingErrors);
	if (!jsonParsingErrors.empty() || !jsonMessage || !jsonMessage.isObject())
	{
		error({}, ErrorCode::ParseError, "Could not parse RPC JSON payload. " + jsonParsingErrors);
		return std::nullopt;
	}

	return {std::move(jsonMessage)};
}

void Transport::trace(std::string _message, Json::Value _extra)
{
	if (m_logTrace != TraceValue::Off)
	{
		Json::Value params;
		if (_extra.isObject())
			params = std::move(_extra);
		params["message"] = std::move(_message);
		notify("$/logTrace", std::move(params));
	}
}

std::optional<std::map<std::string, std::string>> Transport::parseHeaders()
{
	std::map<std::string, std::string> headers;
	size_t headerBytes = 0;
	size_t headerCount = 0;

	while (true)
	{
		auto line = readLine(c_maxHeaderLineLength);
		if (!line)
		{
			closeProtocol();
			return std::nullopt;
		}

		headerBytes += line->size() + 1;
		if (headerBytes > c_maxHeaderBytes)
		{
			closeProtocol();
			return std::nullopt;
		}

		if (boost::trim_copy(*line).empty())
			break;

		if (++headerCount > c_maxHeaderCount)
		{
			closeProtocol();
			return std::nullopt;
		}

		auto const delimiterPos = line->find(':');
		if (delimiterPos == std::string::npos)
			return std::nullopt;

		auto const name = boost::to_lower_copy(line->substr(0, delimiterPos));
		auto const value = line->substr(delimiterPos + 1);
		if (!headers.emplace(boost::trim_copy(name), boost::trim_copy(value)).second)
			return std::nullopt;
	}
	return {std::move(headers)};
}

void Transport::notify(std::string _method, Json::Value _message)
{
	Json::Value json;
	json["method"] = std::move(_method);
	json["params"] = std::move(_message);
	send(std::move(json));
}

void Transport::reply(MessageID _id, Json::Value _message)
{
	Json::Value json;
	json["result"] = std::move(_message);
	send(std::move(json), _id);
}

void Transport::error(MessageID _id, ErrorCode _code, std::string _message)
{
	Json::Value json;
	json["error"]["code"] = static_cast<int>(_code);
	json["error"]["message"] = std::move(_message);
	send(std::move(json), _id);
}

void Transport::send(Json::Value _json, MessageID _id)
{
	hypAssert(_json.isObject());
	_json["jsonrpc"] = "2.0";
	if (_id != Json::nullValue)
		_json["id"] = _id;

	// Trailing CRLF only for easier readability.
	std::string const jsonString = hyperion::util::jsonCompactPrint(_json);

	writeBytes(fmt::format("Content-Length: {}\r\n\r\n", jsonString.size()));
	writeBytes(jsonString);
	flushOutput();
}
// }}}

// {{{ IOStreamTransport
IOStreamTransport::IOStreamTransport(std::istream& _in, std::ostream& _out):
	m_input{_in},
	m_output{_out}
{
}

bool IOStreamTransport::closed() const noexcept
{
	return protocolClosed() || m_input.eof();
}

std::string IOStreamTransport::readBytes(size_t _length)
{
	return util::readBytes(m_input, _length);
}

std::optional<std::string> IOStreamTransport::readLine(size_t _maxLength)
{
	return readLineFromStream(m_input, _maxLength);
}

void IOStreamTransport::writeBytes(std::string_view _data)
{
	m_output.write(_data.data(), static_cast<std::streamsize>(_data.size()));
}

void IOStreamTransport::flushOutput()
{
	m_output.flush();
}
// }}}

// {{{ StdioTransport
StdioTransport::StdioTransport()
{
	#if defined(_WIN32)
	// Attempt to change the modes of stdout from text to binary.
	setmode(fileno(stdout), O_BINARY);
	#endif
}

bool StdioTransport::closed() const noexcept
{
	return protocolClosed() || feof(stdin);
}

std::string StdioTransport::readBytes(size_t _byteCount)
{
	std::string buffer;
	buffer.resize(_byteCount);
	auto const n = fread(buffer.data(), 1, _byteCount, stdin);
	if (n < _byteCount)
		buffer.resize(n);
	return buffer;
}

std::optional<std::string> StdioTransport::readLine(size_t _maxLength)
{
	std::optional<std::string> line = readLineFromStream(std::cin, _maxLength);
	if (line)
		lspDebug(fmt::format("Received: {}", *line));
	return line;
}

void StdioTransport::writeBytes(std::string_view _data)
{
	lspDebug(fmt::format("Sending: {}", _data));
	auto const bytesWritten = fwrite(_data.data(), 1, _data.size(), stdout);
	hypAssert(bytesWritten == _data.size());
}

void StdioTransport::flushOutput()
{
	fflush(stdout);
}
// }}}
