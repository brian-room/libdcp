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

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations
    including the two.

    You must obey the GNU General Public License in all respects
    for all of the code used other than OpenSSL.  If you modify
    file(s) with this exception, you may extend this exception to your
    version of the file(s), but you are not obligated to do so.  If you
    do not wish to do so, delete this exception statement from your
    version.  If you delete this exception statement from all source
    files in the program, then also delete it here.
*/

#ifndef LIBDCP_SUBTITLE_ASSET_H
#define LIBDCP_SUBTITLE_ASSET_H

#include "asset.h"
#include "dcp_time.h"
#include "subtitle_string.h"
#include "data.h"
#include <libcxml/cxml.h>
#include <boost/shared_array.hpp>
#include <map>

namespace xmlpp {
	class Element;
}

struct interop_dcp_font_test;
struct smpte_dcp_font_test;
struct pull_fonts_test1;
struct pull_fonts_test2;
struct pull_fonts_test3;

namespace dcp
{

class SubtitleString;
class FontNode;
class TextNode;
class SubtitleNode;
class LoadFontNode;

namespace order {
	class Part;
	class Context;
}

/** @class SubtitleAsset
 *  @brief A parent for classes representing a file containing subtitles.
 *
 *  This class holds a list of SubtitleString objects which it can extract
 *  from the appropriate part of either an Interop or SMPTE XML file.
 *  Its subclasses InteropSubtitleAsset and SMPTESubtitleAsset handle the
 *  differences between the two types.
 */
class SubtitleAsset : public Asset
{
public:
	SubtitleAsset ();
	explicit SubtitleAsset (boost::filesystem::path file);

	bool equals (
		boost::shared_ptr<const Asset>,
		EqualityOptions,
		NoteHandler note
		) const;

	std::list<SubtitleString> subtitles_during (Time from, Time to, bool starting) const;
	std::list<SubtitleString> const & subtitles () const {
		return _subtitles;
	}

	virtual void add (SubtitleString);
	virtual void add_font (std::string id, boost::filesystem::path file) = 0;
	std::map<std::string, Data> fonts_with_load_ids () const;

	virtual void write (boost::filesystem::path) const = 0;
	virtual std::string xml_as_string () const = 0;

	Time latest_subtitle_out () const;

	virtual std::list<boost::shared_ptr<LoadFontNode> > load_font_nodes () const = 0;

protected:
	friend struct ::interop_dcp_font_test;
	friend struct ::smpte_dcp_font_test;

	struct ParseState {
		boost::optional<std::string> font_id;
		boost::optional<int64_t> size;
		boost::optional<float> aspect_adjust;
		boost::optional<bool> italic;
		boost::optional<bool> bold;
		boost::optional<bool> underline;
		boost::optional<Colour> colour;
		boost::optional<Effect> effect;
		boost::optional<Colour> effect_colour;
		boost::optional<float> h_position;
		boost::optional<HAlign> h_align;
		boost::optional<float> v_position;
		boost::optional<VAlign> v_align;
		boost::optional<Direction> direction;
		boost::optional<Time> in;
		boost::optional<Time> out;
		boost::optional<Time> fade_up_time;
		boost::optional<Time> fade_down_time;
	};

	void parse_subtitles (xmlpp::Element const * node, std::list<ParseState>& state, boost::optional<int> tcr, Standard standard);
	ParseState font_node_state (xmlpp::Element const * node, Standard standard) const;
	ParseState text_node_state (xmlpp::Element const * node) const;
	ParseState subtitle_node_state (xmlpp::Element const * node, boost::optional<int> tcr) const;
	Time fade_time (xmlpp::Element const * node, std::string name, boost::optional<int> tcr) const;

	void subtitles_as_xml (xmlpp::Element* root, int time_code_rate, Standard standard) const;

	/** All our subtitles, in no particular order */
	std::list<SubtitleString> _subtitles;

	class Font
	{
	public:
		Font (std::string load_id_, std::string uuid_, boost::filesystem::path file_)
			: load_id (load_id_)
			, uuid (uuid_)
			, data (file_)
			, file (file_)
		{}

		Font (std::string load_id_, std::string uuid_, Data data_)
			: load_id (load_id_)
			, uuid (uuid_)
			, data (data_)
		{}

		std::string load_id;
		std::string uuid;
		Data data;
		/** .ttf file that this data was last written to, if applicable */
		mutable boost::optional<boost::filesystem::path> file;
	};

	/** TTF font data that we need */
	std::list<Font> _fonts;

private:
	friend struct ::pull_fonts_test1;
	friend struct ::pull_fonts_test2;
	friend struct ::pull_fonts_test3;

	void maybe_add_subtitle (std::string text, std::list<ParseState> const & parse_state);

	static void pull_fonts (boost::shared_ptr<order::Part> part);
};

}

#endif
