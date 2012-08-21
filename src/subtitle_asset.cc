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

#include "subtitle_asset.h"

using namespace std;
using namespace boost;
using namespace libdcp;

SubtitleAsset::SubtitleAsset (string directory, string xml)
	: Asset (directory, xml)
	, XMLFile (path().string(), "DCSubtitle")
{
	_subtitle_id = string_node ("SubtitleID");
	_movie_title = string_node ("MovieTitle");
	_reel_number = int64_node ("ReelNumber");
	_language = string_node ("Language");

	ignore_node ("LoadFont");

	list<shared_ptr<FontNode> > font_nodes = sub_nodes<FontNode> ("Font");
	_load_font_nodes = sub_nodes<LoadFontNode> ("LoadFont");

	/* Now make Subtitle objects to represent the raw XML nodes
	   in a sane way.
	*/

	list<shared_ptr<FontNode> > current_font_nodes;
	for (list<shared_ptr<FontNode> >::iterator i = font_nodes.begin(); i != font_nodes.end(); ++i) {
		examine_font_node (*i, current_font_nodes);
	}
}

void
SubtitleAsset::examine_font_node (shared_ptr<FontNode> font_node, list<shared_ptr<FontNode> >& current_font_nodes)
{
	current_font_nodes.push_back (font_node);
	
	for (list<shared_ptr<SubtitleNode> >::iterator j = font_node->subtitle_nodes.begin(); j != font_node->subtitle_nodes.end(); ++j) {
		for (list<shared_ptr<TextNode> >::iterator k = (*j)->text_nodes.begin(); k != (*j)->text_nodes.end(); ++k) {
			_subtitles.push_back (
				shared_ptr<Subtitle> (
					new Subtitle (
						font_id_to_name (id_from_font_nodes (current_font_nodes)),
						size_from_font_nodes (current_font_nodes),
						(*j)->in,
						(*j)->out,
						(*k)->v_position,
						(*k)->text
						)
					)
				);
		}
	}

	for (list<shared_ptr<FontNode> >::iterator j = font_node->font_nodes.begin(); j != font_node->font_nodes.end(); ++j) {
		examine_font_node (*j, current_font_nodes);
	}

	current_font_nodes.pop_back ();
}

string
SubtitleAsset::id_from_font_nodes (list<shared_ptr<FontNode> > const & font_nodes) const
{
	for (list<shared_ptr<FontNode> >::const_reverse_iterator i = font_nodes.rbegin(); i != font_nodes.rend(); ++i) {
		if (!(*i)->id.empty ()) {
			return (*i)->id;
		}
	}

	return "";
}

int
SubtitleAsset::size_from_font_nodes (list<shared_ptr<FontNode> > const & font_nodes) const
{
	for (list<shared_ptr<FontNode> >::const_reverse_iterator i = font_nodes.rbegin(); i != font_nodes.rend(); ++i) {
		if ((*i)->size != 0) {
			return (*i)->size;
		}
	}

	return 0;

}

FontNode::FontNode (xmlpp::Node const * node)
	: XMLNode (node)
{
	id = string_attribute ("Id");
	size = optional_int64_attribute ("Size");
	subtitle_nodes = sub_nodes<SubtitleNode> ("Subtitle");
	font_nodes = sub_nodes<FontNode> ("Font");
}

LoadFontNode::LoadFontNode (xmlpp::Node const * node)
	: XMLNode (node)
{
	id = string_attribute ("Id");
	uri = string_attribute ("URI");
}
	

SubtitleNode::SubtitleNode (xmlpp::Node const * node)
	: XMLNode (node)
{
	in = time_attribute ("TimeIn");
	out = time_attribute ("TimeOut");
	text_nodes = sub_nodes<TextNode> ("Text");
}

TextNode::TextNode (xmlpp::Node const * node)
	: XMLNode (node)
{
	text = content ();
	v_position = float_attribute ("VPosition");
}

list<shared_ptr<Subtitle> >
SubtitleAsset::subtitles_at (Time t) const
{
	list<shared_ptr<Subtitle> > s;
	for (list<shared_ptr<Subtitle> >::const_iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
		if ((*i)->in() <= t && t <= (*i)->out ()) {
			s.push_back (*i);
		}
	}

	return s;
}

std::string
SubtitleAsset::font_id_to_name (string id) const
{
	list<shared_ptr<LoadFontNode> >::const_iterator i = _load_font_nodes.begin();
	while (i != _load_font_nodes.end() && (*i)->id != id) {
		++i;
	}

	if (i == _load_font_nodes.end ()) {
		return "";
	}

	if ((*i)->uri == "arial.ttf") {
		return "Arial";
	}

	return "";
}

Subtitle::Subtitle (
	std::string font,
	int size,
	Time in,
	Time out,
	float v_position,
	std::string text
	)
	: _font (font)
	, _size (size)
	, _in (in)
	, _out (out)
	, _v_position (v_position)
	, _text (text)
{

}

int
Subtitle::size_in_pixels (int screen_height) const
{
	/* Size in the subtitle file is given in points as if the screen
	   height is 11 inches, so a 72pt font would be 1/11th of the screen
	   height.
	*/
	
	return _size * screen_height / (11 * 72);
}
