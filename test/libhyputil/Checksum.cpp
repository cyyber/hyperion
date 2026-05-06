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
 * Unit tests for address canonicalisation.
 * Addresses are 48 bytes = 96 hex characters after the Q prefix (97 total).
 */

#include <libhyputil/CommonData.h>
#include <libhyputil/Exceptions.h>

#include <test/Common.h>

#include <boost/test/unit_test.hpp>


namespace hyperion::util::test
{

BOOST_AUTO_TEST_SUITE(AddressCanonicalisation)

// 96 hex = 48 bytes. Q + 96 hex = 97 chars.
// Addresses below are 40 hex of original + 56 hex of padding = 96 hex total.

BOOST_AUTO_TEST_CASE(calculate)
{
	BOOST_CHECK(!getChecksummedAddress("Q5aaeb6053f3e94c9b9a09f33669435e7ef1beaed00000000000000000000000000000000000000000000000000000000").empty());
	BOOST_CHECK(!getChecksummedAddress("Q0123456789abcdefabcdef0123456789abcdefab00000000000000000000000000000000000000000000000000000000").empty());
	// no prefix
	BOOST_CHECK_THROW(getChecksummedAddress("5aaeb6053f3e94c9b9a09f33669435e7ef1beaed00000000000000000000000000000000000000000000000000000000"), InvalidAddress);
	// too short (95 hex)
	BOOST_CHECK_THROW(getChecksummedAddress("Q5aaeb6053f3e94c9b9a09f33669435e7ef1beaed0000000000000000000000000000000000000000000000000000000"), InvalidAddress);
	// too long (97 hex)
	BOOST_CHECK_THROW(getChecksummedAddress("Q5aaeb6053f3e94c9b9a09f33669435e7ef1beaed000000000000000000000000000000000000000000000000000000000"), InvalidAddress);
	// non-hex character
	BOOST_CHECK_THROW(getChecksummedAddress("Q5aaeb6053f3e94c9b9a09f33669435e7ef1beaed0000000000000000000000000000000000000000000000000000000K"), InvalidAddress);
}

BOOST_AUTO_TEST_CASE(canonical_roundtrip)
{
	std::string addr = "Q5aaeb6053f3e94c9b9a09f33669435e7ef1beaed00000000000000000000000000000000000000000000000000000000";
	std::string canonical = getChecksummedAddress(addr);
	BOOST_CHECK(passesAddressChecksum(canonical, true));

	std::string addr2 = "Qaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
	std::string canonical2 = getChecksummedAddress(addr2);
	BOOST_CHECK(passesAddressChecksum(canonical2, true));
}

BOOST_AUTO_TEST_CASE(all_lowercase_valid)
{
	std::string lower = "Qde709f2102306220921060314715629080e2fb77aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	BOOST_CHECK(passesAddressChecksum(lower, false));
}

BOOST_AUTO_TEST_CASE(all_uppercase_valid)
{
	std::string upper = "Q52908400098527886E0F7030069857D2E4169EE7AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	BOOST_CHECK(passesAddressChecksum(upper, false));
}

BOOST_AUTO_TEST_CASE(invalid_length)
{
	BOOST_CHECK(!passesAddressChecksum("Q5aaeb6053f3e94c9b9a09f33669435e7ef1beaed0000000000000000000000000000000000000000000000000000000", true));
	BOOST_CHECK(!passesAddressChecksum("Q5aaeb6053f3e94c9b9a09f33669435e7ef1beaed000000000000000000000000000000000000000000000000000000000", true));
	BOOST_CHECK(!passesAddressChecksum("", true));
	BOOST_CHECK(!passesAddressChecksum("Q", true));
}

BOOST_AUTO_TEST_SUITE_END()

}
