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

#include <boost/lexical_cast.hpp>
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
	list<shared_ptr<SubtitleNode> > current_subtitle_nodes;
	current_subtitle_nodes.push_back (shared_ptr<SubtitleNode> ());
	examine_font_nodes (font_nodes, current_font_nodes, current_subtitle_nodes);
}

void
SubtitleAsset::examine_font_nodes (
	list<shared_ptr<FontNode> > const & font_nodes,
	list<shared_ptr<FontNode> >& current_font_nodes,
	list<shared_ptr<SubtitleNode> >& current_subtitle_nodes
	)
{
	for (list<shared_ptr<FontNode> >::const_iterator i = font_nodes.begin(); i != font_nodes.end(); ++i) {
		
		current_font_nodes.push_back (*i);

		for (list<shared_ptr<SubtitleNode> >::iterator j = (*i)->subtitle_nodes.begin(); j != (*i)->subtitle_nodes.end(); ++j) {
			current_subtitle_nodes.push_back (*j);
			examine_text_nodes (*j, (*j)->text_nodes, current_font_nodes);
			examine_font_nodes ((*j)->font_nodes, current_font_nodes, current_subtitle_nodes);
			current_subtitle_nodes.pop_back ();
		}
	
		examine_font_nodes ((*i)->font_nodes, current_font_nodes, current_subtitle_nodes);
		examine_text_nodes (current_subtitle_nodes.back (), (*i)->text_nodes, current_font_nodes);
		
		current_font_nodes.pop_back ();
	}
}

void
SubtitleAsset::examine_text_nodes (
	shared_ptr<SubtitleNode> subtitle_node,
	list<shared_ptr<TextNode> > const & text_nodes,
	list<shared_ptr<FontNode> >& current_font_nodes
	)
{
	for (list<shared_ptr<TextNode> >::const_iterator i = text_nodes.begin(); i != text_nodes.end(); ++i) {
		FontNode effective (current_font_nodes);
		_subtitles.push_back (
			shared_ptr<Subtitle> (
				new Subtitle (
					font_id_to_name (effective.id),
					effective.italic.get(),
					effective.color.get(),
					effective.size,
					subtitle_node->in,
					subtitle_node->out,
					(*i)->v_position,
					(*i)->v_align,
					(*i)->text,
					effective.effect.get(),
					effective.effect_color.get(),
					subtitle_node->fade_up_time,
					subtitle_node->fade_down_time
					)
				)
			);
	}
}

FontNode::FontNode (xmlpp::Node const * node)
	: XMLNode (node)
{
	id = optional_string_attribute ("Id");
	size = optional_int64_attribute ("Size");
	italic = optional_bool_attribute ("Italic");
	color = optional_color_attribute ("Color");
	string const e = optional_string_attribute ("Effect");
	if (e == "none") {
		effect = NONE;
	} else if (e == "border") {
		effect = BORDER;
	} else if (e == "shadow") {
		effect = SHADOW;
	} else if (!e.empty ()) {
		throw DCPReadError ("unknown subtitle effect type");
	}
	effect_color = optional_color_attribute ("EffectColor");
	subtitle_nodes = sub_nodes<SubtitleNode> ("Subtitle");
	font_nodes = sub_nodes<FontNode> ("Font");
	text_nodes = sub_nodes<TextNode> ("Text");
}

FontNode::FontNode (list<shared_ptr<FontNode> > const & font_nodes)
	: size (0)
	, italic (false)
	, color ("FFFFFFFF")
	, effect_color ("FFFFFFFF")
{
	for (list<shared_ptr<FontNode> >::const_iterator i = font_nodes.begin(); i != font_nodes.end(); ++i) {
		if (!(*i)->id.empty ()) {
			id = (*i)->id;
		}
		if ((*i)->size != 0) {
			size = (*i)->size;
		}
		if ((*i)->italic) {
			italic = (*i)->italic.get ();
		}
		if ((*i)->color) {
			color = (*i)->color.get ();
		}
		if ((*i)->effect) {
			effect = (*i)->effect.get ();
		}
		if ((*i)->effect_color) {
			effect_color = (*i)->effect_color.get ();
		}
	}
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
	font_nodes = sub_nodes<FontNode> ("Font");
	text_nodes = sub_nodes<TextNode> ("Text");
	fade_up_time = fade_time ("FadeUpTime");
	fade_down_time = fade_time ("FadeDownTime");
}

Time
SubtitleNode::fade_time (string name)
{
	string const u = optional_string_attribute (name);
	Time t;
	
	if (u.empty ()) {
		t = Time (0, 0, 0, 20);
	} else if (u.find (":") != string::npos) {
		t = Time (u);
	} else {
		t = Time (0, 0, 0, lexical_cast<int> (u));
	}

	if (t > Time (0, 0, 8, 0)) {
		t = Time (0, 0, 8, 0);
	}

	return t;
}

TextNode::TextNode (xmlpp::Node const * node)
	: XMLNode (node)
	, v_align (CENTER)
{
	text = content ();
	v_position = float_attribute ("VPosition");
	string const v = optional_string_attribute ("VAlign");
	if (v == "top") {
		v_align = TOP;
	} else if (v == "center") {
		v_align = CENTER;
	} else if (v == "bottom") {
		v_align = BOTTOM;
	}
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
	string font,
	bool italic,
	Color color,
	int size,
	Time in,
	Time out,
	float v_position,
	VAlign v_align,
	string text,
	Effect effect,
	Color effect_color,
	Time fade_up_time,
	Time fade_down_time
	)
	: _font (font)
	, _italic (italic)
	, _color (color)
	, _size (size)
	, _in (in)
	, _out (out)
	, _v_position (v_position)
	, _v_align (v_align)
	, _text (text)
	, _effect (effect)
	, _effect_color (effect_color)
	, _fade_up_time (fade_up_time)
	, _fade_down_time (fade_down_time)
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

bool
libdcp::operator== (Subtitle const & a, Subtitle const & b)
{
	return (
		a.font() == b.font() &&
		a.italic() == b.italic() &&
		a.color() == b.color() &&
		a.size() == b.size() &&
		a.in() == b.in() &&
		a.out() == b.out() &&
		a.v_position() == b.v_position() &&
		a.v_align() == b.v_align() &&
		a.text() == b.text() &&
		a.effect() == b.effect() &&
		a.effect_color() == b.effect_color() &&
		a.fade_up_time() == b.fade_up_time() &&
		a.fade_down_time() == b.fade_down_time()
		);
}

ostream&
libdcp::operator<< (ostream& s, Subtitle const & sub)
{
	s << "\n`" << sub.text() << "' from " << sub.in() << " to " << sub.out() << ";\n"
	  << "fade up " << sub.fade_up_time() << ", fade down " << sub.fade_down_time() << ";\n"
	  << "font " << sub.font() << ", ";

	if (sub.italic()) {
		s << "italic";
	} else {
		s << "non-italic";
	}
	
	s << ", size " << sub.size() << ", color " << sub.color() << ", vpos " << sub.v_position() << ", valign " << ((int) sub.v_align()) << ";\n"
	  << "effect " << ((int) sub.effect()) << ", effect color " << sub.effect_color();

	return s;
}