/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  src/reel_picture_asset.h
 *  @brief ReelPictureAsset class.
 */

#include "reel_picture_asset.h"
#include "picture_asset.h"
#include "dcp_assert.h"
#include "raw_convert.h"
#include "compose.hpp"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <iomanip>
#include <cmath>

using std::bad_cast;
using std::string;
using std::stringstream;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;
using namespace dcp;

ReelPictureAsset::ReelPictureAsset ()
	: _frame_rate (Fraction (24, 1))
	, _screen_aspect_ratio (Fraction (1998, 1080))
{

}

ReelPictureAsset::ReelPictureAsset (shared_ptr<PictureAsset> asset, int64_t entry_point)
	: ReelAsset (asset, asset->edit_rate(), asset->intrinsic_duration(), entry_point)
	, ReelMXF (asset->key_id())
	, _frame_rate (asset->frame_rate ())
	, _screen_aspect_ratio (asset->screen_aspect_ratio ())
{

}

ReelPictureAsset::ReelPictureAsset (shared_ptr<const cxml::Node> node)
	: ReelAsset (node)
	, ReelMXF (node)
{
	_frame_rate = Fraction (node->string_child ("FrameRate"));
	try {
		_screen_aspect_ratio = Fraction (node->string_child ("ScreenAspectRatio"));
	} catch (XMLError& e) {
		/* It's not a fraction */
		try {
			float f = node->number_child<float> ("ScreenAspectRatio");
			_screen_aspect_ratio = Fraction (f * 1000, 1000);
		} catch (bad_cast& e) {

		}
	}
}

void
ReelPictureAsset::write_to_cpl (xmlpp::Node* node, Standard standard) const
{
	ReelAsset::write_to_cpl (node, standard);

	/* Find <MainPicture> */
	xmlpp::Node* mp = find_child (node, cpl_node_name ());

	mp->add_child ("FrameRate")->add_child_text (String::compose ("%1 %2", _frame_rate.numerator, _frame_rate.denominator));
	if (standard == INTEROP) {

		/* Allowed values for this tag from the standard */
		float allowed[] = { 1.33, 1.66, 1.77, 1.85, 2.00, 2.39 };
		int const num_allowed = sizeof(allowed) / sizeof(float);

		/* Actual ratio */
		float ratio = float (_screen_aspect_ratio.numerator) / _screen_aspect_ratio.denominator;

		/* Pick the closest and use that */
		optional<float> closest;
		optional<float> error;
		for (int i = 0; i < num_allowed; ++i) {
			float const e = fabsf (allowed[i] - ratio);
			if (!closest || e < error.get()) {
				closest = allowed[i];
				error = e;
			}
		}

		mp->add_child ("ScreenAspectRatio")->add_child_text (raw_convert<string> (closest.get(), 2, true));
	} else {
		mp->add_child ("ScreenAspectRatio")->add_child_text (
			String::compose ("%1 %2", _screen_aspect_ratio.numerator, _screen_aspect_ratio.denominator)
			);
	}

        if (key_id ()) {
		/* Find <Hash> */
		xmlpp::Node* hash = find_child (mp, "Hash");
		mp->add_child_before (hash, "KeyId")->add_child_text ("urn:uuid:" + key_id().get ());
        }
}

string
ReelPictureAsset::key_type () const
{
	return "MDIK";
}

bool
ReelPictureAsset::equals (shared_ptr<const ReelAsset> other, EqualityOptions opt, NoteHandler note) const
{
	if (!ReelAsset::equals (other, opt, note)) {
		return false;
	}

	shared_ptr<const ReelPictureAsset> rpa = dynamic_pointer_cast<const ReelPictureAsset> (other);
	if (!rpa) {
		return false;
	}

	if (_frame_rate != rpa->_frame_rate) {
		note (DCP_ERROR, "frame rates differ in reel");
		return false;
	}

	if (_screen_aspect_ratio != rpa->_screen_aspect_ratio) {
		note (DCP_ERROR, "screen aspect ratios differ in reel");
		return false;
	}

	return true;
}
