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

#include <boost/test/unit_test.hpp>

#include <limits>
#include <sstream>
#include <string>

using namespace std::string_literals;

namespace hyperion::lsp::test
{

namespace
{

std::string frame(std::string const& _payload)
{
	return "Content-Length: " + std::to_string(_payload.size()) + "\r\n\r\n" + _payload;
}

bool containsParseError(std::string const& _output, std::string const& _message)
{
	return
		_output.find("\"code\":-32700") != std::string::npos &&
		_output.find(_message) != std::string::npos;
}

}

BOOST_AUTO_TEST_SUITE(LSPTransport)

BOOST_AUTO_TEST_CASE(receive_accepts_valid_message)
{
	std::string const payload = R"({"jsonrpc":"2.0","id":1,"method":"test"})";
	std::istringstream input{frame(payload)};
	std::ostringstream output;
	IOStreamTransport transport{input, output};

	auto message = transport.receive();
	BOOST_REQUIRE(message);
	BOOST_CHECK_EQUAL((*message)["method"].asString(), "test");
	BOOST_CHECK(output.str().empty());
}

BOOST_AUTO_TEST_CASE(receive_rejects_invalid_content_length)
{
	std::istringstream input{"Content-Length: nope\r\n\r\n"};
	std::ostringstream output;
	IOStreamTransport transport{input, output};

	BOOST_CHECK(!transport.receive());
	BOOST_CHECK(containsParseError(output.str(), "Invalid Content-Length header."));
}

BOOST_AUTO_TEST_CASE(receive_rejects_oversized_content_length_without_consuming_next_message)
{
	std::string const payload = R"({"jsonrpc":"2.0","id":1,"method":"test"})";
	std::istringstream input{
		"Content-Length: "s + std::to_string(std::numeric_limits<size_t>::max()) + "\r\n\r\n" +
		frame(payload)
	};
	std::ostringstream output;
	IOStreamTransport transport{input, output};

	BOOST_CHECK(!transport.receive());
	BOOST_CHECK(containsParseError(output.str(), "Content-Length exceeds maximum supported size"));

	auto message = transport.receive();
	BOOST_REQUIRE(message);
	BOOST_CHECK_EQUAL((*message)["method"].asString(), "test");
}

BOOST_AUTO_TEST_CASE(receive_rejects_short_body)
{
	std::istringstream input{"Content-Length: 10\r\n\r\n{}"};
	std::ostringstream output;
	IOStreamTransport transport{input, output};

	BOOST_CHECK(!transport.receive());
	BOOST_CHECK(containsParseError(output.str(), "Unexpected end of input while reading RPC payload."));
}

BOOST_AUTO_TEST_CASE(receive_rejects_oversized_header_line)
{
	std::istringstream input{"X-Test: " + std::string(9000, 'a') + "\r\n\r\n"};
	std::ostringstream output;
	IOStreamTransport transport{input, output};

	BOOST_CHECK(!transport.receive());
	BOOST_CHECK(transport.closed());
	BOOST_CHECK(containsParseError(output.str(), "Could not parse RPC headers."));
}

BOOST_AUTO_TEST_CASE(receive_rejects_too_many_headers)
{
	std::string inputString;
	for (size_t i = 0; i < 65; ++i)
		inputString += "X-Test-" + std::to_string(i) + ": value\r\n";
	inputString += "\r\n";
	std::istringstream input{inputString};
	std::ostringstream output;
	IOStreamTransport transport{input, output};

	BOOST_CHECK(!transport.receive());
	BOOST_CHECK(transport.closed());
	BOOST_CHECK(containsParseError(output.str(), "Could not parse RPC headers."));
}

BOOST_AUTO_TEST_CASE(receive_rejects_oversized_header_block)
{
	std::string inputString;
	for (size_t i = 0; i < 17; ++i)
		inputString += "X-Test-" + std::to_string(i) + ": " + std::string(4090, 'a') + "\r\n";
	inputString += "\r\n";
	std::istringstream input{inputString};
	std::ostringstream output;
	IOStreamTransport transport{input, output};

	BOOST_CHECK(!transport.receive());
	BOOST_CHECK(transport.closed());
	BOOST_CHECK(containsParseError(output.str(), "Could not parse RPC headers."));
}

BOOST_AUTO_TEST_SUITE_END()

}
