#include "auth.h"
#include <stdio.h>
#include <string.h>

void test_auth()
{
	printf("Auth module test\n");

	auth_context_t ctxa;
	auth_context_t ctxb;

	uint8_t key[32] = "1234567890ABCDEF";

	auth_master_init(&ctxa, key, 100);
	auth_slave_init(&ctxb, key, 1000);

	printf("INIT DONE -> BEGIN HANDSHAKE\n");

	uint8_t ba[32];
	uint8_t bb[32];

	uint32_t len1 = sizeof(ba);
	ba[0] = 'x';
	int a = auth_master_make_handshake(&ctxa, ba, 1, &len1);
	printf("auth_master_make_handshake: %i, len=%u\n", a, len1);


	bb[0] = 'y';
	uint32_t len2 = sizeof(bb);
	int b = auth_slave_handshake(&ctxb, ba, 1, len1, bb, 1, &len2);
	printf("auth_slave_handshake: %i, len=%u\n", b, len2);
	
	a = auth_master_process_handshake(&ctxa, bb, 1, len2);
	printf("auth_master_process_handshake: %i\n", a);


	memcpy(ba, "TEST", 5);

	len1 = sizeof(ba);
	a = auth_master_sign(&ctxa, ba, 5, &len1, "A", 1);
	printf("auth_master_sign: %i, len=%u\n", a, len1);

	len2 = len1;
	b = auth_slave_verify(&ctxb, ba, &len1, "A", 1);
	printf("auth_slave_verify: %i, len=%u, str=%s\n", b, len1, ba);

	b = auth_slave_verify(&ctxb, ba, &len2, NULL, 0);
	printf("auth_slave_verify: %i, len=%u\n", b, len1);

	len1 = sizeof(ba);
	b = auth_slave_make_ack(&ctxb, ba, 0, &len1);
	printf("auth_slave_make_ack: %i, len=%u\n", b, len1);

	a = auth_master_check_ack(&ctxa, ba, 0, len1);
	printf("auth_master_check_ack: %i\n", a);
	a = auth_master_check_ack(&ctxa, ba, 0, len1);
	printf("auth_master_check_ack: %i\n", a);

	memcpy(ba, "TEST123", 8);

	len1 = sizeof(ba);
	a = auth_master_sign(&ctxa, ba, 8, &len1, "AA", 2);
	printf("auth_master_sign: %i, len=%u\n", a, len1);


	b = auth_slave_verify(&ctxb, ba, &len1, "AA", 2);
	printf("auth_slave_verify: %i, len=%u, str=%s\n", b, len1, ba);

	len1 = sizeof(ba);
	b = auth_slave_make_ack(&ctxb, ba, 0, &len1);
	printf("auth_slave_make_ack: %i, len=%u\n", b, len1);

	a = auth_master_check_ack(&ctxa, ba, 0, len1);
	printf("auth_master_check_ack: %i\n", a);

}
