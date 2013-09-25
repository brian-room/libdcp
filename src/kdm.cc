/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <iomanip>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <libcxml/cxml.h>
#include "AS_DCP.h"
#include "KM_util.h"
#include "util.h"
#include "kdm.h"
#include "compose.hpp"
#include "exceptions.h"
#include "signer.h"
#include "cpl.h"
#include "mxf_asset.h"
#include "xml/kdm_smpte.h"

using std::list;
using std::string;
using std::stringstream;
using std::hex;
using std::setw;
using std::setfill;
using std::cout;
using boost::shared_ptr;
using namespace libdcp;

KDM::KDM (boost::filesystem::path kdm, boost::filesystem::path private_key)
	: xml_kdm (new xml::DCinemaSecurityMessage (kdm))
{
	/* Read the private key */
	   
	FILE* private_key_file = fopen (private_key.string().c_str(), "r");
	if (!private_key_file) {
		throw FileError ("could not find RSA private key file", private_key);
	}
	
	RSA* rsa = PEM_read_RSAPrivateKey (private_key_file, 0, 0, 0);
	fclose (private_key_file);	
	if (!rsa) {
		throw FileError ("could not read RSA private key file", private_key);
	}

	/* Use it to decrypt the keys */

	list<string> encrypted_keys = xml_kdm->authenticated_private.encrypted_keys;

	for (list<string>::iterator i = encrypted_keys.begin(); i != encrypted_keys.end(); ++i) {

		/* Decode the base-64-encoded cipher value from the KDM */
		unsigned char cipher_value[256];
		int const cipher_value_len = base64_decode (*i, cipher_value, sizeof (cipher_value));

		/* Decrypt it */
		unsigned char* decrypted = new unsigned char[RSA_size(rsa)];
		int const decrypted_len = RSA_private_decrypt (cipher_value_len, cipher_value, decrypted, rsa, RSA_PKCS1_OAEP_PADDING);
		if (decrypted_len == -1) {
			delete[] decrypted;
			throw MiscError (String::compose ("Could not decrypt KDM (%1)", ERR_error_string (ERR_get_error(), 0)));
		}

		_keys.push_back (KDMKey (decrypted, decrypted_len));
		delete[] decrypted;
	}

	RSA_free (rsa);
}

KDM::KDM (
	shared_ptr<const CPL> cpl, shared_ptr<const Signer> signer, shared_ptr<const Certificate> recipient_cert,
	boost::posix_time::ptime not_valid_before, boost::posix_time::ptime not_valid_after,
	string annotation_text, string issue_date
	)
	: xml_kdm (new xml::DCinemaSecurityMessage)
{
	xml::AuthenticatedPublic& apu = xml_kdm->authenticated_public;

	/* AuthenticatedPublic */

	apu.message_id = "urn:uuid:" + make_uuid ();
	apu.message_type = "http://www.smpte-ra.org/430-1/2006/KDM#kdm-key-type";
	apu.annotation_text = annotation_text;
	apu.issue_date = issue_date;
	apu.signer.x509_issuer_name = signer->certificates().leaf()->issuer ();
	apu.signer.x509_serial_number = signer->certificates().leaf()->serial ();
	apu.recipient.x509_issuer_serial.x509_issuer_name = recipient_cert->issuer ();
	apu.recipient.x509_issuer_serial.x509_serial_number = recipient_cert->serial ();
	apu.recipient.x509_subject_name = recipient_cert->subject ();
	apu.composition_playlist_id = "urn:uuid:" + cpl->id ();
	apu.content_title_text = cpl->name ();
	apu.content_keys_not_valid_before = ptime_to_string (not_valid_before);
	apu.content_keys_not_valid_after = ptime_to_string (not_valid_after);
	apu.authorized_device_info.device_list_identifier = "urn:uuid:" + make_uuid ();
	apu.authorized_device_info.device_list_description = recipient_cert->subject ();
	apu.authorized_device_info.device_list.push_back (recipient_cert->thumbprint ());
	
	list<shared_ptr<const Asset> > assets = cpl->assets ();
	for (list<shared_ptr<const Asset> >::iterator i = assets.begin(); i != assets.end(); ++i) {
		/* XXX: non-MXF assets? */
		shared_ptr<const MXFAsset> mxf = boost::dynamic_pointer_cast<const MXFAsset> (*i);
		if (mxf) {
			apu.key_id_list.push_back (xml::TypedKeyId (mxf->key_type(), "urn:uuid:" + mxf->key_id()));
		}
	}

	apu.forensic_mark_flag_list.push_back ("http://www.smpte-ra.org/430-1/2006/KDM#mrkflg-picture-disable");
	apu.forensic_mark_flag_list.push_back ("http://www.smpte-ra.org/430-1/2006/KDM#mrkflg-audio-disable");

	/* AuthenticatedPrivate */

	for (list<shared_ptr<const Asset> >::iterator i = assets.begin(); i != assets.end(); ++i) {
		/* XXX: non-MXF assets? */
		shared_ptr<const MXFAsset> mxf = boost::dynamic_pointer_cast<const MXFAsset> (*i);
		if (mxf) {
			xml_kdm->authenticated_private.encrypted_keys.push_back (
				KDMKey (
					signer, cpl->id (), mxf->key_type (), mxf->key_id (),
					not_valid_before, not_valid_after, mxf->key().get()
					).encrypted_base64 (recipient_cert)
				);
		}
	}

	/* Signature */

	shared_ptr<xmlpp::Document> doc = xml_kdm->as_xml ();
	shared_ptr<cxml::Node> root (new cxml::Node (doc->get_root_node ()));
	xmlpp::Node* signature = root->node_child("Signature")->node();
	signer->add_signature_value (signature, "ds");
	xml_kdm->signature = xml::Signature (shared_ptr<cxml::Node> (new cxml::Node (signature)));
}

void
KDM::as_xml (boost::filesystem::path path) const
{
	shared_ptr<xmlpp::Document> doc = xml_kdm->as_xml ();
	doc->write_to_file_formatted (path.string(), "UTF-8");
}

string
KDM::as_xml () const
{
	shared_ptr<xmlpp::Document> doc = xml_kdm->as_xml ();
	return doc->write_to_string_formatted ("UTF-8");
}

KDMKey::KDMKey (
	shared_ptr<const Signer> signer, string cpl_id, string key_type, string key_id, boost::posix_time::ptime from, boost::posix_time::ptime until, Key key
	)
	: _cpl_id (cpl_id)
	, _key_type (key_type)
	, _key_id (key_id)
	, _not_valid_before (ptime_to_string (from))
	, _not_valid_after (ptime_to_string (until))
	, _key (key)
{
	base64_decode (signer->certificates().leaf()->thumbprint (), _signer_thumbprint, 20);
}

KDMKey::KDMKey (uint8_t const * raw, int len)
{
	switch (len) {
	case 134:
		/* interop */
		/* [0-15] is structure id (fixed sequence specified by standard) */
		raw += 16;
		get (_signer_thumbprint, &raw, 20);
		_cpl_id = get_uuid (&raw);
		_key_id = get_uuid (&raw);
		_not_valid_before = get (&raw, 25);
		_not_valid_after = get (&raw, 25);
		_key = Key (raw);
		break;
	case 138:
		/* SMPTE */
		/* [0-15] is structure id (fixed sequence specified by standard) */
		raw += 16;
		get (_signer_thumbprint, &raw, 20);
		_cpl_id = get_uuid (&raw);
		_key_type = get (&raw, 4);
		_key_id = get_uuid (&raw);
		_not_valid_before = get (&raw, 25);
		_not_valid_after = get (&raw, 25);
		_key = Key (raw);
		break;
	default:
		assert (false);
	}
}

KDMKey::KDMKey (KDMKey const & other)
	: _cpl_id (other._cpl_id)
	, _key_type (other._key_type)
	, _key_id (other._key_id)
	, _not_valid_before (other._not_valid_before)
	, _not_valid_after (other._not_valid_after)
	, _key (other._key)
{
	memcpy (_signer_thumbprint, other._signer_thumbprint, 20);
}

KDMKey &
KDMKey::operator= (KDMKey const & other)
{
	if (&other == this) {
		return *this;
	}
	
	_cpl_id = other._cpl_id;
	_key_type = other._key_type;
	_key_id = other._key_id;
	_not_valid_before = other._not_valid_before;
	_not_valid_after = other._not_valid_after;
	_key = other._key;
	memcpy (_signer_thumbprint, other._signer_thumbprint, 20);

	return *this;
}

string
KDMKey::encrypted_base64 (shared_ptr<const Certificate> recipient_cert) const
{
	assert (_key_type.length() == 4);
	assert (_not_valid_before.length() == 25);
	assert (_not_valid_after.length() == 25);
	
	/* XXX: SMPTE only */
	uint8_t block[138];
	uint8_t* p = block;

	/* Magic value specified by SMPTE S430-1-2006 */
	uint8_t structure_id[] = { 0xf1, 0xdc, 0x12, 0x44, 0x60, 0x16, 0x9a, 0x0e, 0x85, 0xbc, 0x30, 0x06, 0x42, 0xf8, 0x66, 0xab };
	put (&p, structure_id, 16);
	put (&p, _signer_thumbprint, 20);
	put_uuid (&p, _cpl_id);
	put (&p, _key_type);
	put_uuid (&p, _key_id);
	put (&p, _not_valid_before);
	put (&p, _not_valid_after);
	put (&p, _key.value(), ASDCP::KeyLen);

	/* Encrypt using the projector's public key */
	RSA* rsa = recipient_cert->public_key ();
	unsigned char encrypted[RSA_size(rsa)];
	int const encrypted_len = RSA_public_encrypt (p - block, block, encrypted, rsa, RSA_PKCS1_OAEP_PADDING);
	if (encrypted_len == -1) {
		throw MiscError (String::compose ("Could not encrypt KDM (%1)", ERR_error_string (ERR_get_error(), 0)));
	}

	/* Lazy overallocation */
	char out[encrypted_len * 2];
	return Kumu::base64encode (encrypted, encrypted_len, out, encrypted_len * 2);
}

string
KDMKey::get (uint8_t const ** p, int N) const
{
	string g;
	for (int i = 0; i < N; ++i) {
		g += **p;
		(*p)++;
	}

	return g;
}

void
KDMKey::get (uint8_t* o, uint8_t const ** p, int N) const
{
	memcpy (o, *p, N);
	*p += N;
}

string
KDMKey::get_uuid (unsigned char const ** p) const
{
	stringstream g;
	
	for (int i = 0; i < 16; ++i) {
		g << setw(2) << setfill('0') << hex << static_cast<int> (**p);
		(*p)++;
		if (i == 3 || i == 5 || i == 7 || i == 9) {
			g << '-';
		}
	}

	return g.str ();
}

void
KDMKey::put (uint8_t ** d, uint8_t const * s, int N) const
{
	memcpy (*d, s, N);
	(*d) += N;
}

void
KDMKey::put (uint8_t ** d, string s) const
{
	memcpy (*d, s.c_str(), s.length());
	(*d) += s.length();
}

void
KDMKey::put_uuid (uint8_t ** d, string id) const
{
	id.erase (std::remove (id.begin(), id.end(), '-'));
	for (int i = 0; i < 32; i += 2) {
		stringstream s;
		s << id[i] << id[i + 1];
		int h;
		s >> h;
		**d = h;
		(*d)++;
	}
}
