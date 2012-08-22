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

#include "reel.h"
#include "util.h"
#include "picture_asset.h"
#include "sound_asset.h"
#include "subtitle_asset.h"

using namespace std;
using namespace libdcp;

void
Reel::write_to_cpl (ostream& s) const
{
	s << "    <Reel>\n"
	  << "      <Id>urn:uuid:" << make_uuid() << "</Id>\n"
	  << "      <AssetList>\n";
	
	if (_main_picture) {
		_main_picture->write_to_cpl (s);
	}

	if (_main_sound) {
		_main_sound->write_to_cpl (s);
	}

	if (_main_subtitle) {
		_main_subtitle->write_to_cpl (s);
	}
}
	
void
Reel::write_to_pkl (ostream& s) const
{
	if (_main_picture) {
		_main_picture->write_to_pkl (s);
	}

	if (_main_sound) {
		_main_sound->write_to_pkl (s);
	}

	if (_main_subtitle) {
		_main_subtitle->write_to_pkl (s);
	}
}

void
Reel::write_to_assetmap (ostream& s) const
{
	if (_main_picture) {
		_main_picture->write_to_assetmap (s);
	}

	if (_main_sound) {
		_main_sound->write_to_assetmap (s);
	}

	if (_main_subtitle) {
		_main_subtitle->write_to_assetmap (s);
	}
}

list<string>
Reel::equals (boost::shared_ptr<const Reel> other, EqualityOptions opt) const
{
	list<string> o;
	
	if ((_main_picture && !other->_main_picture) || (!_main_picture && other->_main_picture)) {
		o.push_back ("reel has different assets");
	}
	
	if (_main_picture) {
		list<string> m = _main_picture->equals (other->_main_picture, opt);
		o.merge (m);
	}

	if ((_main_sound && !other->_main_sound) || (!_main_sound && other->_main_sound)) {
		o.push_back ("reel has different assets");
	}
	
	if (_main_sound) {
		list<string> m = _main_sound->equals (other->_main_sound, opt);
		o.merge (m);
	}

	if ((_main_subtitle && !other->_main_subtitle) || (!_main_subtitle && other->_main_subtitle)) {
		o.push_back ("reel has different assets");
	}
	
	if (_main_subtitle) {
		list<string> m = _main_subtitle->equals (other->_main_subtitle, opt);
		o.merge (m);
	}

	return o;
}
