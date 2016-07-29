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

/** @file  src/dcp.cc
 *  @brief DCP class.
 */

#include "raw_convert.h"
#include "dcp.h"
#include "sound_asset.h"
#include "picture_asset.h"
#include "interop_subtitle_asset.h"
#include "smpte_subtitle_asset.h"
#include "mono_picture_asset.h"
#include "stereo_picture_asset.h"
#include "reel_subtitle_asset.h"
#include "util.h"
#include "metadata.h"
#include "exceptions.h"
#include "cpl.h"
#include "certificate_chain.h"
#include "compose.hpp"
#include "decrypted_kdm.h"
#include "decrypted_kdm_key.h"
#include "dcp_assert.h"
#include "reel_asset.h"
#include "font_asset.h"
#include <asdcp/AS_DCP.h>
#include <xmlsec/xmldsig.h>
#include <xmlsec/app.h>
#include <libxml++/libxml++.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

using std::string;
using std::list;
using std::cout;
using std::make_pair;
using std::map;
using std::cout;
using std::cerr;
using std::exception;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::algorithm::starts_with;
using namespace dcp;

static string const assetmap_interop_ns = "http://www.digicine.com/PROTO-ASDCP-AM-20040311#";
static string const assetmap_smpte_ns   = "http://www.smpte-ra.org/schemas/429-9/2007/AM";
static string const pkl_interop_ns      = "http://www.digicine.com/PROTO-ASDCP-PKL-20040311#";
static string const pkl_smpte_ns        = "http://www.smpte-ra.org/schemas/429-8/2007/PKL";
static string const volindex_interop_ns = "http://www.digicine.com/PROTO-ASDCP-AM-20040311#";
static string const volindex_smpte_ns   = "http://www.smpte-ra.org/schemas/429-9/2007/AM";

DCP::DCP (boost::filesystem::path directory)
	: _directory (directory)
{
	if (!boost::filesystem::exists (directory)) {
		boost::filesystem::create_directories (directory);
	}

	_directory = boost::filesystem::canonical (_directory);
}

/** Call this instead of throwing an exception if the error can be tolerated */
template<class T> void
survivable_error (bool keep_going, dcp::DCP::ReadErrors* errors, T const & e)
{
	if (keep_going) {
		if (errors) {
			errors->push_back (shared_ptr<T> (new T (e)));
		}
	} else {
		throw e;
	}
}

void
DCP::read (bool keep_going, ReadErrors* errors, bool ignore_incorrect_picture_mxf_type)
{
	/* Read the ASSETMAP */

	boost::filesystem::path asset_map_file;
	if (boost::filesystem::exists (_directory / "ASSETMAP")) {
		asset_map_file = _directory / "ASSETMAP";
	} else if (boost::filesystem::exists (_directory / "ASSETMAP.xml")) {
		asset_map_file = _directory / "ASSETMAP.xml";
	} else {
		boost::throw_exception (DCPReadError (String::compose ("could not find AssetMap file in `%1'", _directory.string())));
	}

	cxml::Document asset_map ("AssetMap");

	asset_map.read_file (asset_map_file);
	if (asset_map.namespace_uri() == assetmap_interop_ns) {
		_standard = INTEROP;
	} else if (asset_map.namespace_uri() == assetmap_smpte_ns) {
		_standard = SMPTE;
	} else {
		boost::throw_exception (XMLError ("Unrecognised Assetmap namespace " + asset_map.namespace_uri()));
	}

	list<shared_ptr<cxml::Node> > asset_nodes = asset_map.node_child("AssetList")->node_children ("Asset");
	map<string, boost::filesystem::path> paths;
	BOOST_FOREACH (shared_ptr<cxml::Node> i, asset_nodes) {
		if (i->node_child("ChunkList")->node_children("Chunk").size() != 1) {
			boost::throw_exception (XMLError ("unsupported asset chunk count"));
		}
		string p = i->node_child("ChunkList")->node_child("Chunk")->string_child ("Path");
		if (starts_with (p, "file://")) {
			p = p.substr (7);
		}
		paths.insert (make_pair (remove_urn_uuid (i->string_child ("Id")), p));
	}

	/* Read all the assets from the asset map */
	/* XXX: I think we should be looking at the PKL here to decide type, not
	   the extension of the file.
	*/

	/* Make a list of non-CPL assets so that we can resolve the references
	   from the CPLs.
	*/
	list<shared_ptr<Asset> > other_assets;

	for (map<string, boost::filesystem::path>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
		boost::filesystem::path path = _directory / i->second;

		if (!boost::filesystem::exists (path)) {
			survivable_error (keep_going, errors, MissingAssetError (path));
			continue;
		}

		if (boost::filesystem::extension (path) == ".xml") {
			xmlpp::DomParser* p = new xmlpp::DomParser;
			try {
				p->parse_file (path.string());
			} catch (std::exception& e) {
				delete p;
				continue;
			}

			string const root = p->get_document()->get_root_node()->get_name ();
			delete p;

			if (root == "CompositionPlaylist") {
				shared_ptr<CPL> cpl (new CPL (path));
				if (_standard && cpl->standard() && cpl->standard().get() != _standard.get()) {
					survivable_error (keep_going, errors, MismatchedStandardError ());
				}
				_cpls.push_back (cpl);
			} else if (root == "DCSubtitle") {
				if (_standard && _standard.get() == SMPTE) {
					survivable_error (keep_going, errors, MismatchedStandardError ());
				}
				other_assets.push_back (shared_ptr<InteropSubtitleAsset> (new InteropSubtitleAsset (path)));
			}
		} else if (boost::filesystem::extension (path) == ".mxf") {

			/* XXX: asdcplib does not appear to support discovery of read MXFs standard
			   (Interop / SMPTE)
			*/

			ASDCP::EssenceType_t type;
			if (ASDCP::EssenceType (path.string().c_str(), type) != ASDCP::RESULT_OK) {
				throw DCPReadError ("Could not find essence type");
			}
			switch (type) {
				case ASDCP::ESS_UNKNOWN:
				case ASDCP::ESS_MPEG2_VES:
					throw DCPReadError ("MPEG2 video essences are not supported");
				case ASDCP::ESS_JPEG_2000:
					try {
						other_assets.push_back (shared_ptr<MonoPictureAsset> (new MonoPictureAsset (path)));
					} catch (dcp::MXFFileError& e) {
						if (ignore_incorrect_picture_mxf_type && e.number() == ASDCP::RESULT_SFORMAT) {
							/* Tried to load it as mono but the error says it's stereo; try that instead */
							other_assets.push_back (shared_ptr<StereoPictureAsset> (new StereoPictureAsset (path)));
						} else {
							throw;
						}
					}
					break;
				case ASDCP::ESS_PCM_24b_48k:
				case ASDCP::ESS_PCM_24b_96k:
					other_assets.push_back (shared_ptr<SoundAsset> (new SoundAsset (path)));
					break;
				case ASDCP::ESS_JPEG_2000_S:
					other_assets.push_back (shared_ptr<StereoPictureAsset> (new StereoPictureAsset (path)));
					break;
				case ASDCP::ESS_TIMED_TEXT:
					other_assets.push_back (shared_ptr<SMPTESubtitleAsset> (new SMPTESubtitleAsset (path)));
					break;
				default:
					throw DCPReadError ("Unknown MXF essence type");
			}
		} else if (boost::filesystem::extension (path) == ".ttf") {
			other_assets.push_back (shared_ptr<FontAsset> (new FontAsset (i->first, path)));
		}
	}

	BOOST_FOREACH (shared_ptr<CPL> i, cpls ()) {
		i->resolve_refs (other_assets);
	}
}

void
DCP::resolve_refs (list<shared_ptr<Asset> > assets)
{
	BOOST_FOREACH (shared_ptr<CPL> i, cpls ()) {
		i->resolve_refs (assets);
	}
}

bool
DCP::equals (DCP const & other, EqualityOptions opt, NoteHandler note) const
{
	list<shared_ptr<CPL> > a = cpls ();
	list<shared_ptr<CPL> > b = other.cpls ();

	if (a.size() != b.size()) {
		note (DCP_ERROR, String::compose ("CPL counts differ: %1 vs %2", a.size(), b.size()));
		return false;
	}

	bool r = true;

	BOOST_FOREACH (shared_ptr<CPL> i, a) {
		list<shared_ptr<CPL> >::const_iterator j = b.begin ();
		while (j != b.end() && !(*j)->equals (i, opt, note)) {
			++j;
		}

		if (j == b.end ()) {
			r = false;
		}
	}

	return r;
}

void
DCP::add (boost::shared_ptr<CPL> cpl)
{
	_cpls.push_back (cpl);
}

bool
DCP::encrypted () const
{
	BOOST_FOREACH (shared_ptr<CPL> i, cpls ()) {
		if (i->encrypted ()) {
			return true;
		}
	}

	return false;
}

void
DCP::add (DecryptedKDM const & kdm)
{
	list<DecryptedKDMKey> keys = kdm.keys ();

	BOOST_FOREACH (shared_ptr<CPL> i, cpls ()) {
		BOOST_FOREACH (DecryptedKDMKey const & j, kdm.keys ()) {
			if (j.cpl_id() == i->id()) {
				i->add (kdm);
			}
		}
	}
}

boost::filesystem::path
DCP::write_pkl (string file, Standard standard, string pkl_uuid, XMLMetadata metadata, shared_ptr<const CertificateChain> signer) const
{
	boost::filesystem::path p = _directory;
	p /= file;

	xmlpp::Document doc;
	xmlpp::Element* pkl;
	if (standard == INTEROP) {
		pkl = doc.create_root_node("PackingList", pkl_interop_ns);
	} else {
		pkl = doc.create_root_node("PackingList", pkl_smpte_ns);
	}

	if (signer) {
		pkl->set_namespace_declaration ("http://www.w3.org/2000/09/xmldsig#", "dsig");
	}

	pkl->add_child("Id")->add_child_text ("urn:uuid:" + pkl_uuid);

	/* XXX: this is a bit of a hack */
	DCP_ASSERT (cpls().size() > 0);
	pkl->add_child("AnnotationText")->add_child_text (cpls().front()->annotation_text ());

	pkl->add_child("IssueDate")->add_child_text (metadata.issue_date);
	pkl->add_child("Issuer")->add_child_text (metadata.issuer);
	pkl->add_child("Creator")->add_child_text (metadata.creator);

	xmlpp::Element* asset_list = pkl->add_child("AssetList");
	BOOST_FOREACH (shared_ptr<Asset> i, assets ()) {
		i->write_to_pkl (asset_list, _directory, standard);
	}

	if (signer) {
		signer->sign (pkl, standard);
	}

	doc.write_to_file (p.string (), "UTF-8");
	return p.string ();
}

/** Write the VOLINDEX file.
 *  @param standard DCP standard to use (INTEROP or SMPTE)
 */
void
DCP::write_volindex (Standard standard) const
{
	boost::filesystem::path p = _directory;
	switch (standard) {
	case INTEROP:
		p /= "VOLINDEX";
		break;
	case SMPTE:
		p /= "VOLINDEX.xml";
		break;
	default:
		DCP_ASSERT (false);
	}

	xmlpp::Document doc;
	xmlpp::Element* root;

	switch (standard) {
	case INTEROP:
		root = doc.create_root_node ("VolumeIndex", volindex_interop_ns);
		break;
	case SMPTE:
		root = doc.create_root_node ("VolumeIndex", volindex_smpte_ns);
		break;
	default:
		DCP_ASSERT (false);
	}

	root->add_child("Index")->add_child_text ("1");
	doc.write_to_file (p.string (), "UTF-8");
}

void
DCP::write_assetmap (Standard standard, string pkl_uuid, int pkl_length, XMLMetadata metadata) const
{
	boost::filesystem::path p = _directory;

	switch (standard) {
	case INTEROP:
		p /= "ASSETMAP";
		break;
	case SMPTE:
		p /= "ASSETMAP.xml";
		break;
	default:
		DCP_ASSERT (false);
	}

	xmlpp::Document doc;
	xmlpp::Element* root;

	switch (standard) {
	case INTEROP:
		root = doc.create_root_node ("AssetMap", assetmap_interop_ns);
		break;
	case SMPTE:
		root = doc.create_root_node ("AssetMap", assetmap_smpte_ns);
		break;
	default:
		DCP_ASSERT (false);
	}

	root->add_child("Id")->add_child_text ("urn:uuid:" + make_uuid());
	root->add_child("AnnotationText")->add_child_text ("Created by " + metadata.creator);

	switch (standard) {
	case INTEROP:
		root->add_child("VolumeCount")->add_child_text ("1");
		root->add_child("IssueDate")->add_child_text (metadata.issue_date);
		root->add_child("Issuer")->add_child_text (metadata.issuer);
		root->add_child("Creator")->add_child_text (metadata.creator);
		break;
	case SMPTE:
		root->add_child("Creator")->add_child_text (metadata.creator);
		root->add_child("VolumeCount")->add_child_text ("1");
		root->add_child("IssueDate")->add_child_text (metadata.issue_date);
		root->add_child("Issuer")->add_child_text (metadata.issuer);
		break;
	default:
		DCP_ASSERT (false);
	}

	xmlpp::Node* asset_list = root->add_child ("AssetList");

	xmlpp::Node* asset = asset_list->add_child ("Asset");
	asset->add_child("Id")->add_child_text ("urn:uuid:" + pkl_uuid);
	asset->add_child("PackingList")->add_child_text ("true");
	xmlpp::Node* chunk_list = asset->add_child ("ChunkList");
	xmlpp::Node* chunk = chunk_list->add_child ("Chunk");
	chunk->add_child("Path")->add_child_text ("pkl_" + pkl_uuid + ".xml");
	chunk->add_child("VolumeIndex")->add_child_text ("1");
	chunk->add_child("Offset")->add_child_text ("0");
	chunk->add_child("Length")->add_child_text (raw_convert<string> (pkl_length));

	BOOST_FOREACH (shared_ptr<Asset> i, assets ()) {
		i->write_to_assetmap (asset_list, _directory);
	}

	/* This must not be the _formatted version otherwise signature digests will be wrong */
	doc.write_to_file (p.string (), "UTF-8");
}

/** Write all the XML files for this DCP.
 *  @param standand INTEROP or SMPTE.
 *  @param metadata Metadata to use for PKL and asset map files.
 *  @param signer Signer to use, or 0.
 */
void
DCP::write_xml (
	Standard standard,
	XMLMetadata metadata,
	shared_ptr<const CertificateChain> signer,
	FilenameFormat filename_format
	)
{
	BOOST_FOREACH (shared_ptr<CPL> i, cpls ()) {
		NameFormat::Map values;
		values['t'] = "cpl";
		values['i'] = i->id();
		i->write_xml (_directory / (filename_format.get(values) + ".xml"), standard, signer);
	}

	string const pkl_uuid = make_uuid ();
	NameFormat::Map values;
	values['t'] = "pkl";
	values['i'] = pkl_uuid;
	boost::filesystem::path const pkl_path = write_pkl (filename_format.get(values) + ".xml", standard, pkl_uuid, metadata, signer);

	write_volindex (standard);
	write_assetmap (standard, pkl_uuid, boost::filesystem::file_size (pkl_path), metadata);
}

list<shared_ptr<CPL> >
DCP::cpls () const
{
	return _cpls;
}

/** @return All assets (including CPLs) */
list<shared_ptr<Asset> >
DCP::assets () const
{
	list<shared_ptr<Asset> > assets;
	BOOST_FOREACH (shared_ptr<CPL> i, cpls ()) {
		assets.push_back (i);
		BOOST_FOREACH (shared_ptr<const ReelAsset> j, i->reel_assets ()) {
			shared_ptr<Asset> o = j->asset_ref().asset ();
			assets.push_back (o);
			/* More Interop special-casing */
			shared_ptr<InteropSubtitleAsset> sub = dynamic_pointer_cast<InteropSubtitleAsset> (o);
			if (sub) {
				sub->add_font_assets (assets);
			}
		}
	}

	return assets;
}
