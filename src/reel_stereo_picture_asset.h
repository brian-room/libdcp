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

/** @file  src/reel_stereo_picture_asset.h
 *  @brief ReelStereoPictureAsset class.
 */

#ifndef LIBDCP_REEL_STEREO_PICTURE_ASSET_H
#define LIBDCP_REEL_STEREO_PICTURE_ASSET_H

#include "reel_picture_asset.h"
#include "stereo_picture_asset.h"

namespace dcp {

class StereoPictureAsset;

/** @class ReelStereoPictureAsset
 *  @brief Part of a Reel's description which refers to a stereoscopic picture asset.
 */
class ReelStereoPictureAsset : public ReelPictureAsset
{
public:
	ReelStereoPictureAsset ();
	ReelStereoPictureAsset (boost::shared_ptr<StereoPictureAsset> content, int64_t entry_point);
	ReelStereoPictureAsset (boost::shared_ptr<const cxml::Node>);

	/** @return the StereoPictureAsset that this object refers to */
	boost::shared_ptr<const StereoPictureAsset> stereo_asset () const {
		return asset_of_type<const StereoPictureAsset> ();
	}

	/** @return the StereoPictureAsset that this object refers to */
	boost::shared_ptr<StereoPictureAsset> stereo_asset () {
		return asset_of_type<StereoPictureAsset> ();
	}

private:
	std::string cpl_node_name () const;
	std::pair<std::string, std::string> cpl_node_attribute (Standard standard) const;
};

}

#endif
