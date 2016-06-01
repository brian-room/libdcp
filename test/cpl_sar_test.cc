/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "cpl.h"
#include "reel_mono_picture_asset.h"
#include "mono_picture_asset.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <boost/test/unit_test.hpp>

using std::string;
using boost::shared_ptr;

static void
check (shared_ptr<dcp::ReelMonoPictureAsset> pa, dcp::Fraction far, string sar)
{
	pa->set_screen_aspect_ratio (far);
	xmlpp::Document doc;
	xmlpp::Element* el = doc.create_root_node ("Test");
	pa->write_to_cpl (el, dcp::INTEROP);

	cxml::Node node (el);
	BOOST_CHECK_EQUAL (node.node_child("MainPicture")->string_child ("ScreenAspectRatio"), sar);
}

/** Test for a reported bug where <ScreenAspectRatio> in Interop files uses
 *  excessive decimal places and (sometimes) the wrong decimal point character.
 *  Also check that we correctly use one of the allowed <ScreenAspectRatio>
 *  values with Interop.
 */
BOOST_AUTO_TEST_CASE (cpl_sar)
{
	shared_ptr<dcp::ReelMonoPictureAsset> pa (
		new dcp::ReelMonoPictureAsset (
			shared_ptr<dcp::MonoPictureAsset> (new dcp::MonoPictureAsset ("test/ref/DCP/dcp_test1/video.mxf")),
			0
			)
		);

	/* Easy ones */
	check (pa, dcp::Fraction (1998, 1080), "1.85");
	check (pa, dcp::Fraction (2048, 858), "2.39");

	/* Check the use of the allowed values */

	/* Just less then, equal to and just more than 1.33 */
	check (pa, dcp::Fraction (1200, 1000), "1.33");
	check (pa, dcp::Fraction (1330, 1000), "1.33");
	check (pa, dcp::Fraction (1430, 1000), "1.33");

	/* Same for 1.66 */
	check (pa, dcp::Fraction (1600, 1000), "1.66");
	check (pa, dcp::Fraction (1660, 1000), "1.66");
	check (pa, dcp::Fraction (1670, 1000), "1.66");

	/* 1.77 */
	check (pa, dcp::Fraction (1750, 1000), "1.77");
	check (pa, dcp::Fraction (1770, 1000), "1.77");
	check (pa, dcp::Fraction (1800, 1000), "1.77");

	/* 1.85 */
	check (pa, dcp::Fraction (1820, 1000), "1.85");
	check (pa, dcp::Fraction (1850, 1000), "1.85");
	check (pa, dcp::Fraction (1910, 1000), "1.85");

	/* 2.00 */
	check (pa, dcp::Fraction (1999, 1000), "2.00");
	check (pa, dcp::Fraction (2000, 1000), "2.00");
	check (pa, dcp::Fraction (2001, 1000), "2.00");

	/* 2.39 */
	check (pa, dcp::Fraction (2350, 1000), "2.39");
	check (pa, dcp::Fraction (2390, 1000), "2.39");
	check (pa, dcp::Fraction (2500, 1000), "2.39");
}
