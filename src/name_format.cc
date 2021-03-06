/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

    This file is part of libdcp.

    libdcp is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    libdcp is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libdcp.  If not, see <http://www.gnu.org/licenses/>.

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations
    including the two.

    You must obey the GNU General Public License in all respects
    for all of the code used other than OpenSSL.  If you modify
    file(s) with this exception, you may extend this exception to your
    version of the file(s), but you are not obligated to do so.  If you
    do not wish to do so, delete this exception statement from your
    version.  If you delete this exception statement from all source
    files in the program, then also delete it here.
*/

#include "name_format.h"
#include <boost/optional.hpp>
#include <boost/foreach.hpp>

using std::string;
using std::map;
using boost::optional;
using namespace dcp;

static char
filter (char c)
{
	if (c == '/' || c == ':') {
		c = '-';
	} else if (c == ' ') {
		c = '_';
	}

	return c;
}

static string
filter (string c)
{
	string o;

	for (size_t i = 0; i < c.length(); ++i) {
		o += filter (c[i]);
	}

	return o;
}


/** @param values Values to replace our specifications with; e.g.
 *  if the specification contains %c it will be be replaced with the
 *  value corresponding to the key 'c'.
 *  @param suffix Suffix to add on after processing the specification.
 *  @param ignore Any specification characters in this string will not
 *  be replaced, but left as-is.
 */
string
NameFormat::get (Map values, string suffix, string ignore) const
{
	string result;
	for (size_t i = 0; i < _specification.length(); ++i) {
		bool done = false;
		if (_specification[i] == '%' && (i < _specification.length() - 1)) {
			char const key = _specification[i + 1];
			Map::const_iterator j = values.find(key);
			if (j != values.end() && ignore.find(key) == string::npos) {
				result += filter (j->second);
				++i;
				done = true;
			}
		}

		if (!done) {
			result += filter (_specification[i]);
		}
	}

	return result + suffix;
}

bool
dcp::operator== (NameFormat const & a, NameFormat const & b)
{
	return a.specification() == b.specification();
}
