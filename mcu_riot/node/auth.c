#include "auth.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <hashes/sha256.h>

#define AUTH_MASTER         1
#define AUTH_SLAVE          2
#define AUTH_HANDSHAKE_CPLT 4
#define AUTH_HANDSHAKE_PEND 8


/*
 * All authenticated connections have two sides. One side is the master, the other is called slave.
 * The master can send messages to the slave that change the slaves state (config values or status updates).
 * The slave can only reply with a ACK.
 *
 * Auth message footers / message types
 * HS1:  The first message sent by the auth module form the master.
 *       This contains a challenge for the slave.
 *
 *       USERDATA
 *       CHALLENGE
 *
 * HS2:  This is sent from the slave as a reply to the HS1.
 *       It is a data message containing the initial challenge and the slaves curent nonce.
 *
 *       USERDATA
 *       CHALLENGE
 *       NONCE
 *       AUTH_TAG
 *
 * Data: A data message contains some user data (or a HS2).
 *       The data is followed by a 64 bit tag consisting of the last 4 bits of the nonce and a 60 bit hash over
 *       The data and a secret key (HMAC)
 *
 *       USERDATA
 *       AUTH_TAG
 */

/*
 * This is used as representation for 64 bit numbers in the auth process.
 * This comes handy because normal numbers must be aligned.
 */
typedef struct
{
	uint8_t bytes[sizeof(uint64_t)];
} auth_number_t;

static inline uint8_t is_master(auth_context_t *ctx)
{
	return ctx->status & AUTH_MASTER;
}

static inline uint8_t handshake_cplt(auth_context_t *ctx)
{
	return ctx->status & AUTH_HANDSHAKE_CPLT;
}

static inline uint8_t handshake_pend(auth_context_t *ctx)
{
	return ctx->status & AUTH_HANDSHAKE_PEND;
}


/*
 * Compares memory in constant time.
 */
static uint8_t timesafe_memeq(const void *a, const void *b, uint32_t length)
{
	uint8_t r = 0;
	uint8_t *ua = (uint8_t *)a;
	uint8_t *ub = (uint8_t *)b;
	while (length--)
	{
		r |= (*(ua++)) ^ (*(ub++));
	}

	return (r == 0);
}


static auth_number_t generate_tag(const auth_context_t *ctx, uint64_t nonce, const void *data1, uint32_t len1, const void *data2, uint32_t len2)
{
	hmac_context_t hash_ctx;
	uint8_t digest[SHA256_DIGEST_LENGTH];

	hmac_sha256_init(&hash_ctx, ctx->key, AUTH_KEY_LEN);

	if (len1 != 0)
	{
		hmac_sha256_update(&hash_ctx, data1, len1);
	}

	if (len2 != 0)
	{
		hmac_sha256_update(&hash_ctx, data2, len2);
	}
	hmac_sha256_update(&hash_ctx, &nonce, sizeof(nonce));
	hmac_sha256_final(&hash_ctx, digest);

	return (*(auth_number_t *)digest);
}


/*
 * Signs message with given nonce
 *
 * The message footer of a normal authenticated message contains a 60 bit tag
 * and the last 4 bit of the current nonce.
 */
int sign_message(const auth_context_t *ctx, uint64_t nonce, void *data, uint32_t len, uint32_t *result_len, const void *add_data, uint32_t add_datalen)
{
	if ((*result_len) < len + sizeof(auth_number_t))
	{
		return -ENOMEM;
	}

	// make tag with given nonce

	auth_number_t tag = generate_tag(ctx, nonce, data, len, add_data, add_datalen);

	// Set the first 4 bits of the tag to the current nonce
	tag.bytes[0] = (tag.bytes[0] & 0x0F) | ((nonce & 0x0F) << 4);

	memcpy((uint8_t *)data + len, &tag, sizeof(tag));

	(*result_len) = len + sizeof(auth_number_t);

	return 0;
}


/*
 * Checks if message is signed with given nonce
 */
int check_message_tag(const auth_context_t *ctx, uint64_t nonce, const void *data, uint32_t *len, const void *add_data, uint32_t add_datalen)
{
	if (*len < sizeof(auth_number_t))
	{
		return AUTH_WRONG_SIZE;
	}

	uint32_t data_len = (*len) - sizeof(auth_number_t);

	auth_number_t *footer = (auth_number_t *)(((uint8_t *)data) + data_len);

	// Compare nonce first before generating the tag
	if ((footer->bytes[0] & 0xF0) != ((nonce & 0x0F) << 4))
	{
		return AUTH_WRONG_NONCE;
	}

	// nonce OK -> check tag
	auth_number_t tag = generate_tag(ctx, nonce, data, data_len, add_data, add_datalen);

	auth_number_t message_tag;
	memcpy(&message_tag, footer, sizeof(message_tag));

	// set the first 4 bits to 0
	tag.bytes[0] &= 0x0F;
	message_tag.bytes[0] &= 0x0F;

	if (!timesafe_memeq(&tag, &message_tag, sizeof(tag)))
	{
		return AUTH_WRONG_MAC;
	}

	// tag ok -> packet is valid
	(*len) = data_len;

	return 0;
}


void auth_master_init(auth_context_t *ctx, const uint8_t *key, uint64_t challenge)
{
	memcpy(ctx->key, key, sizeof(ctx->key));

	printf("Init auth in master mode with k=%x and challenge=%08lx%08lx\n", key[0], (uint32_t)(challenge >> 32), (uint32_t)challenge);

	ctx->nonce = challenge;
	ctx->status = AUTH_MASTER;
}


int auth_master_make_handshake(auth_context_t *ctx, void *data, uint32_t offset, uint32_t *result_len)
{
	if (!is_master(ctx))
	{
		return AUTH_WRONG_STATE;
	}

	ctx->status |= AUTH_HANDSHAKE_PEND;
	ctx->status &=~ AUTH_HANDSHAKE_CPLT;

	if (*result_len < sizeof(ctx->nonce) + offset)
	{
		return -ENOMEM;
	}

	memcpy(((uint8_t *)data) + offset, &(ctx->nonce), sizeof(ctx->nonce));

	(*result_len) = sizeof(ctx->nonce) + offset;

	return 0;
}


int auth_master_process_handshake(auth_context_t *ctx, const void *data, uint32_t offset, uint32_t len)
{
	if (!is_master(ctx) || handshake_cplt(ctx) || !handshake_pend(ctx))
	{
		return AUTH_WRONG_STATE;
	}

	// message is normally signed, the contents must be the challenge + nonce + offset data

	if (len != sizeof(auth_number_t) * 3 + offset)
	{
		return AUTH_WRONG_SIZE;
	}

	auth_number_t *hs2_values = (auth_number_t *)(((uint8_t *)data) + offset);
	auth_number_t *hs2_challenge = hs2_values + 0;


	if (memcmp(hs2_challenge, &ctx->nonce, sizeof(ctx->nonce)) != 0)
	{
		// wrong challenge
		return AUTH_WRONG_NONCE;
	}

	uint64_t hs2_nonce;
	memcpy(&hs2_nonce, hs2_values + 1, sizeof(hs2_nonce));

	auth_number_t tag = generate_tag(ctx, hs2_nonce, data, offset + sizeof(*hs2_values) * 2, NULL, 0);
	auth_number_t hs2_tag;
	memcpy(&hs2_tag, hs2_values + 2, sizeof(hs2_tag));

	// Set the first 4 bits to 0
	hs2_tag.bytes[0] &= 0x0f;
	tag.bytes[0] &= 0x0f;

	if (!timesafe_memeq(&tag, &hs2_tag, sizeof(tag)))
	{
		return AUTH_WRONG_MAC;
	}

	// store nonce
	ctx->nonce = hs2_nonce;
	ctx->status |= AUTH_HANDSHAKE_CPLT;
	ctx->status &= ~AUTH_HANDSHAKE_PEND;

	// increment the nonce for the next sent packet
	ctx->nonce += 2;

	return 0;
}


int auth_master_sign(auth_context_t *ctx, void *data, uint32_t len, uint32_t *result_len, const void *add_data, uint32_t add_datalen)
{
	if (!is_master(ctx) || !handshake_cplt(ctx))
	{
		return AUTH_WRONG_STATE;
	}

	// make tag with current nonce
	return sign_message(ctx, ctx->nonce, data, len, result_len, add_data, add_datalen);
}


int auth_master_check_ack(auth_context_t *ctx, const void *data, uint32_t offset, uint32_t len)
{
	if (!is_master(ctx) || !handshake_cplt(ctx))
	{
		return AUTH_WRONG_STATE;
	}

	if (len != sizeof(auth_number_t) + offset)
	{
		// ack message is only the footer
		return AUTH_WRONG_SIZE;
	}


	uint64_t ack_expected_nonce = ctx->nonce + 1;

	uint32_t msg_len = len;

	int res = check_message_tag(ctx, ack_expected_nonce, data, &msg_len, NULL, 0);
	if (res != 0)
	{
		return res;
	}

	// advance nonce by 2
	ctx->nonce += 2;
	return 0;
}


void auth_slave_init(auth_context_t *ctx, const uint8_t *key, uint64_t nonce)
{
	memcpy(ctx->key, key, sizeof(ctx->key));
	printf("Init auth in slave mode with k=%x and nonce=%08lx%08lx\n", key[0], (uint32_t)(nonce >> 32), (uint32_t)nonce);
	ctx->nonce = nonce;
	ctx->status = AUTH_SLAVE;

}


int auth_slave_handshake(auth_context_t *ctx, const void *inmsg, uint32_t inofs, uint32_t inlen, void *outmsg, uint32_t outofs, uint32_t *outlen)
{
	if (is_master(ctx))
	{
		return AUTH_WRONG_STATE;
	}

	// Need three numbers in response
	if (*outlen < sizeof(auth_number_t) * 3 + outofs)
	{
		return -ENOMEM;
	}

	if (inlen != sizeof(auth_number_t) + inofs)
	{
		return AUTH_WRONG_SIZE;
	}

	printf("Make hs2 with nonce=%08lx%08lx\n", (uint32_t)(ctx->nonce >> 32), (uint32_t)ctx->nonce);

	auth_number_t *hs2_values = (auth_number_t *)(((uint8_t *)outmsg) + outofs);

	// reply with my nonce and the challenge as data
	// Copy the challenge from the hs1 to the hs2
	memcpy(hs2_values, ((uint8_t*)inmsg) + inofs, sizeof(auth_number_t));

	// Write current nonce to hs2
	memcpy(hs2_values + 1, &ctx->nonce, sizeof(*hs2_values));

	// And normally sign the message using the current nonce
	int res = sign_message(ctx, ctx->nonce, outmsg, outofs + sizeof(*hs2_values) * 2, outlen, NULL, 0);

	if (res != 0)
	{
		return res;
	}


	ctx->status |= AUTH_HANDSHAKE_PEND;

	return 0;
}


int auth_slave_verify(auth_context_t *ctx, const void *data, uint32_t *len, const void *add_data, uint32_t add_datalen)
{
	if (handshake_pend(ctx))
	{
		ctx->status |= AUTH_HANDSHAKE_CPLT;
		ctx->status &= ~AUTH_HANDSHAKE_PEND;
	}

	if (is_master(ctx) || !handshake_cplt(ctx))
	{
		return AUTH_WRONG_STATE;
	}

	/*
	 * We expect a new message to have nonce + 2 as nonce
	 */
	uint64_t nonce_expected = ctx->nonce + 2;

	int res = check_message_tag(ctx, nonce_expected, data, len, add_data, add_datalen);

	if (res == AUTH_WRONG_NONCE)
	{
		// If an ack was lost, i may receive the last nonce again, this is reported as a special error.

		auth_number_t *tag = (auth_number_t *)(((uint8_t *)data) + (*len) - sizeof(auth_number_t));

		// Check if the 4-bit nonce code matches the old nonce
		if (((tag->bytes[0] & 0xF0) >> 4) == (ctx->nonce & 0x0F))
		{
			return AUTH_OLD_NONCE;
		}

		return res;
	}
	else if (res != 0)
	{
		return res;
	}

	// tag ok -> packet is valid
	// => return ok and increment nonce

	ctx->nonce += 2;
	return 0;
}


int auth_slave_make_ack(auth_context_t *ctx, void *data, uint32_t offset, uint32_t *len)
{
	if (is_master(ctx) || !handshake_cplt(ctx))
	{
		return AUTH_WRONG_STATE;
	}

	// make an empty packet with nonce + 1
	uint64_t ack_num = ctx->nonce + 1;

	return sign_message(ctx, ack_num, data, offset, len, NULL, 0);

	return 0;
}
