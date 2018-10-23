/*
 * Copyright (C) 2018 Daniel Frejek <daniel.frejek@stusta.net>
 *
 *
 * libusbhost is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdint.h>

typedef struct _scsi_command_inquiry
{
	uint8_t opcode;
	uint8_t flags;
	uint8_t pagecode;
	uint8_t length_msb;
	uint8_t length_lsb;
	uint8_t control;
} __attribute__((packed)) scsi_command_inquiry_t;


typedef struct _scsi_command_inquiry_response
{
	uint8_t periph_info;
	uint8_t flags_0;
	uint8_t version;
	uint8_t flags_1;
	uint8_t additional_length;
	uint8_t flags_2;
	uint8_t flags_3;
	uint8_t flags_4;
	uint8_t vender_indentification[8];
	uint8_t product_identification[16];
	uint8_t product_revision[4];
	uint8_t serial_number[8];
	uint8_t vendor_unique[12];
	uint8_t flags_5;
	// ...
} __attribute__((packed)) scsi_command_inquiry_response_t;

#define SCSI_COMMAND_INQUIRY_OPCODE 0x12

typedef struct _scsi_command_read_capacity16
{
	uint8_t opcode;
	uint8_t action;
	uint8_t lba[8];
	uint8_t allocation_length[4];
	uint8_t pmi;
	uint8_t control;
} __attribute__((packed)) scsi_command_read_capacity16_t;


typedef struct _scsi_command_read_capacity16_response
{
	uint8_t lba[8];
	uint8_t blocksize[4];
	uint8_t protection;
	uint8_t prot_lbppb;
	uint8_t lowest_aligned_block_and_flags[2];
	uint8_t reserverd[16];
} __attribute__((packed)) scsi_command_read_capacity16_response_t;


typedef struct _scsi_command_read_capacity10
{
	uint8_t opcode;
	uint8_t reserverd1;
	uint8_t lba[4];
	uint8_t reserverd2[2];
	uint8_t pmi;
	uint8_t control;
} __attribute__((packed)) scsi_command_read_capacity10_t;


typedef struct _scsi_command_read_capacity10_response
{
	uint8_t lba[4];
	uint8_t blocksize[4];
} __attribute__((packed)) scsi_command_read_capacity10_response_t;


_Static_assert(sizeof(scsi_command_read_capacity10_response_t) == 8,
			   "scsi read capacity (10) response must be exactly 8 bytes long");

#define SCSI_COMMAND_READ_CAPACITY16_OPCODE 0x9e
#define SCSI_COMMAND_READ_CAPACITY10_OPCODE 0x25
#define SCSI_COMMAND_READ_CAPACITY16_ACTION 0x10


typedef struct _scsi_command_read_6
{
	uint8_t opcode;
	uint8_t lba[3];
	uint8_t length;
	uint8_t control;
} __attribute__((packed)) scsi_command_read_6_t;


typedef struct _scsi_command_read_10
{
	uint8_t opcode;
	uint8_t flags;
	uint8_t lba[4];
	uint8_t group_num;
	uint8_t length[2];
	uint8_t control;
} __attribute__((packed)) scsi_command_read_10_t;


typedef struct _scsi_command_read_16
{
	uint8_t opcode;
	uint8_t flags;
	uint8_t lba[8];
	uint8_t length[4];
	uint8_t group_num;
	uint8_t control;
} __attribute__((packed)) scsi_command_read_16_t;

#define SCSI_COMMAND_READ6_OPCODE 0x08
#define SCSI_COMMAND_READ10_OPCODE 0x28
#define SCSI_COMMAND_READ16_OPCODE 0x88


typedef struct _scsi_command_write_6
{
	uint8_t opcode;
	uint8_t lba[3];
	uint8_t length;
	uint8_t control;
} __attribute__((packed)) scsi_command_write_6_t;


typedef struct _scsi_command_write_10
{
	uint8_t opcode;
	uint8_t flags;
	uint8_t lba[4];
	uint8_t group_num;
	uint8_t length[2];
	uint8_t control;
} __attribute__((packed)) scsi_command_write_10_t;


typedef struct _scsi_command_write_16
{
	uint8_t opcode;
	uint8_t flags;
	uint8_t lba[8];
	uint8_t length[4];
	uint8_t group_num;
	uint8_t control;
} __attribute__((packed)) scsi_command_write_16_t;

#define SCSI_COMMAND_WRITE6_OPCODE 0x0A
#define SCSI_COMMAND_WRITE10_OPCODE 0x2A
#define SCSI_COMMAND_WRITE16_OPCODE 0x8A

typedef struct _scsi_command_test_unit_ready
{
	uint8_t opcode;
	uint8_t reserved[4];
	uint8_t control;
} __attribute__((packed)) scsi_command_test_unit_ready_t;

#define SCSI_COMMAND_TEST_UNIT_READY_OPCODE 0x00

typedef struct _scsi_command_request_sense
{
	uint8_t opcode;
	uint8_t flags;
	uint8_t reserved[2];
	uint8_t length;
	uint8_t control;
} __attribute__((packed)) scsi_command_request_sense_t;

#define SCSI_COMMAND_REQUEST_SENSE_OPCODE 0x03
