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

#ifndef LIBDCP_NAME_FORMAT
#define LIBDCP_NAME_FORMAT

#include <boost/optional.hpp>
#include <map>
#include <list>

namespace dcp {

class NameFormat
{
public:
	std::list<char> components () const {
		return _components;
	}

	std::string specification () const {
		return _specification;
	}

	void set_specification (std::string specification) {
		_specification = specification;
	}

	typedef std::map<char, std::string> Map;

	std::string get (Map) const;

protected:
	NameFormat () {}

	NameFormat (std::string specification)
		: _specification (specification)
	{}

	void add (char placeholder);

private:
	/** placeholders for each component */
	std::list<char> _components;
	std::string _specification;
};

extern bool operator== (NameFormat const & a, NameFormat const & b);

}

#endif
