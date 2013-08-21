/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file  src/metadata.cc
 *  @brief Metadata for writing to the DCP.
 */

#include <sstream>
#include <iomanip>
#include <time.h>
#ifdef LIBDCP_WINDOWS
#include <windows.h>
#endif
#include "metadata.h"

using namespace std;
using namespace libdcp;

MXFMetadata::MXFMetadata ()
	: company_name ("libdcp")
	, product_name ("libdcp")
	, product_version (LIBDCP_VERSION)
{

}


XMLMetadata::XMLMetadata ()
	: issuer ("libdcp" LIBDCP_VERSION)
	, creator ("libdcp" LIBDCP_VERSION)
{
	set_issue_date_now ();
}

void
XMLMetadata::set_issue_date_now ()
{
	char buffer[64];
	time_t now = time (0);
	struct tm* tm = localtime (&now);
	strftime (buffer, 64, "%Y-%m-%dT%I:%M:%S", tm);

	int offset = 0;

#ifdef LIBDCP_POSIX

	offset = tm->tm_gmtoff / 60;
	
#else
	TIME_ZONE_INFORMATION tz;
	GetTimeZoneInformation (&tz);
	offset = tz.Bias;
#endif
	
	issue_date = string (buffer) + utc_offset_to_string (offset);
}

/** @param b Offset from UTC to local time in minutes.
 *  @return string of the form e.g. -01:00.
 */
string
XMLMetadata::utc_offset_to_string (int b)
{
	bool const negative = (b < 0);
	b = negative ? -b : b;

	int const hours = b / 60;
	int const minutes = b % 60;

	stringstream o;
	if (negative) {
		o << "-";
	} else {
		o << "+";
	}

	o << setw(2) << setfill('0') << hours << ":" << setw(2) << setfill('0') << minutes;
	return o.str ();
}
