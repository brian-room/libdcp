/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  src/reel_sound_asset.cc
 *  @brief ReelSoundAsset class.
 */

#include "reel_sound_asset.h"
#include "dcp_assert.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>

using std::string;
using boost::shared_ptr;
using namespace dcp;

ReelSoundAsset::ReelSoundAsset (shared_ptr<SoundAsset> asset, int64_t entry_point)
	: ReelAsset (asset, asset->edit_rate(), asset->intrinsic_duration(), entry_point)
	, ReelMXF (asset->key_id())
{

}

ReelSoundAsset::ReelSoundAsset (shared_ptr<const cxml::Node> node)
	: ReelAsset (node)
	, ReelMXF (node)
{
	node->ignore_child ("Language");
	node->done ();
}

string
ReelSoundAsset::cpl_node_name () const
{
	return "MainSound";
}

string
ReelSoundAsset::key_type () const
{
	return "MDAK";
}

void
ReelSoundAsset::write_to_cpl (xmlpp::Node* node, Standard standard) const
{
	ReelAsset::write_to_cpl (node, standard);

        if (key_id ()) {
		/* Find <MainSound> */
		xmlpp::Node* ms = find_child (node, cpl_node_name ());
		/* Find <Hash> */
		xmlpp::Node* hash = find_child (ms, "Hash");
		ms->add_child_before (hash, "KeyId")->add_child_text ("urn:uuid:" + key_id().get ());
        }
}
