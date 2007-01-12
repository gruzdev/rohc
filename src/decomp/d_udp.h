/**
 * @file d_udp.h
 * @brief ROHC decompression context for the UDP profile.
 * @author Didier Barvaux <didier.barvaux@b2i-toulouse.com>
 * @author The hackers from ROHC for Linux
 */

#ifndef D_UDP_H
#define D_UDP_H

#include <netinet/ip.h>
#include <netinet/udp.h>
#include <string.h>

#include "d_generic.h"


/**
 * @brief Define the UDP part of the decompression profile context.
 * 
 * This object must be used with the generic part of the decompression
 * context d_generic_context.
 *
 * @see d_generic_context
 */
struct d_udp_context
{
	/// Whether the UDP checksum field is encoded in the ROHC packet or not
	int udp_checksum_present;
};


/*
 * Public function prototypes.
 */

int udp_detect_ir_size(unsigned char *packet, int second_byte);

int udp_detect_ir_dyn_size(unsigned char *first_byte,
                           struct d_context *context);

int udp_decode_static_udp(struct d_generic_context *context,
                          const unsigned char *packet,
                          unsigned char *dest);

int udp_decode_dynamic_udp(struct d_generic_context *context,
                           const unsigned char *packet,
                           int payload_size,
                           unsigned char *dest);

void udp_build_uncompressed_udp(struct d_generic_context *context,
                                struct d_generic_changes *active,
                                unsigned char *dest,
                                int payload_size);


#endif

