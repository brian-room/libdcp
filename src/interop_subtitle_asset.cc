/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "interop_subtitle_asset.h"
#include "interop_load_font_node.h"
#include "xml.h"
#include "raw_convert.h"
#include "font_node.h"
#include "util.h"
#include "font_asset.h"
#include "dcp_assert.h"
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>
#include <cmath>
#include <cstdio>

using std::list;
using std::string;
using std::cout;
using std::cerr;
using std::map;
using boost::shared_ptr;
using boost::shared_array;
using boost::optional;
using boost::dynamic_pointer_cast;
using namespace dcp;

InteropSubtitleAsset::InteropSubtitleAsset (boost::filesystem::path file)
	: SubtitleAsset (file)
{
	shared_ptr<cxml::Document> xml (new cxml::Document ("DCSubtitle"));
	xml->read_file (file);
	_id = xml->string_child ("SubtitleID");
	_reel_number = xml->string_child ("ReelNumber");
	_language = xml->string_child ("Language");
	_movie_title = xml->string_child ("MovieTitle");
	_load_font_nodes = type_children<dcp::InteropLoadFontNode> (xml, "LoadFont");

	list<cxml::NodePtr> f = xml->node_children ("Font");
	list<shared_ptr<dcp::FontNode> > font_nodes;
	BOOST_FOREACH (cxml::NodePtr& i, f) {
		font_nodes.push_back (shared_ptr<FontNode> (new FontNode (i, 250)));
	}

	parse_subtitles (xml, font_nodes);
}

InteropSubtitleAsset::InteropSubtitleAsset ()
{
	
}

Glib::ustring
InteropSubtitleAsset::xml_as_string () const
{
	xmlpp::Document doc;
	xmlpp::Element* root = doc.create_root_node ("DCSubtitle");
	root->set_attribute ("Version", "1.0");

	root->add_child("SubtitleID")->add_child_text (_id);
	root->add_child("MovieTitle")->add_child_text (_movie_title);
	root->add_child("ReelNumber")->add_child_text (raw_convert<string> (_reel_number));
	root->add_child("Language")->add_child_text (_language);

	for (list<shared_ptr<InteropLoadFontNode> >::const_iterator i = _load_font_nodes.begin(); i != _load_font_nodes.end(); ++i) {
		xmlpp::Element* load_font = root->add_child("LoadFont");
		load_font->set_attribute ("Id", (*i)->id);
		load_font->set_attribute ("URI", (*i)->uri);
	}

	subtitles_as_xml (root, 250, "");

	return doc.write_to_string_formatted ("UTF-8");
}

void
InteropSubtitleAsset::add_font (string id, boost::filesystem::path file)
{
	add_font_data (id, file);
	_load_font_nodes.push_back (shared_ptr<InteropLoadFontNode> (new InteropLoadFontNode (id, file.leaf().string ())));
}

bool
InteropSubtitleAsset::equals (shared_ptr<const Asset> other_asset, EqualityOptions options, NoteHandler note) const
{
	if (!SubtitleAsset::equals (other_asset, options, note)) {
		return false;
	}
	
	shared_ptr<const InteropSubtitleAsset> other = dynamic_pointer_cast<const InteropSubtitleAsset> (other_asset);
	if (!other) {
		return false;
	}

	list<shared_ptr<InteropLoadFontNode> >::const_iterator i = _load_font_nodes.begin ();
	list<shared_ptr<InteropLoadFontNode> >::const_iterator j = other->_load_font_nodes.begin ();

	while (i != _load_font_nodes.end ()) {
		if (j == other->_load_font_nodes.end ()) {
			note (DCP_ERROR, "<LoadFont> nodes differ");
			return false;
		}

		if (**i != **j) {
			note (DCP_ERROR, "<LoadFont> nodes differ");
			return false;
		}

		++i;
		++j;
	}

	if (_movie_title != other->_movie_title) {
		note (DCP_ERROR, "Subtitle movie titles differ");
		return false;
	}

	return true;
}

list<shared_ptr<LoadFontNode> >
InteropSubtitleAsset::load_font_nodes () const
{
	list<shared_ptr<LoadFontNode> > lf;
	copy (_load_font_nodes.begin(), _load_font_nodes.end(), back_inserter (lf));
	return lf;
}

/** Write this content to an XML file with its fonts alongside */
void
InteropSubtitleAsset::write (boost::filesystem::path p) const
{
	FILE* f = fopen_boost (p, "w");
	if (!f) {
		throw FileError ("Could not open file for writing", p, -1);
	}
	
	Glib::ustring const s = xml_as_string ();
	fwrite (s.c_str(), 1, s.bytes(), f);
	fclose (f);

	_file = p;

	BOOST_FOREACH (shared_ptr<InteropLoadFontNode> i, _load_font_nodes) {
		boost::filesystem::path file = p.parent_path() / i->uri;
		FILE* f = fopen_boost (file, "w");
		if (!f) {
			throw FileError ("could not open font file for writing", file, errno);
		}
		map<string, FileData>::const_iterator j = _fonts.find (i->id);
		if (j != _fonts.end ()) {
			fwrite (j->second.data.get(), 1, j->second.size, f);
			j->second.file = file;
		}
		fclose (f);
	}
}

void
InteropSubtitleAsset::resolve_fonts (list<shared_ptr<Object> > objects)
{
	BOOST_FOREACH (shared_ptr<Object> i, objects) {
		shared_ptr<FontAsset> font = dynamic_pointer_cast<FontAsset> (i);
		if (!font) {
			continue;
		}

		BOOST_FOREACH (shared_ptr<InteropLoadFontNode> j, _load_font_nodes) {
			if (j->uri == font->file().leaf().string ()) {
				add_font_data (j->id, font->file ());
			}
		}
	}
}

void
InteropSubtitleAsset::add_font_assets (list<shared_ptr<Asset> >& assets)
{
	for (map<string, FileData>::const_iterator i = _fonts.begin(); i != _fonts.end(); ++i) {
		DCP_ASSERT (i->second.file);
		assets.push_back (shared_ptr<FontAsset> (new FontAsset (i->second.file.get ())));
	}
}