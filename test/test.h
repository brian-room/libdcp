/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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
*/

#include <boost/filesystem.hpp>

namespace xmlpp {
	class Element;
}

extern boost::filesystem::path private_test;
extern boost::filesystem::path xsd_test;

extern void check_xml (xmlpp::Element* ref, xmlpp::Element* test, std::list<std::string> ignore);
extern void check_xml (std::string ref, std::string test, std::list<std::string> ignore);
extern void check_file (boost::filesystem::path ref, boost::filesystem::path check);

/** Creating an object of this class will make asdcplib's random number generation
 *  (more) predictable.
 */
class RNGFixer
{
public:
	RNGFixer ();
	~RNGFixer ();
};

