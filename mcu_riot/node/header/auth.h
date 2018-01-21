/*
 * Authentication Module
 * Used to authenticate messsages and check authenticity
 *
 * WARNING:
 * This is SELF IMPLEMENTED CRYPTO and therefore UNSAFE by definition.
 * DO NOT USE THIS FOR ANYTHING SECURITY OR PRIVARY RELEVANT!!!!
 *
 */

#pragma once

#include <stdint.h>

// 128 bit key
#define AUTH_KEY_LEN 16

// Wrong state to call this function
#define AUTH_WRONG_STATE     1

// Nonce is wrong
#define AUTH_WRONG_NONCE     2

// MAC is wrong
#define AUTH_WRONG_MAC       3

// Message has wrong size
#define AUTH_WRONG_SIZE      4

// Received message has old nonce (that of the previous packet)
// Should be re-acked
// The message must not be processed again!
#define AUTH_OLD_NONCE       5

typedef struct
{
	uint8_t key[AUTH_KEY_LEN];
	uint64_t nonce;
	uint8_t status;
} auth_context_t;

/*
 * Initializes the auth context in master (data sending) mode
 */
void auth_master_init(auth_context_t *ctx, const uint8_t *key, uint64_t challenge);

/*
 * Generates the initial auth message sent to the slave
 * The message content is written into <data>, at position <offset>
 * Any data before <offset> in the buffer is kept.
 * On call <result_len> specifies the length of the <data> buffer. After return <result_len> is the length of the message.
 * returns nonzero on error
 */
int auth_master_make_handshake(auth_context_t *ctx, void *data, uint32_t offset, uint32_t *result_len);

/*
 * Processes the auth handshake reply from the slave
 * <data> is the received handshake message form the slave, len the message length
 * <offset> is the offset of the "real" handshake message. (Which starts after the packet haeder)
 * (must be the same as offset parameter in auth_slave_handshake)
 * returns nonzero on error
 */
int auth_master_process_handshake(auth_context_t *ctx, const void *data, uint32_t offset, uint32_t len);

/*
 * Signs a message, only the master can sign
 * <data> is the message to sign, <len> is the message length.
 * The result is appended to the message, <result_len> is initially the size of the data buffer, after return <result_data>
 * is the size of the signed message.
 * add_data is additional data that is included in the tag.
 * returns nonzero on error
 */
int auth_master_sign(auth_context_t *ctx, void *data, uint32_t len, uint32_t *result_len, const void *add_data, uint32_t add_datalen);

/*
 * Checks the ack packet and increments the nonce on success
 * data is the received ack packet, len the length, offset the start of the message data
 */
int auth_master_check_ack(auth_context_t *ctx, const void *data, uint32_t offset, uint32_t len);

/*
 * Initializes the auth context in slave (data receiving) mode.
 */
void auth_slave_init(auth_context_t *ctx, const uint8_t *key, uint64_t nonce);

/*
 * Processes the handshake message from the master and creates the handshake reply message.
 * inmsg is the handshake from the master (inlen its length)
 * outmsg is the reply message, on call outlen is the size of the outmsg buffer, after return it is the length of the
 * In / out ofs is the length of the packet header. This header is authenticated, the handshake data starts right behind this header.
 * reply message.
 */
int auth_slave_handshake(auth_context_t *ctx, const void *inmsg, uint32_t inofs, uint32_t inlen, void *outmsg, uint32_t outofs, uint32_t *outlen);

/*
 * Check the signature in the message.
 * data is the message, on call len is the message length (including auth tag), after return this is the
 * message length without the tag.
 * add_data is additonal data that is included in the auth tag.
 * The caller must check if the return value is AUTH_OLD_NONCE in this case the received packet was the last and the ACK should be sent again.
 */
int auth_slave_verify(auth_context_t *ctx, const void *data, uint32_t *len, const void *add_data, uint32_t add_datalen);

/*
 * Creates an ACK message for the current nonce
 * data is the target buffer, on call rsult is the buffer length, on return result_len is the length of the ack packet
 * offset is the length of the packet header
 */
int auth_slave_make_ack(auth_context_t *ctx, void *data, uint32_t offset, uint32_t *result_len);
