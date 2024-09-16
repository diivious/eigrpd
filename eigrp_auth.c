/*
 * EIGRP Authnication functions to support sending and receiving of EIGRP Packets.
 * Copyright (C) 2023
 * Authors:
 *   Donnie Savage
 */
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_auth.h"

DEFINE_MTYPE_STATIC(EIGRPD, EIGRP_AUTH_TLV,        "EIGRP AUTH TLV");
DEFINE_MTYPE_STATIC(EIGRPD, EIGRP_AUTH_SHA256_TLV, "EIGRP SHA TLV");

static unsigned char zeropad[16] = {0};

int eigrp_make_md5_digest(eigrp_interface_t *ei, struct stream *s,
			  uint8_t flags)
{
	struct key *key = NULL;
	struct keychain *keychain;

	unsigned char digest[EIGRP_AUTH_TYPE_MD5_LEN];
	MD5_CTX ctx;
	uint8_t *ibuf;
	size_t backup_get, backup_end;
	struct TLV_MD5_Authentication_Type *auth_TLV;

	ibuf = s->data;
	backup_end = s->endp;
	backup_get = s->getp;

	auth_TLV = eigrp_authTLV_MD5_new();

	stream_set_getp(s, EIGRP_HEADER_LEN);
	stream_get(auth_TLV, s, EIGRP_AUTH_MD5_TLV_SIZE);
	stream_set_getp(s, backup_get);

	keychain = keychain_lookup(ei->params.auth_keychain);
	if (keychain)
		key = key_lookup_for_send(keychain);
	else {
		eigrp_authTLV_MD5_free(auth_TLV);
		return EIGRP_AUTH_TYPE_NONE;
	}

	memset(&ctx, 0, sizeof(ctx));
	MD5Init(&ctx);

	/* Generate a digest. Each situation needs different handling */
	if (flags & EIGRP_AUTH_BASIC_HELLO_FLAG) {
		MD5Update(&ctx, ibuf, EIGRP_MD5_BASIC_COMPUTE);
		MD5Update(&ctx, key->string, strlen(key->string));
		if (strlen(key->string) < 16)
			MD5Update(&ctx, zeropad, 16 - strlen(key->string));
	} else if (flags & EIGRP_AUTH_UPDATE_INIT_FLAG) {
		MD5Update(&ctx, ibuf, EIGRP_MD5_UPDATE_INIT_COMPUTE);
	} else if (flags & EIGRP_AUTH_UPDATE_FLAG) {
		MD5Update(&ctx, ibuf, EIGRP_MD5_BASIC_COMPUTE);
		MD5Update(&ctx, key->string, strlen(key->string));
		if (strlen(key->string) < 16)
			MD5Update(&ctx, zeropad, 16 - strlen(key->string));
		if (backup_end > (EIGRP_HEADER_LEN + EIGRP_AUTH_MD5_TLV_SIZE)) {
			MD5Update(&ctx,
				  ibuf
					  + (EIGRP_HEADER_LEN
					     + EIGRP_AUTH_MD5_TLV_SIZE),
				  backup_end - 20
					  - (EIGRP_HEADER_LEN
					     + EIGRP_AUTH_MD5_TLV_SIZE));
		}
	}

	MD5Final(digest, &ctx);

	/* Append md5 digest to the end of the stream. */
	memcpy(auth_TLV->digest, digest, EIGRP_AUTH_TYPE_MD5_LEN);

	stream_set_endp(s, EIGRP_HEADER_LEN);
	stream_put(s, auth_TLV, EIGRP_AUTH_MD5_TLV_SIZE);
	stream_set_endp(s, backup_end);

	eigrp_authTLV_MD5_free(auth_TLV);
	return EIGRP_AUTH_TYPE_MD5_LEN;
}

int eigrp_check_md5_digest(struct stream *s,
			   struct TLV_MD5_Authentication_Type *authTLV,
			   eigrp_neighbor_t *nbr, uint8_t flags)
{
	MD5_CTX ctx;
	unsigned char digest[EIGRP_AUTH_TYPE_MD5_LEN];
	unsigned char orig[EIGRP_AUTH_TYPE_MD5_LEN];
	struct key *key = NULL;
	struct keychain *keychain;
	uint8_t *ibuf;
	size_t backup_end;
	struct TLV_MD5_Authentication_Type *auth_TLV;
	struct eigrp_header *eigrph;

	if (ntohl(nbr->crypt_seqnum) > ntohl(authTLV->key_sequence)) {
		zlog_warn(
			"interface %s: eigrp_check_md5 bad sequence %d (expect %d)",
			EIGRP_INTF_NAME(nbr->ei), ntohl(authTLV->key_sequence),
			ntohl(nbr->crypt_seqnum));
		return 0;
	}

	eigrph = (struct eigrp_header *)s->data;
	eigrph->checksum = 0;

	auth_TLV = (struct TLV_MD5_Authentication_Type *)(s->data
							  + EIGRP_HEADER_LEN);
	memcpy(orig, auth_TLV->digest, EIGRP_AUTH_TYPE_MD5_LEN);
	memset(digest, 0, EIGRP_AUTH_TYPE_MD5_LEN);
	memset(auth_TLV->digest, 0, EIGRP_AUTH_TYPE_MD5_LEN);

	ibuf = s->data;
	backup_end = s->endp;

	keychain = keychain_lookup(nbr->ei->params.auth_keychain);
	if (keychain)
		key = key_lookup_for_send(keychain);

	if (!key) {
		zlog_warn(
			"Interface %s: Expected key value not found in config",
			nbr->ei->ifp->name);
		return 0;
	}

	memset(&ctx, 0, sizeof(ctx));
	MD5Init(&ctx);

	/* Generate a digest. Each situation needs different handling */
	if (flags & EIGRP_AUTH_BASIC_HELLO_FLAG) {
		MD5Update(&ctx, ibuf, EIGRP_MD5_BASIC_COMPUTE);
		MD5Update(&ctx, key->string, strlen(key->string));
		if (strlen(key->string) < 16)
			MD5Update(&ctx, zeropad, 16 - strlen(key->string));
	} else if (flags & EIGRP_AUTH_UPDATE_INIT_FLAG) {
		MD5Update(&ctx, ibuf, EIGRP_MD5_UPDATE_INIT_COMPUTE);
	} else if (flags & EIGRP_AUTH_UPDATE_FLAG) {
		MD5Update(&ctx, ibuf, EIGRP_MD5_BASIC_COMPUTE);
		MD5Update(&ctx, key->string, strlen(key->string));
		if (strlen(key->string) < 16)
			MD5Update(&ctx, zeropad, 16 - strlen(key->string));
		if (backup_end > (EIGRP_HEADER_LEN + EIGRP_AUTH_MD5_TLV_SIZE)) {
			MD5Update(&ctx,
				  ibuf
					  + (EIGRP_HEADER_LEN
					     + EIGRP_AUTH_MD5_TLV_SIZE),
				  backup_end - 20
					  - (EIGRP_HEADER_LEN
					     + EIGRP_AUTH_MD5_TLV_SIZE));
		}
	}

	MD5Final(digest, &ctx);

	/* compare the two */
	if (memcmp(orig, digest, EIGRP_AUTH_TYPE_MD5_LEN) != 0) {
		zlog_warn("interface %s: eigrp_check_md5 checksum mismatch",
			  EIGRP_INTF_NAME(nbr->ei));
		return 0;
	}

	/* save neighbor's crypt_seqnum */
	nbr->crypt_seqnum = authTLV->key_sequence;

	return 1;
}

int eigrp_make_sha256_digest(eigrp_interface_t *ei, struct stream *s,
			     uint8_t flags)
{
	struct key *key = NULL;
	struct keychain *keychain;
	char source_ip[PREFIX_STRLEN];

	unsigned char digest[EIGRP_AUTH_TYPE_SHA256_LEN];
	unsigned char buffer[1 + PLAINTEXT_LENGTH + 45 + 1] = {0};

	HMAC_SHA256_CTX ctx;
	void *ibuf;
	size_t backup_get, backup_end;
	struct TLV_SHA256_Authentication_Type *auth_TLV;

	ibuf = s->data;
	backup_end = s->endp;
	backup_get = s->getp;

	auth_TLV = eigrp_authTLV_SHA256_new();

	stream_set_getp(s, EIGRP_HEADER_LEN);
	stream_get(auth_TLV, s, EIGRP_AUTH_SHA256_TLV_SIZE);
	stream_set_getp(s, backup_get);

	keychain = keychain_lookup(ei->params.auth_keychain);
	if (keychain)
		key = key_lookup_for_send(keychain);

	if (!key) {
		zlog_warn(
			"Interface %s: Expected key value not found in config",
			ei->ifp->name);
		eigrp_authTLV_SHA256_free(auth_TLV);
		return 0;
	}

	inet_ntop(AF_INET, &ei->address.u.prefix4, source_ip, PREFIX_STRLEN);

	memset(&ctx, 0, sizeof(ctx));
	buffer[0] = '\n';
	memcpy(buffer + 1, key, strlen(key->string));
	memcpy(buffer + 1 + strlen(key->string), source_ip, strlen(source_ip));
	HMAC__SHA256_Init(&ctx, buffer,
			  1 + strlen(key->string) + strlen(source_ip));
	HMAC__SHA256_Update(&ctx, ibuf, strlen(ibuf));
	HMAC__SHA256_Final(digest, &ctx);


	/* Put hmac-sha256 digest to it's place */
	memcpy(auth_TLV->digest, digest, EIGRP_AUTH_TYPE_SHA256_LEN);

	stream_set_endp(s, EIGRP_HEADER_LEN);
	stream_put(s, auth_TLV, EIGRP_AUTH_SHA256_TLV_SIZE);
	stream_set_endp(s, backup_end);

	eigrp_authTLV_SHA256_free(auth_TLV);

	return EIGRP_AUTH_TYPE_SHA256_LEN;
}

int eigrp_check_sha256_digest(struct stream *s,
			      struct TLV_SHA256_Authentication_Type *authTLV,
			      eigrp_neighbor_t *nbr, uint8_t flags)
{
	return 1;
}

uint16_t eigrp_add_authTLV_MD5_encode(struct stream *s, eigrp_interface_t *ei)
{
	struct key *key;
	struct keychain *keychain;
	struct TLV_MD5_Authentication_Type *authTLV;

	authTLV = eigrp_authTLV_MD5_new();

	authTLV->type = htons(EIGRP_TLV_AUTH);
	authTLV->length = htons(EIGRP_AUTH_MD5_TLV_SIZE);
	authTLV->auth_type = htons(EIGRP_AUTH_TYPE_MD5);
	authTLV->auth_length = htons(EIGRP_AUTH_TYPE_MD5_LEN);
	authTLV->key_sequence = 0;
	memset(authTLV->Nullpad, 0, sizeof(authTLV->Nullpad));

	keychain = keychain_lookup(ei->params.auth_keychain);
	if (keychain)
		key = key_lookup_for_send(keychain);
	else {
		free(ei->params.auth_keychain);
		ei->params.auth_keychain = NULL;
		eigrp_authTLV_MD5_free(authTLV);
		return 0;
	}

	if (key) {
		authTLV->key_id = htonl(key->index);
		memset(authTLV->digest, 0, EIGRP_AUTH_TYPE_MD5_LEN);
		stream_put(s, authTLV,
			   sizeof(struct TLV_MD5_Authentication_Type));
		eigrp_authTLV_MD5_free(authTLV);
		return EIGRP_AUTH_MD5_TLV_SIZE;
	}

	eigrp_authTLV_MD5_free(authTLV);

	return 0;
}

uint16_t eigrp_add_authTLV_SHA256_encode(struct stream *s,
					 eigrp_interface_t *ei)
{
	struct key *key;
	struct keychain *keychain;
	struct TLV_SHA256_Authentication_Type *authTLV;

	authTLV = eigrp_authTLV_SHA256_new();

	authTLV->type = htons(EIGRP_TLV_AUTH);
	authTLV->length = htons(EIGRP_AUTH_SHA256_TLV_SIZE);
	authTLV->auth_type = htons(EIGRP_AUTH_TYPE_SHA256);
	authTLV->auth_length = htons(EIGRP_AUTH_TYPE_SHA256_LEN);
	authTLV->key_sequence = 0;
	memset(authTLV->Nullpad, 0, sizeof(authTLV->Nullpad));

	keychain = keychain_lookup(ei->params.auth_keychain);
	if (keychain)
		key = key_lookup_for_send(keychain);
	else {
		free(ei->params.auth_keychain);
		ei->params.auth_keychain = NULL;
		eigrp_authTLV_SHA256_free(authTLV);
		return 0;
	}

	if (key) {
		authTLV->key_id = 0;
		memset(authTLV->digest, 0, EIGRP_AUTH_TYPE_SHA256_LEN);
		stream_put(s, authTLV,
			   sizeof(struct TLV_SHA256_Authentication_Type));
		eigrp_authTLV_SHA256_free(authTLV);
		return EIGRP_AUTH_SHA256_TLV_SIZE;
	}

	eigrp_authTLV_SHA256_free(authTLV);

	return 0;
}

struct TLV_MD5_Authentication_Type *eigrp_authTLV_MD5_new(void)
{
	struct TLV_MD5_Authentication_Type *new;

	new = XCALLOC(MTYPE_EIGRP_AUTH_TLV,
		      sizeof(struct TLV_MD5_Authentication_Type));

	return new;
}

void eigrp_authTLV_MD5_free(struct TLV_MD5_Authentication_Type *authTLV)
{
	XFREE(MTYPE_EIGRP_AUTH_TLV, authTLV);
}

struct TLV_SHA256_Authentication_Type *eigrp_authTLV_SHA256_new(void)
{
	struct TLV_SHA256_Authentication_Type *new;

	new = XCALLOC(MTYPE_EIGRP_AUTH_SHA256_TLV,
		      sizeof(struct TLV_SHA256_Authentication_Type));

	return new;
}

void eigrp_authTLV_SHA256_free(struct TLV_SHA256_Authentication_Type *authTLV)
{
	XFREE(MTYPE_EIGRP_AUTH_SHA256_TLV, authTLV);
}

