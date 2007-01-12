/**
 * @file c_generic.c
 * @brief ROHC generic compression context for IP-only, UDP and UDP Lite
 *        profiles.
 * @author Didier Barvaux <didier.barvaux@b2i-toulouse.com>
 * @author The hackers from ROHC for Linux
 */

#include "c_generic.h"


/**
 * @brief The description of the different ROHC packets.
 */
const char *generic_packet_types[] =
{
	"IR",
	"IRDYN",
	"OU-0",
	"OU-1",
	"OU-2",
};


/**
 * @brief The description of the different extensions for the UO-2 packet.
 */
const char *generic_extension_types[] =
{
	"NOEXT",
	"EXT0",
	"EXT1",
	"EXT2",
	"EXT3",
};


/*
 * Function prototypes.
 */

int code_packet(struct c_context *context,
                const struct iphdr *ip,
                const struct iphdr *ip2,
                const unsigned char *next_header,
                unsigned char *dest);

int code_IR_packet(struct c_context *context,
                   const struct iphdr *ip,
                   const struct iphdr *ip2,
                   const unsigned char *next_header,
                   unsigned char *dest);

int code_IR_DYN_packet(struct c_context *context,
                       const struct iphdr *ip,
                       const struct iphdr *ip2,
                       const unsigned char *next_header,
                       unsigned char *dest);

int code_generic_static_part(struct c_context *context,
                     struct ip_header_info *header_info,
                     const struct iphdr *ip,
                     unsigned char *dest,
                     int counter);

int code_generic_dynamic_part(struct c_context *context,
                      struct ip_header_info *header_info,
                      const struct iphdr *ip,
                      unsigned char *dest,
                      int counter);

int code_UO_packet_tail(struct c_context *context,
                        const struct iphdr *ip,
                        const struct iphdr *ip2,
                        const unsigned char *next_header,
                        unsigned char *dest,
								int counter);

int code_UO0_packet(struct c_context *context,
                    const struct iphdr *ip,
                    const struct iphdr *ip2,
                    const unsigned char *next_header,
                    unsigned char *dest);

int code_UO1_packet(struct c_context *context,
                    const struct iphdr *ip,
                    const struct iphdr *ip2,
                    const unsigned char *next_header,
                    unsigned char *dest);

int code_UO2_packet(struct c_context *context,
                    const struct iphdr *ip,
                    const struct iphdr *ip2,
                    const unsigned char *next_header,
                    unsigned char *dest);

int code_EXT0_packet(struct c_context *context,
                     unsigned char *dest,
                     int counter);

int code_EXT1_packet(struct c_context *context,
                     unsigned char *dest,
                     int counter);

int code_EXT2_packet(struct c_context *context,
                     unsigned char *dest,
                     int counter);

int code_EXT3_packet(struct c_context *context,
                     const struct iphdr *ip,
                     const struct iphdr *ip2,
                     unsigned char *dest,
                     int counter);

void decide_state(struct c_context *context);

int decide_packet(struct c_context *context);

void update_variables(struct c_context *context,
                      const struct iphdr *ip,
                      const struct iphdr *ip2);

int decide_extension(struct c_context *context);

int header_flags(struct c_context *context,
                 struct ip_header_info *header_info,
                 unsigned short changed_f,
                 const struct iphdr *ip,
                 boolean is_outer,
                 int nr_ip_id_bits,
                 unsigned char *dest,
                 int counter);

int header_fields(struct c_context *context,
                  struct ip_header_info *header_info,
                  unsigned short changed_f,
                  const struct iphdr *ip,
                  boolean is_outer,
                  int nr_ip_id_bits,
                  unsigned char *dest,
                  int counter);

int changed_static_both_hdr(struct c_context *context,
                            const struct iphdr *ip,
                            const struct iphdr *ip2);

int changed_static_one_hdr(unsigned short changed_fields,
                           struct ip_header_info *header_info,
                           const struct iphdr *ip,
                           struct c_context *context);

int changed_dynamic_both_hdr(struct c_context *context,
                             const struct iphdr *ip,
                             const struct iphdr *ip2);

int changed_dynamic_one_hdr(unsigned short changed_fields,
                            struct ip_header_info *header_info,
                            const struct iphdr *ip,
                            struct c_context *context);

unsigned short changed_fields(struct ip_header_info *header_info,
                              const struct iphdr *ip);

void check_ip_identification(struct ip_header_info *header_info,
                             const struct iphdr *ip);


/**
 * @brief Initialize the inner or outer IP header info stored in the context.
 *
 * @param header_info The inner or outer IP header info to initialize
 * @param ip          The inner or outer IP header
 * @return            1 if successful, 0 otherwise
 */
int c_init_header_info(struct ip_header_info *header_info,
                       const struct iphdr *ip)
{
	/* init the parameters to encode the IP-ID with W-LSB encoding */
	header_info->ip_id_window = c_create_wlsb(16, C_WINDOW_WIDTH, 0);
	if(header_info->ip_id_window == NULL)
	{
		rohc_debugf(0, "no memory to allocate W-LSB encoding for IP-ID\n");
		goto error;
	}
	
	/* store the IP packet and the random and NBO parameters
	 * in the header info */
	header_info->old_ip = *ip;
	header_info->rnd = 0;
	header_info->old_rnd = header_info->old_rnd;
	header_info->nbo = 1;
	header_info->old_nbo = header_info->nbo;

	/* init the thresholds the counters must reach before launching
	 * an action */
	header_info->tos_count = MAX_FO_COUNT;
	header_info->ttl_count = MAX_FO_COUNT;
	header_info->df_count = MAX_FO_COUNT;
	header_info->protocol_count = MAX_FO_COUNT;
	header_info->rnd_count = MAX_FO_COUNT;
	header_info->nbo_count = MAX_FO_COUNT;

	return 1;

error:
	return 0;
}


/**
 * @brief Initialize all temporary variables stored in the context.
 *
 * @param tmp_variables The temporary variables to initialize
 */
void c_init_tmp_variables(struct generic_tmp_variables *tmp_variables)
{
	tmp_variables->nr_of_ip_hdr = -1;
	tmp_variables->changed_fields = -1;
	tmp_variables->changed_fields2 = -1;
	tmp_variables->send_static = -1;
	tmp_variables->send_dynamic = -1;
	tmp_variables->nr_ip_id_bits = -1;
	tmp_variables->nr_sn_bits = -1;
	tmp_variables->nr_ip_id_bits2 = -1;
	tmp_variables->packet_type = -1;
	tmp_variables->max_size = -1;
}


/**
 * @brief Create a new context and initialize it thanks to the given IP packet.
 *
 * This function is one of the functions that must exist in one profile for the
 * framework to work.
 *
 * @param context The compression context
 * @param ip      The IP packet given to initialize the new context
 * @return        1 if successful, 0 otherwise
 */
int c_generic_create(struct c_context *context, const struct iphdr *ip)
{
	struct c_generic_context *g_context;

	if(ip->version != 4)
	{
		rohc_debugf(0, "wrong IP version (%d)\n", ip->version);
		goto quit;
	}

	if(ip->ihl * 4 != 20)
	{
		rohc_debugf(0, "wrong IP header size (%d)\n", ip->ihl);
		goto quit;
	}

	if((ntohs(ip->frag_off) & (~IP_DF)) != 0)
	{
		rohc_debugf(0, "fragment error in outer IP header (0x%04x)\n",
		            ntohs(ip->frag_off));
		return -1;
	}

	if(ip->protocol == IPPROTO_IPIP)
	{
		struct iphdr *ip2 = (struct iphdr *) (ip + 1);

		if(ip2->version != 4)
		{
			rohc_debugf(0, "wrong inner IP version (%d)\n", ip2->version);
			goto quit;
		}
		
		if(ip2->ihl * 4 != 20)
		{
			rohc_debugf(0, "wrong inner IP header size (%d)\n", ip2->ihl);
			goto quit;
		}

		if((ntohs(ip2->frag_off) & (~IP_DF)) != 0)
		{
			rohc_debugf(0, "fragment error in inner IP header (0x%04x)\n",
			            ntohs(ip2->frag_off));
			goto quit;
		}
	}

	/* allocate memory for the generic part of the context */
	g_context =
		(struct c_generic_context *) malloc(sizeof(struct c_generic_context));
	if(g_context == NULL)
	{
	  rohc_debugf(0, "no memory for generic part of the profile context\n");
	  goto quit;
	}
	bzero(g_context, sizeof(struct c_generic_context));
	context->specific = g_context;

	/* initialize some context variables:
	 *  1. init the Sequence Number (SN) to 0
	 *  2. init the parameters to encode the SN with W-LSB encoding
	 *  3. init the counters of packet types
	 *  4. init the counters for the periodic transition to lower states
	 *  5. init the info related to the outer IP header, the info related to the
	 *     inner IP header will be initialized later if necessary
	 *  6. init the temporary variables
	 *  7. init the profile-specific variables to safe values
	 */

	/* step 1 */
	g_context->sn = 0;

	/* step 2 */
	g_context->sn_window = c_create_wlsb(16, C_WINDOW_WIDTH, -1);
	if(g_context->sn_window == NULL)
	{
		rohc_debugf(0, "no memory to allocate W-LSB encoding for SN\n");
		goto clean;
	}

	/* step 3 */
	g_context->ir_count = 0;
	g_context->fo_count = 0;
	g_context->so_count = 0;

	/* step 4 */
	g_context->go_back_fo_count = 0;
	g_context->go_back_ir_count = 0;
	g_context->ir_dyn_count = 0;

	/* step 5 */
	if(!c_init_header_info(&g_context->ip_flags, ip))
		goto clean;
	g_context->is_ip2_initialized = 0;

	/* step 6 */
	c_init_tmp_variables(&g_context->tmp_variables);

	/* step 7 */
	g_context->specific = NULL;
	g_context->next_header_proto = 0;
	g_context->next_header_len = 0;
	g_context->decide_state = decide_state;
	g_context->init_at_IR = NULL;
	g_context->code_static_part = NULL;
	g_context->code_dynamic_part = NULL;
	g_context->code_UO_packet_head = NULL;
	g_context->code_UO_packet_tail = NULL;

	return 1;

clean:
	c_generic_destroy(context);
quit:
	return 0;
}


/**
 * @brief Destroy the context.
 * 
 * This function is one of the functions that must exist in one profile for the
 * framework to work.
 *
 * @param context The compression context
 */
void c_generic_destroy(struct c_context *context)
{
	struct c_generic_context *g_context =
		(struct c_generic_context *) context->specific;

	if(g_context != NULL)
	{
		if(g_context->ip_flags.ip_id_window != NULL)
			c_destroy_wlsb(g_context->ip_flags.ip_id_window);
		if(g_context->is_ip2_initialized && g_context->ip2_flags.ip_id_window != NULL)
			c_destroy_wlsb(g_context->ip2_flags.ip_id_window);
		if(g_context->sn_window != NULL)
			c_destroy_wlsb(g_context->sn_window);

		if(g_context->specific != NULL)
			zfree(g_context->specific);

		zfree(g_context);
	}
}


/**
 * @brief Change the mode of the context.
 *
 * @param context  The compression context
 * @param new_mode The new mode the context must enter in
 */
void change_mode(struct c_context *context, rohc_mode new_mode)
{
	if(context->mode != new_mode)
	{
		/* change mode and go back to IR state */
		rohc_debugf(1, "change from mode %d to mode %d\n",
		            context->mode, new_mode);
		context->mode = new_mode;
		change_state(context, IR);
	}
}


/**
 * @brief Change the state of the context.
 *
 * @param context   The compression context
 * @param new_state The new state the context must enter in
 */
void change_state(struct c_context *context, rohc_c_state new_state)
{
	struct c_generic_context *g_context;
	
	g_context = (struct c_generic_context *) context->specific;

	if(context->state != new_state)
	{
		rohc_debugf(1, "change from state %d to state %d\n",
		            context->state, new_state);

		/* reset counters */
		g_context->ir_count = 0;
		g_context->fo_count = 0;
		g_context->so_count = 0;

		/* change state */
		context->state = new_state;
	}
}


/**
 * @brief Encode an IP packet according to a pattern decided by several
 *        different factors.
 *
 * 1. Check if we have double IP headers.\n
 * 2. Check if the IP-ID fields are random and if they are in NBO.\n
 * 3. Decide in which state to go (IR, FO or SO).\n
 * 4. Decide how many bits are needed to send the IP-ID and SN fields and more
 *    important update the sliding windows.\n
 * 5. Decide which packet type to send.\n
 * 6. Code the packet.\n
 * \n
 * This function is one of the functions that must exist in one profile for the
 * framework to work.
 *
 * @param context        The compression context
 * @param ip             The IP packet to encode
 * @param packet_size    The length of the IP packet to encode
 * @param dest           The rohc-packet-under-build buffer
 * @param dest_size      The length of the rohc-packet-under-build buffer
 * @param payload_offset The offset for the payload in the IP packet
 * @return               The length of the created ROHC packet
 */
int c_generic_encode(struct c_context *context,
                     const struct iphdr *ip,
                     int packet_size,
                     unsigned char *dest,
                     int dest_size,
                     int *payload_offset)
{
	struct c_generic_context *g_context;
	struct iphdr *ip2, *last_ip_header;
	unsigned char *next_header;
	int size;
	
	g_context = (struct c_generic_context *) context->specific;
	if(g_context == NULL)
	{
	 	 rohc_debugf(0, "generic context not valid\n");
	 	 return 0;
	}

	g_context->tmp_variables.changed_fields2 = 0;
	g_context->tmp_variables.nr_ip_id_bits2 = 0;
	g_context->tmp_variables.packet_type = PACKET_IR;
	g_context->tmp_variables.max_size = dest_size;
	
	/* STEP 1:
	 *  - check double IP headers
	 *  - check the next header protocol if necessary
	 *  - compute the payload offset
	 *  - discard IP fragments
	 */
	if(ip->protocol == IPPROTO_IPIP)
	{
		/* there are 2 IP headers */
		ip2 = (struct iphdr *) (ip + 1);
		g_context->tmp_variables.nr_of_ip_hdr = 2;
		last_ip_header = ip2;

		if(!g_context->is_ip2_initialized)
		{
			if(!c_init_header_info(&g_context->ip2_flags, ip2))
				return -1;
			g_context->is_ip2_initialized = 1;
		}
	}
	else
	{
		/* there is only one IP header */
		ip2 = NULL;
		g_context->tmp_variables.nr_of_ip_hdr = 1;
		last_ip_header = (struct iphdr *) ip;
	}
	
	/* check the next header protocol if necessary */
	if(g_context->next_header_proto != 0 &&
	   last_ip_header->protocol != g_context->next_header_proto)
	{
		/* the IP protocol field does not match the attended
		 * next header protocol */
		return -1;
	}
	next_header = (unsigned char *) (last_ip_header + 1);

	/* find the offset of the payload */
	*payload_offset = g_context->tmp_variables.nr_of_ip_hdr * sizeof(struct iphdr)
	                  + g_context->next_header_len;

	/* discard IP fragments:
	 *  - the R (Reserved) and MF (More Fragments) bits must be zero
	 *  - the Fragment Offset field must be zero
	 *  => ip->frag_off must be zero except the DF (Don't Fragment) bit
	 */
	if((ntohs(ip->frag_off) & (~IP_DF)) != 0)
	{
		rohc_debugf(0, "fragment error in outer IP header (0x%04x)\n", ntohs(ip->frag_off));
		return -1;
	}

	if(ip2 != NULL && (ntohs(ip2->frag_off) & (~IP_DF)) != 0)
	{
		rohc_debugf(0, "fragment error in inner IP header (0x%04x)\n", ntohs(ip2->frag_off));
		return -1;
	}

	/* STEP 2:
	 *  - check NBO and RND of the IP-ID of the outer and inner IP headers
	 *  - increase the Sequence Number (SN)
	 *  - find how many static and dynamic IP fields changed
	 */
	if(g_context->sn != 0) /* skip first packet (sn == 0) */
	{
		check_ip_identification(&g_context->ip_flags, ip);
		if(g_context->tmp_variables.nr_of_ip_hdr > 1)
			check_ip_identification(&g_context->ip2_flags, ip2);
	}

	/* increase the sequence number every time we encode something */
	g_context->sn++;
	rohc_debugf(3, "SN = %d\n", g_context->sn);

	/* find IP fields that changed */
	g_context->tmp_variables.changed_fields = changed_fields(&g_context->ip_flags, ip);
	if(g_context->tmp_variables.nr_of_ip_hdr > 1)
		g_context->tmp_variables.changed_fields2 = changed_fields(&g_context->ip2_flags, ip2);

	/* how many changed fields are static ones? */
	g_context->tmp_variables.send_static = changed_static_both_hdr(context, ip, ip2);
	/* how many changed fields are dynamic ones? */
	g_context->tmp_variables.send_dynamic = changed_dynamic_both_hdr(context, ip, ip2);

	rohc_debugf(2, "send_static = %d, send_dynamic = %d\n",
	            g_context->tmp_variables.send_static,
	            g_context->tmp_variables.send_dynamic);

	/* STEP 3: decide in which state to go */
	if(g_context->decide_state != NULL)
		g_context->decide_state(context);
	rohc_debugf(2, "ip_id = %d context_sn = %d\n", ntohs(ip->id), g_context->sn);

	/* STEP 4:
	 *  - compute how many bits are needed to send the IP-ID and SN fields
	 *  - update the sliding windows
	 */
	update_variables(context, ip, ip2);

	/* STEP 5: decide which packet to send */
	g_context->tmp_variables.packet_type = decide_packet(context);

	/* STEP 6: code the packet (and the extension if needed) */
	size = code_packet(context, ip, ip2, next_header, dest);
	if(size < 0)
		return -1;

	/* update the context with the new headers */
	g_context->ip_flags.old_ip = *ip;
	g_context->ip_flags.old_rnd = g_context->ip_flags.rnd;
	g_context->ip_flags.old_nbo = g_context->ip_flags.nbo;

	if(g_context->tmp_variables.nr_of_ip_hdr > 1)
	{
		g_context->ip2_flags.old_ip = *ip2;
		g_context->ip2_flags.old_rnd = g_context->ip2_flags.rnd;
		g_context->ip2_flags.old_nbo = g_context->ip2_flags.nbo;
	}

	/* update packet counters */
	if(g_context->tmp_variables.packet_type == PACKET_IR)
		context->num_sent_ir++;
	else if (g_context->tmp_variables.packet_type == PACKET_IR_DYN)
		context->num_sent_ir_dyn++;

	/* return the length of the ROHC packet */
	return size;
}


/**
 * @brief Update the profile when feedback arrives.
 *
 * This function is one of the functions that must exist in one profile for the
 * framework to work.
 *
 * @param context  The compression context
 * @param feedback The feedback information including the whole feedback packet
 */
void c_generic_feedback(struct c_context *context,
                        struct c_feedback *feedback)
{
	struct c_generic_context *g_context;
	unsigned char *p; /* pointer to the profile-specific data
	                     in the feedback packet */
	unsigned int sn;
	
	g_context = (struct c_generic_context *) context->specific;
	p = feedback->data + feedback->specific_offset;

	switch(feedback->type)
	{
		case 1: /* FEEDBACK-1 */
			sn = p[0];
			
			c_ack_sn_wlsb(g_context->ip_flags.ip_id_window, sn);
			c_ack_sn_wlsb(g_context->sn_window, sn);
			break;

		case 2: /* FEEDBACK-2 */
		{
			unsigned int crc = 0, crc_used = 0;
			int sn_not_valid = 0;
			unsigned char mode = (p[0] >> 4) & 3;
			int remaining = feedback->specific_size - 2;
			int opt, optlen;

			sn = ((p[0] & 15) << 8) + p[1];
			p += 2;
			
			while(remaining > 0)
			{
				opt = p[0] >> 4;
				optlen = p[0] & 0x0f;
				
				switch(opt)
				{
					case 1: /* CRC */
						crc = p[1];
						crc_used = 1;
						p[1] = 0; /* set to zero for crc computation */
						break;
					case 3: /* SN-Not-Valid */
						sn_not_valid = 1;
						break;
					case 4: /* SN */
						/* TODO: how are several SN options combined? */
						sn = (sn << 8) + p[1];
						break;
					case 2: /* Reject */
					case 7: /* Loss */
					default:
						rohc_debugf(0, "unknown feedback type: %d\n", opt);
						break;
				}
				
				remaining -= 1 + optlen;
				p += 1 + optlen;
			}
			
			/* check CRC if used */
			if(crc_used && crc_calculate(CRC_TYPE_8, feedback->data, feedback->size) != crc)
			{
				rohc_debugf(0, "CRC check failed (size = %d)\n", feedback->size);
				return;
			}

			if(mode != 0)
			{
				if(crc_used)
					change_mode(context, mode);
				else
					rohc_debugf(0, "mode change requested but no crc was given\n");
			}
			
			switch(feedback->acktype)
			{
				case ACK:
					if(sn_not_valid == 0)
					{
						c_ack_sn_wlsb(g_context->ip_flags.ip_id_window, sn);
						c_ack_sn_wlsb(g_context->sn_window, sn);
					}
					break;
				
				case NACK:
					if(context->state == SO)
					{
						change_state(context, FO);
						g_context->ir_dyn_count = 0;
					}
					else if(context->state == FO)
						g_context->ir_dyn_count = 0;
					break;
					
				case STATIC_NACK:
					change_state(context, IR);
					break;
					
				case RESERVED:
					rohc_debugf(0, "reserved field used\n");
					break;

				default:
					/* impossible value */
					rohc_debugf(2, "unknown ack type\n");
			}	
		}
		break;

		default: /* not FEEDBACK-1 nor FEEDBACK-2 */
			rohc_debugf(0, "feedback type not implemented (%d)\n",
			            feedback->type);
	}
}


/**
 * @brief Periodically change the context state after a certain number
 *        of packets.
 *
 * @param context The compression context
 */
void periodic_down_transition(struct c_context *context)
{
	struct c_generic_context *g_context;
	
	g_context = (struct c_generic_context *) context->specific;

	if(g_context->go_back_fo_count >= CHANGE_TO_FO_COUNT)
	{
		g_context->go_back_fo_count = 0;
		g_context->ir_dyn_count = 0;
		change_state(context, FO);
	}
	else if(g_context->go_back_ir_count >= CHANGE_TO_IR_COUNT)
	{
		g_context->go_back_ir_count = 0;
		change_state(context, IR);
	}

	if(context->state == SO)
		g_context->go_back_fo_count++;
	if(context->state == SO || context->state == FO)
		g_context->go_back_ir_count++;
}


/**
 * @brief Decide the state that should be used for the next packet.
 *
 * The three states are:\n
 *  - Initialization and Refresh (IR),\n
 *  - First Order (FO),\n
 *  - Second Order (SO).
 *
 * @param context The compression context
 */
void decide_state(struct c_context *context)
{
	struct c_generic_context *g_context;
	int ir_count, fo_count, send_static, send_dynamic;
	rohc_c_state curr_state, next_state;
	
	curr_state = context->state;
	next_state = curr_state;

	g_context = (struct c_generic_context *) context->specific;
	ir_count = g_context->ir_count;
	fo_count = g_context->fo_count;
	send_static = g_context->tmp_variables.send_static;
	send_dynamic = g_context->tmp_variables.send_dynamic;

	if(curr_state == IR && send_dynamic && ir_count >= MAX_IR_COUNT)
		next_state = FO;
	else if(curr_state == IR && send_static && ir_count >= MAX_IR_COUNT)
		next_state = FO;
	else if(curr_state == IR && g_context->ir_count >= MAX_IR_COUNT)
		next_state = SO;
	else if(curr_state == FO && send_dynamic && fo_count >= MAX_FO_COUNT)
		next_state = FO;
	else if(curr_state == FO && send_static && fo_count >= MAX_FO_COUNT)
		next_state = FO;
	else if(curr_state == FO && fo_count >= MAX_FO_COUNT)
		next_state = SO;
	else if(curr_state == SO && send_dynamic)
		next_state = FO;
	else if(curr_state == SO && send_static)
		next_state = FO;

	change_state(context, next_state);
	
	if(context->mode == U_MODE)
		periodic_down_transition(context);
}


/**
 * @brief Update some context variables.
 *
 * This function is only used in encode. Everything in this function could
 * be in encode but to make it more readable we have it here instead.
 *
 * @param context The compression context
 * @param ip      The outer IP header
 * @param ip2     The inner IP header
 */
void update_variables(struct c_context *context,
                      const struct iphdr *ip,
                      const struct iphdr *ip2)
{
	struct c_generic_context *g_context;
	
	g_context = (struct c_generic_context *) context->specific;

	if(g_context->ip_flags.nbo)
		g_context->ip_flags.id_delta = ntohs(ip->id) - g_context->sn;
	else
		g_context->ip_flags.id_delta = ip->id - g_context->sn;

	if(g_context->tmp_variables.nr_of_ip_hdr > 1)
	{
		if(g_context->ip2_flags.nbo)
			g_context->ip2_flags.id_delta = ntohs(ip2->id) - g_context->sn;
		else
			g_context->ip2_flags.id_delta = ip2->id - g_context->sn;
	}

	rohc_debugf(3, "Get k\n");
	rohc_debugf(2, "id_delta: %d\n", g_context->ip_flags.id_delta);
	rohc_debugf(2, "Given that sn: %d\n", g_context->sn);
	g_context->tmp_variables.nr_ip_id_bits = c_get_k_wlsb(g_context->ip_flags.ip_id_window, g_context->ip_flags.id_delta);
	g_context->tmp_variables.nr_sn_bits = c_get_k_wlsb(g_context->sn_window, g_context->sn);
	rohc_debugf(2, "ip_id bits=%d\n", g_context->tmp_variables.nr_ip_id_bits);
	rohc_debugf(2, "sn bits=%d\n", g_context->tmp_variables.nr_sn_bits);

	rohc_debugf(3, "Adding\n");
	c_add_wlsb(g_context->ip_flags.ip_id_window, g_context->sn, 0, g_context->ip_flags.id_delta);
	c_add_wlsb(g_context->sn_window, g_context->sn, 0, g_context->sn);
	
	if(g_context->tmp_variables.nr_of_ip_hdr > 1)
	{
		g_context->tmp_variables.nr_ip_id_bits2 = c_get_k_wlsb(g_context->ip2_flags.ip_id_window, g_context->ip2_flags.id_delta);
		rohc_debugf(2, "ip_id bits2=%d\n", g_context->tmp_variables.nr_ip_id_bits2);
		c_add_wlsb(g_context->ip2_flags.ip_id_window, g_context->sn, 0, g_context->ip2_flags.id_delta);
	}
}


/**
 * @brief Decide which packet to send when in First Order (FO) state.
 *
 * Packets that can be used are the IR-DYN and UO-2 packets.
 *
 * @see decide_packet
 *
 * @param context The compression context
 * @return        The packet type among PACKET_IR_DYN and PACKET_UOR_2
 */
int decide_FO_packet(struct c_context *context)
{
	struct c_generic_context *g_context;
	int nr_of_ip_hdr, send_static, send_dynamic;
	int packet;

	g_context = (struct c_generic_context *) context->specific;
	nr_of_ip_hdr = g_context->tmp_variables.nr_of_ip_hdr;
	send_static = g_context->tmp_variables.send_static;
	send_dynamic = g_context->tmp_variables.send_dynamic;

	if(g_context->ir_dyn_count < MAX_FO_COUNT)
	{
		g_context->ir_dyn_count++;
		packet = PACKET_IR_DYN;
	}
	else if(send_static) /* if one of the static fields changed, go back to IR */
		packet = PACKET_UOR_2;
	else if(nr_of_ip_hdr == 1 && send_dynamic > 2)
		packet = PACKET_IR_DYN;
	else if(nr_of_ip_hdr > 1 && send_dynamic > 4)
		packet = PACKET_IR_DYN;
	else
		packet = PACKET_UOR_2;

	return packet;
}


/**
 * @brief Decide which packet to send when in Second Order (SO) state.
 *
 * Packets that can be used are the UO-0, UO-1 and UO-2 (with or without
 * extensions) packets.
 *
 * @see decide_packet
 *
 * @param context The compression context
 * @return        The packet type among PACKET_UO_0, PACKET_UO_1 and
 *                PACKET_UOR_2
 */
int decide_SO_packet(const struct c_context *context)
{
	struct c_generic_context *g_context;
	int nr_of_ip_hdr, nr_sn_bits, nr_ip_id_bits, nr_ip_id_bits2;
	int packet;
	
	g_context = (struct c_generic_context *) context->specific;
	nr_of_ip_hdr = g_context->tmp_variables.nr_of_ip_hdr;
	nr_sn_bits = g_context->tmp_variables.nr_sn_bits;
	nr_ip_id_bits = g_context->tmp_variables.nr_ip_id_bits;
	nr_ip_id_bits2 = g_context->tmp_variables.nr_ip_id_bits2;

	rohc_debugf(3, "nr_ip_bits=%d nr_sn_bits=%d nr_of_ip_hdr=%d\n",
	            nr_ip_id_bits, nr_sn_bits, nr_of_ip_hdr);

	packet = PACKET_UOR_2; /* default packet type */

	if(nr_of_ip_hdr == 1)
	{
		if(g_context->ip_flags.rnd == 1 && nr_sn_bits <= 4)
			packet = PACKET_UO_0;
		else if(nr_sn_bits <= 4 && nr_ip_id_bits == 0)
			packet = PACKET_UO_0;
		else if(nr_sn_bits == 5 && nr_ip_id_bits == 0)
			packet = PACKET_UOR_2;
		else if(nr_sn_bits <= 5 && nr_ip_id_bits <= 6)
			packet = PACKET_UO_1;
	}
	else
	{
		if(g_context->ip_flags.rnd == 1 && g_context->ip2_flags.rnd == 1 &&
		   nr_sn_bits <= 4)
			packet = PACKET_UO_0;
		else if(nr_sn_bits <= 4 &&
		        (g_context->ip_flags.rnd == 1 || nr_ip_id_bits == 0) &&
		        (g_context->ip2_flags.rnd == 1 || nr_ip_id_bits2 ==0))
			packet = PACKET_UO_0;
		else if(nr_sn_bits <= 5 && nr_ip_id_bits <= 6 &&
		        (g_context->ip2_flags.rnd == 1 || nr_ip_id_bits2 == 0))
			packet = PACKET_UO_1;
	}

	return packet;
}


/**
 * @brief Decide which packet to send when in the different states.
 *
 * In IR state, IR packets are used. In FO and SO, the decide_FO_packet and
 * decide_SO_packet are used to decide which packet to send.
 *
 * @see decide_FO_packet
 * @see decide_SO_packet
 *
 * @param context The compression context
 * @return        The packet type among PACKET_IR, PACKET_IR_DYN, PACKET_UO_0,
 *                PACKET_UO_1 and PACKET_UOR_2
 */
int decide_packet(struct c_context *context)
{
	struct c_generic_context *g_context;
	int packet;
	
	g_context = (struct c_generic_context *) context->specific;

	packet = PACKET_IR; /* default packet type */

	switch(context->state)
	{
		case IR:
			rohc_debugf(2, "IR state\n");
			g_context->ir_count++;
			packet = PACKET_IR;
			break;

		case FO:
			rohc_debugf(2, "FO state\n");
			g_context->fo_count++;
			packet = decide_FO_packet(context);
			break;

		case SO:
			rohc_debugf(2, "SO state\n");
			g_context->so_count++;
			packet = decide_SO_packet(context);
			break;

		default:
			/* impossible value */
			rohc_debugf(2, "unknown state (%d) => IR packet\n", context->state);
	}

	return packet;
}


/**
 * @brief Build the ROHC packet to send.
 *
 * @param context     The compression context
 * @param ip          The outer IP header
 * @param ip2         The inner IP header
 * @param next_header The next header such as UDP or UDP-Lite
 * @param dest        The rohc-packet-under-build buffer
 * @return            The position in the rohc-packet-under-build buffer 
 */
int code_packet(struct c_context *context,
                const struct iphdr *ip,
                const struct iphdr *ip2,
                const unsigned char *next_header,
                unsigned char *dest)
{
	struct c_generic_context *g_context;
	int nr_of_ip_hdr, packet_type;
	int (*code_packet_type)(struct c_context *context, const struct iphdr *ip,
	                        const struct iphdr *ip2,
	                        const unsigned char *next_header,
	                        unsigned char *dest);
	int counter;

	g_context = (struct c_generic_context *) context->specific;
	nr_of_ip_hdr = g_context->tmp_variables.nr_of_ip_hdr;
	packet_type = g_context->tmp_variables.packet_type;

	code_packet_type = NULL;
	counter = -1;

	switch(packet_type)
	{
		case PACKET_IR:
			code_packet_type = code_IR_packet;
			break;

		case PACKET_IR_DYN:
			code_packet_type = code_IR_DYN_packet;
			break;

		case PACKET_UO_0:
			code_packet_type = code_UO0_packet;
			break;

		case PACKET_UO_1:
			code_packet_type = code_UO1_packet;
			break;

		case PACKET_UOR_2:
			code_packet_type = code_UO2_packet;
			break;

		default:
			rohc_debugf(0, "unknown packet, failure\n");
			break;
	}

	if(code_packet != NULL)
		counter = code_packet_type(context, ip, ip2, next_header, dest);

	return counter;
}


/**
 * @brief Build the IR packet.
 *
 * \verbatim

 IR packet (5.7.7.1):

      0   1   2   3   4   5   6   7
     --- --- --- --- --- --- --- ---
 1  |         Add-CID octet         |  if for small CIDs and CID != 0
    +---+---+---+---+---+---+---+---+
 2  | 1   1   1   1   1   1   0 | D |
    +---+---+---+---+---+---+---+---+
    |                               |
 3  /    0-2 octets of CID info     /  1-2 octets if for large CIDs
    |                               |
    +---+---+---+---+---+---+---+---+
 4  |            Profile            |  1 octet
    +---+---+---+---+---+---+---+---+
 5  |              CRC              |  1 octet
    +---+---+---+---+---+---+---+---+
    |                               |
 6  |         Static chain          |  variable length
    |                               |
    +---+---+---+---+---+---+---+---+
    |                               |
 7  |         Dynamic chain         |  present if D = 1, variable length
    |                               |
    +---+---+---+---+---+---+---+---+
 8  |             SN                | 2 octets
    +---+---+---+---+---+---+---+---+
    |                               |
    |           Payload             |  variable length
    |                               |
     - - - - - - - - - - - - - - - -

\endverbatim
 *
 * @param context        The compression context
 * @param ip             The outer IP header
 * @param ip2            The inner IP header
 * @param next_header    The next header data used to code the static and
 *                       dynamic parts of the next header for some profiles such
 *                       as UDP, UDP-Lite, and so on.
 * @param dest           The rohc-packet-under-build buffer
 * @return               The position in the rohc-packet-under-build buffer 
 */
int code_IR_packet(struct c_context *context,
                   const struct iphdr *ip,
                   const struct iphdr *ip2,
                   const unsigned char *next_header,
                   unsigned char *dest)
{
	struct c_generic_context *g_context;
	int nr_of_ip_hdr;
	unsigned char type;
	int counter;
	int first_position, crc_position;

	g_context = (struct c_generic_context *) context->specific;
	nr_of_ip_hdr = g_context->tmp_variables.nr_of_ip_hdr;

	rohc_debugf(2, "code IR packet (CID = %d)\n", context->cid);

	/* parts 1 and 3:
	 *  - part 2 will be placed at 'first_position'
	 *  - part 4 will start at 'counter'
	 */
	counter = code_cid_values(context, dest, g_context->tmp_variables.max_size,
	                          &first_position);

	/* initialize some profile-specific things when building an IR
	 * or IR-DYN packet */
	if(g_context->init_at_IR != NULL)
		g_context->init_at_IR(context, next_header);

	/* part 2: type of packet and D flag if dynamic part is included */
	type = 0xfc;
	type |= 1; /* D flag */
	rohc_debugf(3, "type of packet + D flag = 0x%02x\n", type);
	dest[first_position] = type;

	/* part 4 */
	rohc_debugf(3, "profile ID = 0x%02x\n", context->profile->id);
	dest[counter] = context->profile->id;
	counter++;

	/* part 5: the CRC is computed later since it must be computed
	 * over the whole packet with an empty CRC field */
	rohc_debugf(3, "CRC = 0x00 for CRC calculation\n");
	crc_position = counter;
	dest[counter] = 0;
	counter++;

	/* part 6: static part */
	counter = code_generic_static_part(context, &g_context->ip_flags, ip, dest, counter);
	if(counter < 0)
		goto error;

	if(nr_of_ip_hdr > 1)
	{
		counter = code_generic_static_part(context, &g_context->ip2_flags, ip2,
		                           dest, counter);
		if(counter < 0)
			goto error;
	}

	if(g_context->code_static_part != NULL && next_header != NULL)
	{
		/* static part of next header */
		counter = g_context->code_static_part(context, next_header,
		                                      dest, counter);
		if(counter < 0)
			goto error;
	}

	/* part 7: if we do not want dynamic part in IR packet, we should not
	 * send the following */
	counter = code_generic_dynamic_part(context, &g_context->ip_flags, ip,
	                                    dest, counter);
	if(counter < 0)
		goto error;

	if(nr_of_ip_hdr > 1)
	{
		counter = code_generic_dynamic_part(context, &g_context->ip2_flags, ip2,
		                                    dest, counter);
		if(counter < 0)
			goto error;
	}

	if(g_context->code_dynamic_part != NULL && next_header != NULL)
	{
		/* dynamic part of next header */
		counter = g_context->code_dynamic_part(context, next_header,
		                                       dest, counter);
		if(counter < 0)
			goto error;
	}

	/* part 8 */
	dest[counter] = g_context->sn >> 8;
	counter++;
	dest[counter] = g_context->sn & 0xff;
	counter++;
	rohc_debugf(3, "SN = %d -> 0x%02x%02x\n", g_context->sn, dest[counter-2], dest[counter-1]);

	/* part 5 */
	dest[crc_position] = crc_calculate(CRC_TYPE_8, dest, counter);
	rohc_debugf(3, "CRC (header length = %d, crc = 0x%x)\n",
	            counter, dest[crc_position]);

error:
	return counter;
}



/**
 * @brief Build the IR-DYN packet.
 *
 * \verbatim

 IR-DYN packet (5.7.7.2):

      0   1   2   3   4   5   6   7
     --- --- --- --- --- --- --- ---
 1  :         Add-CID octet         : if for small CIDs and CID != 0
    +---+---+---+---+---+---+---+---+
 2  | 1   1   1   1   1   0   0   0 | IR-DYN packet type
    +---+---+---+---+---+---+---+---+
    :                               :
 3  /     0-2 octets of CID info    / 1-2 octets if for large CIDs
    :                               :
    +---+---+---+---+---+---+---+---+
 4  |            Profile            | 1 octet
    +---+---+---+---+---+---+---+---+
 5  |              CRC              | 1 octet
    +---+---+---+---+---+---+---+---+
    |                               |
 6  /         Dynamic chain         / variable length
    |                               |
    +---+---+---+---+---+---+---+---+
 7  |             SN                | 2 octets
    +---+---+---+---+---+---+---+---+
    :                               :
    /           Payload             / variable length
    :                               :
     - - - - - - - - - - - - - - - -

\endverbatim
 *
 * @param context        The compression context
 * @param ip             The outer IP header
 * @param ip2            The inner IP header
 * @param next_header    The next header data used to code the dynamic part
 *                       of the next header for some profiles such as UDP,
 *                       UDP-Lite, etc.
 * @param dest           The rohc-packet-under-build buffer
 * @return               The position in the rohc-packet-under-build buffer 
 */
int code_IR_DYN_packet(struct c_context *context,
                       const struct iphdr *ip,
                       const struct iphdr *ip2,
                       const unsigned char *next_header,
                       unsigned char *dest)
{
	struct c_generic_context *g_context;
	int nr_of_ip_hdr;
	int counter;
	int first_position, crc_position;

	g_context = (struct c_generic_context *) context->specific;
	nr_of_ip_hdr = g_context->tmp_variables.nr_of_ip_hdr;

	rohc_debugf(2, "code IR-DYN packet (CID = %d)\n", context->cid);

	/* parts 1 and 3:
	 *  - part 2 will be placed at 'first_position'
	 *  - part 4 will start at 'counter'
	 */
	counter = code_cid_values(context, dest, g_context->tmp_variables.max_size,
	                          &first_position);

	/* initialize some profile-specific things when building an IR
	 * or IR-DYN packet */
	if(g_context->init_at_IR != NULL)
		g_context->init_at_IR(context, next_header);

	/* part 2 */
	dest[first_position] = 0xf8;

	/* part 4 */
	dest[counter] = context->profile->id;
	counter++;

	/* part 5: the CRC is computed later since it must be computed
	 * over the whole packet with an empty CRC field */
	crc_position = counter;
	dest[counter] = 0;
	counter++;

	/* part 6: dynamic part of outer and inner IP header and dynamic part
	 * of next header */
	counter = code_generic_dynamic_part(context, &g_context->ip_flags, ip,
	                            dest, counter);
	if(counter < 0)
		goto error;

	if(nr_of_ip_hdr > 1)
	{
		counter = code_generic_dynamic_part(context, &g_context->ip2_flags, ip2,
		                            dest, counter);
		if(counter < 0)
			goto error;
	}

	if(g_context->code_dynamic_part != NULL && next_header != NULL)
	{
		/* dynamic part of next header */
		counter = g_context->code_dynamic_part(context, next_header, dest, counter);
		if(counter < 0)
			goto error;
	}

	/* part 7 */
	rohc_debugf(3, "SN = %d\n", g_context->sn);
	dest[counter] = g_context->sn >> 8;
	counter++;
	dest[counter] = g_context->sn & 0xff;
	counter++;

	/* part 5 */
	dest[crc_position] = crc_calculate(CRC_TYPE_8, dest, counter);
	rohc_debugf(3, "CRC (header length = %d, crc = 0x%x)\n",
	            counter, dest[crc_position]);

error:
	return counter;
}


/**
 * @brief Build the static part of the IR and IR-DYN packets.
 *
 * \verbatim

 Static part IPv4 (5.7.7.4):

    +---+---+---+---+---+---+---+---+
 1  |  Version = 4  |       0       |
    +---+---+---+---+---+---+---+---+
 2  |           Protocol            |
    +---+---+---+---+---+---+---+---+
 3  /        Source Address         /   4 octets
    +---+---+---+---+---+---+---+---+
 4  /      Destination Address      /   4 octets
    +---+---+---+---+---+---+---+---+

\endverbatim
 *
 * @param context     The compression context
 * @param header_info The header info stored in the profile
 * @param ip          The IP header the static part is built for
 * @param dest        The rohc-packet-under-build buffer
 * @param counter     The current position in the rohc-packet-under-build buffer
 * @return            The new position in the rohc-packet-under-build buffer 
*/
int code_generic_static_part(struct c_context *context,
                             struct ip_header_info *header_info,
                             const struct iphdr *ip,
                             unsigned char *dest,
                             int counter)
{
	/* part 1 */
	dest[counter] = 0x40;
	counter++;

	/* part 2 */
	rohc_debugf(3, "protocol = 0x%02x\n", ip->protocol);
	dest[counter] = ip->protocol;
	counter++;
	header_info->protocol_count++;

	/* part 3 */
	memcpy(&dest[counter], &ip->saddr, 4);
	rohc_debugf(3, "src addr = %02x %02x %02x %02x\n",
	            dest[counter], dest[counter + 1],
	            dest[counter + 2], dest[counter + 3]);
	counter += 4;

	/* part 4 */
	memcpy(&dest[counter], &ip->daddr, 4);
	rohc_debugf(3, "dst addr = %02x %02x %02x %02x\n",
	            dest[counter], dest[counter + 1],
	            dest[counter + 2], dest[counter + 3]);
	counter += 4;

	return counter;
}


/**
 * @brief Build the dynamic part of the IR and IR-DYN packets.
 *
 * \verbatim

 Dynamic part IPv4 (5.7.7.4):
 
    +---+---+---+---+---+---+---+---+
 1  |        Type of Service        |
   +---+---+---+---+---+---+---+---+
 2  |         Time to Live          |
    +---+---+---+---+---+---+---+---+
 3  /        Identification         /   2 octets
    +---+---+---+---+---+---+---+---+
 4  | DF|RND|NBO|         0         |
    +---+---+---+---+---+---+---+---+
 5  / Generic extension header list /  variable length
    +---+---+---+---+---+---+---+---+

\endverbatim
 *
 * @param context     The compression context
 * @param header_info The header info stored in the profile
 * @param ip          The IP header the dynamic part is built for
 * @param dest        The rohc-packet-under-build buffer
 * @param counter     The current position in the rohc-packet-under-build buffer
 * @return            The new position in the rohc-packet-under-build buffer 
 */
int code_generic_dynamic_part(struct c_context *context,
                              struct ip_header_info *header_info,
                              const struct iphdr *ip,
                              unsigned char *dest,
                              int counter)
{
	unsigned char df_rnd_nbo;
	int dont_fragment;
	unsigned short fragment;

	/* part 1 */
	dest[counter] = ip->tos;
	counter++;
	header_info->tos_count++;

	/* part 2 */
	dest[counter] = ip->ttl;
	counter++;
	header_info->ttl_count++;

	/* part 3 */
	memcpy(&dest[counter], &ip->id, 2);
	counter += 2;

	/* part 4 */
	fragment = ntohs(ip->frag_off);
	dont_fragment = (fragment >> 14) & 1;

	/* discard IP fragments:
	 *  - the R (Reserved) and MF (More Fragments) bits must be zero
	 *  - the Fragment Offset field must be zero
	 *  => ip->frag_off must be zero except the DF (Don't Fragment) bit
	 */
	if((fragment & (~IP_DF)) != 0)
	{
		rohc_debugf(0, "fragment error (0x%04x)\n", fragment);
		return -1;
	}

	df_rnd_nbo = dont_fragment << 7;
	if(header_info->rnd)
		df_rnd_nbo |= 0x40;
	if(header_info->nbo)
		df_rnd_nbo |= 0x20;

	dest[counter] = df_rnd_nbo;
	counter++;

	header_info->df_count++;
	header_info->rnd_count++;
	header_info->nbo_count++;

	/* part 5 is not supported for the moment */

	rohc_debugf(3, "TOS = 0x%02x, TTL = 0x%02x, IP-ID = 0x%04x, df_rnd_nbo = "
	            "0x%02x (DF = %d, RND = %d, NBO = %d)\n", ip->tos, ip->ttl,
	            ip->id, df_rnd_nbo, dont_fragment, header_info->rnd,
	            header_info->nbo);

	return counter;
}


/**
 * @brief Build the tail of the UO packet.
 *
 * \verbatim

 The general format for the UO packets is:

      0   1   2   3   4   5   6   7
     --- --- --- --- --- --- --- ---
 1  :         Add-CID octet         :                    |
    +---+---+---+---+---+---+---+---+                    |
 2  |   first octet of base header  |                    |
    +---+---+---+---+---+---+---+---+                    |
    :                               :                    |
 3  /   0, 1, or 2 octets of CID    /                    |
    :                               :                    |
    +---+---+---+---+---+---+---+---+                    |
 4  /   remainder of base header    /                    |
    +---+---+---+---+---+---+---+---+                    |
    :                               :                    |
 5  /           Extension           /                    |
    :                               :                    |
     --- --- --- --- --- --- --- ---                     |
    :                               :                    |
 6  +   IP-ID of outer IPv4 header  +
    :                               :     (see section 5.7 or [RFC-3095])
     --- --- --- --- --- --- --- ---
 7  /    AH data for outer list     /                    |
     --- --- --- --- --- --- --- ---                     |
    :                               :                    |
 8  +         GRE checksum          +                    |
    :                               :                    |
     --- --- --- --- --- --- --- ---                     |
    :                               :                    |
 9  +   IP-ID of inner IPv4 header  +                    |
    :                               :                    |
     --- --- --- --- --- --- --- ---                     |
 10 /    AH data for inner list     /                    |
     --- --- --- --- --- --- --- ---                     |
    :                               :                    |
 11 +         GRE checksum          +                    |
    :                               :                    |
     --- --- --- --- --- --- --- ---
    :            List of            :
 12 /        Dynamic chains         /    variable, given by static chain
    :   for additional IP headers   :           (includes no SN)
     --- --- --- --- --- --- --- ---

\endverbatim
 *
 * Parts 7, 8, 10, 11 and 12 are not supported. Parts 1, 2, 3, 4 and 5 are
 * built in packet-specific functions. Parts 6 and 9 are built in this
 * function.
 *
 * @param context     The compression context
 * @param ip          The outer IP header
 * @param ip2         The inner IP header
 * @param next_header The next header such as UDP or UDP-Lite
 * @param dest        The rohc-packet-under-build buffer
 * @param counter     The current position in the rohc-packet-under-build buffer
 * @return            The new position in the rohc-packet-under-build buffer 
 */
int code_UO_packet_tail(struct c_context *context,
                        const struct iphdr *ip,
                        const struct iphdr *ip2,
                        const unsigned char *next_header,
                        unsigned char *dest,
								int counter)
{
	struct c_generic_context *g_context;
	int nr_of_ip_hdr;

	g_context = (struct c_generic_context *) context->specific;
	nr_of_ip_hdr = g_context->tmp_variables.nr_of_ip_hdr;

	/* parts 6 */
	if(g_context->ip_flags.rnd == 1)
	{
		memcpy(&dest[counter], &ip->id, 2);
		counter += 2;
	}

	/* parts 7 and 8 are not supported */

	/* step 9 */
	if(nr_of_ip_hdr > 1 && g_context->ip2_flags.rnd == 1)
	{
		memcpy(&dest[counter], &ip2->id, 2);
		counter += 2;
	}
	
	/* parts 10, 11 and 12 are not supported */

	/* add fields related to the next header */
	if(g_context->code_UO_packet_tail != NULL && next_header != NULL)
		counter = g_context->code_UO_packet_tail(context, next_header,
		                                         dest, counter);

	return counter;
}


/**
 * @brief Build the UO-0 packet.
 *
 * \verbatim

      0   1   2   3   4   5   6   7
     --- --- --- --- --- --- --- ---
 1  :         Add-CID octet         :
    +---+---+---+---+---+---+---+---+
 2  |   first octet of base header  |
    +---+---+---+---+---+---+---+---+
    :                               :
 3  /   0, 1, or 2 octets of CID    /
    :                               :
    +---+---+---+---+---+---+---+---+
 
 UO-0 (5.7.1)
 
      0   1   2   3   4   5   6   7
    +---+---+---+---+---+---+---+---+
 2  | 0 |      SN       |    CRC    |
    +===+===+===+===+===+===+===+===+

\endverbatim
 *
 * @param context        The compression context
 * @param ip             The outer IP header
 * @param ip2            The inner IP header
 * @param next_header    The next header such as UDP or UDP-Lite
 * @param dest           The rohc-packet-under-build buffer
 * @return               The position in the rohc-packet-under-build buffer 
 */
int code_UO0_packet(struct c_context *context,
                    const struct iphdr *ip,
                    const struct iphdr *ip2,
                    const unsigned char *next_header,
                    unsigned char *dest)
{
	int counter;
	int first_position;
	unsigned char f_byte;
	struct c_generic_context *g_context;
	int nr_of_ip_hdr;

	g_context = (struct c_generic_context *) context->specific;
	nr_of_ip_hdr = g_context->tmp_variables.nr_of_ip_hdr;

	rohc_debugf(2, "code UO-0 packet (CID = %d)\n", context->cid);

	/* parts 1 and 3:
	 *  - part 2 will be placed at 'first_position'
	 *  - part 4 will start at 'counter'
	 */
	counter = code_cid_values(context, dest, g_context->tmp_variables.max_size,
	                          &first_position);

	/* build the UO head if necessary */
	if(g_context->code_UO_packet_head != NULL && next_header != NULL)
		counter = g_context->code_UO_packet_head(context, next_header,
		                                         dest, counter, &first_position);

	/* part 2 */
	f_byte = (g_context->sn & 0x0f) << 3;
	f_byte |= crc_calculate(CRC_TYPE_3, (unsigned char *) ip,
	                        (nr_of_ip_hdr == 1 ? 1 : 2) * sizeof(struct iphdr) +
	                        g_context->next_header_len);
	dest[first_position] = f_byte;

	/* build the UO tail */
	counter = code_UO_packet_tail(context, ip, ip2, next_header, dest, counter);

	return counter;
}


/**
 * @brief Build the UO-1 packet.
 *
 * \verbatim

      0   1   2   3   4   5   6   7
     --- --- --- --- --- --- --- ---
 1  :         Add-CID octet         :
    +---+---+---+---+---+---+---+---+
 2  |   first octet of base header  |
    +---+---+---+---+---+---+---+---+
    :                               :
 3  /   0, 1, or 2 octets of CID    /
    :                               :
    +---+---+---+---+---+---+---+---+
 
 OU-1 (5.11.3)
 
      0   1   2   3   4   5   6   7
    +---+---+---+---+---+---+---+---+
 2  | 1   0 |         IP-ID         |
    +===+===+===+===+===+===+===+===+
 4  |        SN         |    CRC    |
    +---+---+---+---+---+---+---+---+

\endverbatim
 *
 * @param context        The compression context
 * @param ip             The outer IP header
 * @param ip2            The inner IP header
 * @param next_header    The next header such as UDP or UDP-Lite
 * @param dest           The rohc-packet-under-build buffer
 * @return               The position in the rohc-packet-under-build buffer 
 */
int code_UO1_packet(struct c_context *context,
                    const struct iphdr *ip,
                    const struct iphdr *ip2,
                    const unsigned char *next_header,
                    unsigned char *dest)
{
	int counter;
	int first_position;
	unsigned char f_byte;
	unsigned char s_byte;
	struct c_generic_context *g_context;
	int nr_of_ip_hdr;

	g_context = (struct c_generic_context *) context->specific;
	nr_of_ip_hdr = g_context->tmp_variables.nr_of_ip_hdr;

	rohc_debugf(2, "code UO-1 packet (CID = %d)\n", context->cid);

	/* parts 1 and 3:
	 *  - part 2 will be placed at 'first_position'
	 *  - part 4 will start at 'counter'
	 */
	counter = code_cid_values(context, dest, g_context->tmp_variables.max_size,
	                          &first_position);

	/* build the UO head if necessary */
	if(g_context->code_UO_packet_head != NULL && next_header != NULL)
		counter = g_context->code_UO_packet_head(context, next_header,
		                                         dest, counter, &first_position);

	/* part 2 */
	f_byte = g_context->ip_flags.id_delta & 0x3f;
	f_byte |= 0x80;
	dest[first_position] = f_byte;

	/* part 4 */
	s_byte = (g_context->sn & 0x1f) << 3;
	s_byte |= crc_calculate(CRC_TYPE_3, (unsigned char *) ip,
	                        (nr_of_ip_hdr == 1 ? 1 : 2) * sizeof(struct iphdr) +
	                        g_context->next_header_len);
	dest[counter] = s_byte;
	counter++;

	/* build the UO tail */
	counter = code_UO_packet_tail(context, ip, ip2, next_header, dest, counter);

	return counter;
}


/**
 * @brief Build the UO-2 packet.
 *
 * \verbatim

      0   1   2   3   4   5   6   7
     --- --- --- --- --- --- --- ---
 1  :         Add-CID octet         :
    +---+---+---+---+---+---+---+---+
 2  |   first octet of base header  |
    +---+---+---+---+---+---+---+---+
    :                               :
 3  /   0, 1, or 2 octets of CID    /
    :                               :
    +---+---+---+---+---+---+---+---+
 
 OU-2 (5.11.3):
 
      0   1   2   3   4   5   6   7
    +---+---+---+---+---+---+---+---+
 2  | 1   1   0 |        SN         |
    +===+===+===+===+===+===+===+===+
 4  | X |            CRC            |
    +---+---+---+---+---+---+---+---+
 
    +---+---+---+---+---+---+---+---+
    :                               :
 5  /           Extension           /
    :                               :
     --- --- --- --- --- --- --- ---

\endverbatim
 *
 * @param context        The compression context
 * @param ip             The outer IP header
 * @param ip2            The inner IP header
 * @param next_header    The next header such as UDP or UDP-Lite
 * @param dest           The rohc-packet-under-build buffer
 * @return               The position in the rohc-packet-under-build buffer 
 */
int code_UO2_packet(struct c_context *context,
                    const struct iphdr *ip,
                    const struct iphdr *ip2,
                    const unsigned char *next_header,
                    unsigned char *dest)
{
	unsigned char f_byte;
	unsigned char s_byte;
	int counter;
	int first_position, s_byte_position;
	int extension;
	struct c_generic_context *g_context;
	int nr_sn_bits;
	int nr_of_ip_hdr;

	g_context = (struct c_generic_context *) context->specific;
	nr_of_ip_hdr = g_context->tmp_variables.nr_of_ip_hdr;
	nr_sn_bits = g_context->tmp_variables.nr_sn_bits;

	rohc_debugf(2, "code UO-2 packet (CID = %d)\n", context->cid);

	/* parts 1 and 3:
	 *  - part 2 will be placed at 'first_position'
	 *  - part 4 will start at 'counter'
	 */
	counter = code_cid_values(context, dest, g_context->tmp_variables.max_size,
	                          &first_position);

	/* build the UO head if necessary */
	if(g_context->code_UO_packet_head != NULL && next_header != NULL)
		counter = g_context->code_UO_packet_head(context, next_header,
		                                         dest, counter, &first_position);

	/* part 2: to be continued, we need to add the 4 bits of SN */
	f_byte = 0xc0;

	/* part 4: partially calculate the s-byte value, then remember the position
	 * of the s-byte, its final value is currently unknown */
	s_byte = crc_calculate(CRC_TYPE_7, (unsigned char *) ip,
	                       nr_of_ip_hdr * sizeof(struct iphdr) +
	                       g_context->next_header_len);
	s_byte_position = counter;
	counter++;

	/* part 5 */
	extension = decide_extension(context);

	switch(extension)
	{
		case PACKET_NOEXT:
			rohc_debugf(1, "no extension\n");
			/* part 2 */
			f_byte |= g_context->sn & 0x1f;
			/* part 4: set the X bit to 0 */
			s_byte &= ~0x80;
			/* part 5: nothing to do */
			break;

		case PACKET_EXT_0:
			rohc_debugf(1, "using extension 0\n");
			/* part 2 */
			f_byte |= (g_context->sn & 0xff) >> 3;
			/* part 4: set the X bit to 1 */
			s_byte |= 0x80;
			/* part 5 */
			counter = code_EXT0_packet(context, dest, counter);
			break;

		case PACKET_EXT_1:
			rohc_debugf(1, "using extension 1\n");
			/* part 2 */
			f_byte |= (g_context->sn & 0xff) >> 3;
			/* part 4: set the X bit to 1 */
			s_byte |= 0x80;
			/* part 5 */
			counter = code_EXT1_packet(context, dest, counter);
			break;

		case PACKET_EXT_2:
			rohc_debugf(1, "using extension 2\n");
			/* part 2 */
			f_byte |= (g_context->sn & 0xff) >> 3;
			/* part 4: set the X bit to 1 */
			s_byte |= 0x80;
			/* part 5 */
			counter = code_EXT2_packet(context, dest, counter);
			break;

		case PACKET_EXT_3:
			rohc_debugf(1, "using extension 3\n");
			/* part 2: check if the s-field needs to be used */
			if(nr_sn_bits > 5)
				f_byte |= g_context->sn >> 8;
			else
				f_byte |= g_context->sn & 0x1f;
			/* part 4: set the X bit to 1 */
			s_byte |= 0x80;
			/* part 5 */
			counter = code_EXT3_packet(context, ip, ip2, dest, counter);
			break;

		default:
			rohc_debugf(0, "unknown extension (%d)\n", extension);
	}

	dest[first_position] = f_byte;
	dest[s_byte_position] = s_byte;

	/* build the UO tail */
	counter = code_UO_packet_tail(context, ip, ip2, next_header, dest, counter);

	return counter;
}


/**
 * @brief Build the extension 0 of the UO-2 packet.
 *
 * \verbatim

 Extension 0 (5.11.4):
 
    +---+---+---+---+---+---+---+---+
 1  | 0   0 |    SN     |   IP-ID   |
    +---+---+---+---+---+---+---+---+

\endverbatim
 *
 * @param context    The compression context
 * @param dest       The rohc-packet-under-build buffer
 * @param counter    The current position in the rohc-packet-under-build buffer
 * @return           The new position in the rohc-packet-under-build buffer 
 */
int code_EXT0_packet(struct c_context *context,
                     unsigned char *dest,
                     int counter)
{
	struct c_generic_context *g_context;
	unsigned char f_byte;

	g_context = (struct c_generic_context *) context->specific;

	/* part 1 */
	f_byte = (g_context->sn & 0x07) << 3;
	f_byte |= g_context->ip_flags.id_delta & 0x07;
	dest[counter] = f_byte;
	counter++;

	return counter;
}


/**
 * @brief Build the extension 1 of the UO-2 packet.
 *
 * \verbatim

 Extension 1 (5.11.4):
 
    +---+---+---+---+---+---+---+---+
 1  | 0   1 |    SN     |   IP-ID   |
    +---+---+---+---+---+---+---+---+
 2  |             IP-ID             |
    +---+---+---+---+---+---+---+---+

\endverbatim
 *
 * @param context    The compression context
 * @param dest       The rohc-packet-under-build buffer
 * @param counter    The current position in the rohc-packet-under-build buffer
 * @return           The new position in the rohc-packet-under-build buffer 
 */
int code_EXT1_packet(struct c_context *context,
                     unsigned char *dest,
                     int counter)
{
   struct c_generic_context *g_context;
	unsigned char f_byte;
	unsigned char s_byte;

	g_context = (struct c_generic_context *) context->specific;

	/* part 1 */
	f_byte = (g_context->sn & 0x07) << 3;
	f_byte |= (g_context->ip_flags.id_delta & 0x0700) >> 8;
	f_byte |= 0x40;
	dest[counter] = f_byte;
	counter++;

	/* part 2 */
	s_byte = g_context->ip_flags.id_delta & 0xff;
	dest[counter] = s_byte;
	counter++;

	return counter;
}


/**
 * @brief Build the extension 2 of the UO-2 packet.
 *
 * \verbatim

 Extension 2 (5.11.4):
 
    +---+---+---+---+---+---+---+---+
 1  | 1   0 |    SN     |   IP-ID2  |
    +---+---+---+---+---+---+---+---+
 2  |            IP-ID2             |
    +---+---+---+---+---+---+---+---+
 3  |             IP-ID             |
    +---+---+---+---+---+---+---+---+

 IP-ID2 is for outer IP-ID field.

\endverbatim
 *
 * @param context    The compression context
 * @param dest       The rohc-packet-under-build buffer
 * @param counter    The current position in the rohc-packet-under-build buffer
 * @return           The new position in the rohc-packet-under-build buffer 
 */
int code_EXT2_packet(struct c_context *context,
                     unsigned char *dest,
                     int counter)
{
	struct c_generic_context *g_context;
	unsigned char f_byte;
	unsigned char s_byte;

	g_context = (struct c_generic_context *) context->specific;

	/* To avoid confusion:
	 *  - IP-ID2 in the header description is related to the outer IP header
	 *    and thus to the g_context->ip_flags header info,
	 *  - IP-ID in the header description is related to the inner IP header
	 *    and thus to the g_context->ip2_flags header info.
	 */

	/* part 1 */
	f_byte = (g_context->sn & 0x07) << 3;
	f_byte |= (g_context->ip_flags.id_delta & 0x0700) >> 8;
	f_byte |= 0x80;
	dest[counter] = f_byte;
	counter++;

	/* part 2 */
	s_byte = g_context->ip_flags.id_delta & 0xff;
	dest[counter] = s_byte;
	counter++;

	/* part 3 */
	dest[counter] = g_context->ip2_flags.id_delta & 0xff;
	counter++;

	return counter;
}


/**
 * @brief Build the extension 3 of the UO-2 packet.
 *
 * \verbatim

 Extension 3 (5.7.5 && 5.11.4):
 
       0     1     2     3     4     5     6     7
    +-----+-----+-----+-----+-----+-----+-----+-----+
 1  |  1     1  |  S  |   Mode    |  I  | ip  | ip2 |
    +-----+-----+-----+-----+-----+-----+-----+-----+
 2  |            Inner IP header flags        |     |  if ip = 1
    +-----+-----+-----+-----+-----+-----+-----+-----+
 3  |            Outer IP header flags              |
    +-----+-----+-----+-----+-----+-----+-----+-----+
 4  |                      SN                       |  if S = 1
    +-----+-----+-----+-----+-----+-----+-----+-----+
    |                                               |
 5  /            Inner IP header fields             /  variable,
    |                                               |
    +-----+-----+-----+-----+-----+-----+-----+-----+
 6  |                     IP-ID                     |  2 octets, if I = 1
    +-----+-----+-----+-----+-----+-----+-----+-----+
    |                                               |
 7  /            Outer IP header fields             /  variable,
    |                                               |
    +-----+-----+-----+-----+-----+-----+-----+-----+

\endverbatim
 *
 * @param context    The compression context
 * @param ip         The outer IP header
 * @param ip2        The inner IP header
 * @param dest       The rohc-packet-under-build buffer
 * @param counter    The current position in the rohc-packet-under-build buffer
 * @return           The new position in the rohc-packet-under-build buffer 
 */
int code_EXT3_packet(struct c_context *context,
                     const struct iphdr *ip,
                     const struct iphdr *ip2,
                     unsigned char *dest,
                     int counter)
{
	struct c_generic_context *g_context;
	unsigned char f_byte;
	int nr_of_ip_hdr;
	int nr_sn_bits;
	unsigned short changed_f, changed_f2;
	int nr_ip_id_bits, nr_ip_id_bits2;
	boolean have_inner = 0;
	boolean have_outer = 0;

	g_context = (struct c_generic_context *) context->specific;
	nr_of_ip_hdr = g_context->tmp_variables.nr_of_ip_hdr;
	nr_sn_bits = g_context->tmp_variables.nr_sn_bits;
	changed_f = g_context->tmp_variables.changed_fields;
	changed_f2 = g_context->tmp_variables.changed_fields2;
	nr_ip_id_bits = g_context->tmp_variables.nr_ip_id_bits;
	nr_ip_id_bits2 = g_context->tmp_variables.nr_ip_id_bits2;

	/* part 1 */
	f_byte = 0xc0;
	if(nr_sn_bits > 5)
		f_byte |= 0x20;

	f_byte = f_byte | (context->mode << 3);
	/* if random bit is set we have the IP-ID field outside this function */
	rohc_debugf(1, "rnd_count_up: %d \n", g_context->ip_flags.rnd_count);

	if(nr_of_ip_hdr == 1)
	{
		if((nr_ip_id_bits > 0 && g_context->ip_flags.rnd == 0) ||
		   (g_context->ip_flags.rnd_count < MAX_FO_COUNT &&
		    g_context->ip_flags.rnd == 0))
			f_byte = f_byte | 0x04;

		rohc_debugf(3, "check for changed fields in the inner IP header\n");
		if(changed_dynamic_one_hdr(changed_f, &g_context->ip_flags, ip, context) ||
		   changed_static_one_hdr(changed_f, &g_context->ip_flags, ip, context))
		{
			have_inner = 1;
			f_byte = f_byte | 0x02;
		}
	}
	else
	{
		if((nr_ip_id_bits > 0 && g_context->ip2_flags.rnd == 0) ||
		   (g_context->ip2_flags.rnd_count < MAX_FO_COUNT &&
		    g_context->ip2_flags.rnd == 0))
			f_byte = f_byte | 0x04;

		rohc_debugf(3, "check for changed fields in the outer IP header\n");
		if(changed_dynamic_one_hdr(changed_f, &g_context->ip_flags, ip, context) ||
		   changed_static_one_hdr(changed_f, &g_context->ip_flags, ip, context))
		{
			have_outer = 1;
			f_byte = f_byte | 0x01;
		}

		rohc_debugf(3, "check for changed fields in the inner IP header\n");
		if(changed_dynamic_one_hdr(changed_f2, &g_context->ip2_flags, ip2, context) ||
		   changed_static_one_hdr(changed_f2, &g_context->ip2_flags, ip2, context))
		{
			have_inner = 1;
			f_byte = f_byte | 0x02;
		}
	}

	rohc_debugf(3, "first byte = 0x%02x\n", f_byte);
	dest[counter] = f_byte;
	counter++;

	if(nr_of_ip_hdr == 1)
	{
		/* part 2 */
		if(have_inner)
			counter = header_flags(context, &g_context->ip_flags, changed_f, ip,
			                       0, nr_ip_id_bits, dest, counter);

		/* part 4 */
		if(nr_sn_bits > 5)
		{
			dest[counter] = g_context->sn & 0xff;
			rohc_debugf(3, "SN = 0x%02x\n", dest[counter]);
			counter++;
		}

		/* part 5 */
		if(have_inner)
			counter = header_fields(context, &g_context->ip_flags, changed_f, ip,
			                        0, nr_ip_id_bits, dest, counter);

		/* part 6 */
		if((nr_ip_id_bits > 0 && g_context->ip_flags.rnd == 0) ||
		   (g_context->ip_flags.rnd_count-1 < MAX_FO_COUNT &&
		    g_context->ip_flags.rnd == 0))
		{
			memcpy(&dest[counter], &ip->id, 2);
			rohc_debugf(3, "IP ID = 0x%02x 0x%02x\n",
			            dest[counter], dest[counter + 1]);
			counter += 2;
		}
	}
	else
	{
		/* part 2 */
		if(have_inner)
			counter = header_flags(context, &g_context->ip2_flags, changed_f2, ip2,
			                       0, nr_ip_id_bits2, dest, counter);

		/* part 3 */
		if(have_outer)
			counter = header_flags(context, &g_context->ip_flags, changed_f, ip,
			                       1, nr_ip_id_bits, dest, counter);

		/* part 4 */
		if(nr_sn_bits > 5)
		{
			dest[counter] = g_context->sn & 0xff;
			counter++;
		}

		/* part 5 */
		if(have_inner)
			counter = header_fields(context, &g_context->ip2_flags, changed_f2, ip2,
			                        0, nr_ip_id_bits2, dest, counter);

		/* part 6 */
		if((nr_ip_id_bits2 > 0 && g_context->ip2_flags.rnd == 0) ||
		   (g_context->ip2_flags.rnd_count-1 < MAX_FO_COUNT &&
		    g_context->ip2_flags.rnd == 0))
		{
			memcpy(&dest[counter], &ip2->id, 2);
			rohc_debugf(3, "IP ID = 0x%02x 0x%02x\n",
			            dest[counter], dest[counter + 1]);
			counter += 2;
		}

		/* part 7 */
		if(have_outer)
			counter = header_fields(context, &g_context->ip_flags, changed_f, ip,
			                        1, nr_ip_id_bits, dest, counter);
	}

	/* no IP extension until list compression */

	return counter;
}


/**
 * @brief Check if a specified IP field has changed.
 *
 * @param changed_fields The fields that changed, created by the function
 *                       changed_fields
 * @param check_field    The field for which to check a change
 * @return               Whether the field changed or not
 *
 * @see changed_fields 
 */
boolean is_changed(unsigned short changed_fields, unsigned short check_field)
{
	return (changed_fields & check_field);
}


/**
 * @brief Build inner or outer IP header flags.
 *
 * The function must be called twice: one for inner and one for outer
 * with different arguments.
 *
 * \verbatim

 For inner flags:
 
    +-----+-----+-----+-----+-----+-----+-----+-----+
 1  |            Inner IP header flags        |     |  if ip = 1
    | TOS | TTL | DF  | PR  | IPX | NBO | RND | 0** |  0** reserved
    +-----+-----+-----+-----+-----+-----+-----+-----+
 
 or for outer flags:
 
    +-----+-----+-----+-----+-----+-----+-----+-----+
 2  |            Outer IP header flags              |
    | TOS2| TTL2| DF2 | PR2 |IPX2 |NBO2 |RND2 |  I2 |  if ip2 = 1
    +-----+-----+-----+-----+-----+-----+-----+-----+

\endverbatim
 *
 * @param context        The compression context
 * @param header_info    The header info stored in the profile
 * @param changed_f      The fields that changed, created by the function
 *                       changed_fields
 * @param ip             One inner or outer IP header
 * @param is_outer       Whether the IP header is the outer header or not
 * @param nr_ip_id_bits  The number of bits needed to transmit the IP-ID field
 * @param dest           The rohc-packet-under-build buffer
 * @param counter        The current position in the rohc-packet-under-build
 *                       buffer
 * @return               The new position in the rohc-packet-under-build buffer 
 *
 * @see changed_fields
 */
int header_flags(struct c_context *context,
                 struct ip_header_info *header_info,
                 unsigned short changed_f,
                 const struct iphdr *ip,
                 boolean is_outer,
                 int nr_ip_id_bits,
                 unsigned char *dest,
                 int counter)
{
	int flags = 0;

	/* for inner and outer flags (1 & 2) */
	if(is_changed(changed_f, MOD_TOS) || header_info->tos_count < MAX_FO_COUNT)
		flags |= 0x80;
	if(is_changed(changed_f, MOD_TTL) || header_info->ttl_count < MAX_FO_COUNT)
		flags |= 0x40;
	if(is_changed(changed_f, MOD_PROTOCOL) || header_info->protocol_count < MAX_FO_COUNT)
		flags |= 0x10;

	rohc_debugf(1, "DF = %d\n", GET_DF(ip->frag_off));
	header_info->df_count++;
	flags |= GET_DF(ip->frag_off) << 5;

	header_info->nbo_count++;
	flags |= header_info->nbo << 2;

	header_info->rnd_count++;
	flags |= header_info->rnd << 1;

	/* only for outer flags (only 2) */
	if(is_outer)
	{
		if((nr_ip_id_bits > 0 && (header_info->rnd == 0)) ||
		   (header_info->rnd_count-1 < MAX_FO_COUNT && header_info->rnd == 0))
			flags |= 0x01;
	}

	/* for inner and outer flags (1 & 2) */
	dest[counter] = flags;
	counter++;

	return counter;
}


/**
 * @brief Build inner or outer IP header fields.
 *
 * The function must be called twice: one for inner and one for outer
 * with different arguments.
 * 
 * \verbatim

    +-----+-----+-----+-----+-----+-----+-----+-----+
 1  |         Type of Service/Traffic Class         |  if TOS = 1
     ..... ..... ..... ..... ..... ..... ..... .....
 2  |         Time to Live/Hop Limit                |  if TTL = 1
     ..... ..... ..... ..... ..... ..... ..... .....
 3  |         Protocol/Next Header                  |  if PR = 1
     ..... ..... ..... ..... ..... ..... ..... .....
 4  /         IP extension headers                  /  variable, if IPX = 1
     ..... ..... ..... ..... ..... ..... ..... .....
 
    IP-ID is coded here for outer header fields although it doesn't look that
    way in the extension 3 picture in 5.7.5 and 5.11.4 of RFC 3095.
    +-----+-----+-----+-----+-----+-----+-----+-----+
 5  |                  IP-ID                        |  2 octets, if I = 1
    +-----+-----+-----+-----+-----+-----+-----+-----+

\endverbatim
 *
 * Step 4 is not supported.
 *
 * @param context        The compression context
 * @param header_info    The header info stored in the profile
 * @param changed_f      The fields that changed, created by the function
 *                       changed_fields
 * @param ip             One inner or outer IP header
 * @param is_outer       Whether the IP header is the outer header or not
 * @param nr_ip_id_bits  The number of bits needed to transmit the IP-ID field
 * @param dest           The rohc-packet-under-build buffer
 * @param counter        The current position in the rohc-packet-under-build
 *                       buffer
 * @return               The new position in the rohc-packet-under-build buffer 
 *
 * @see changed_fields
 */
int header_fields(struct c_context *context,
                  struct ip_header_info *header_info,
                  unsigned short changed_f,
                  const struct iphdr *ip,
                  boolean is_outer,
                  int nr_ip_id_bits,
                  unsigned char *dest,
                  int counter)
{
	/* part 1 */
	if(is_changed(changed_f, MOD_TOS) || header_info->tos_count < MAX_FO_COUNT)
	{
		rohc_debugf(3, "(outer = %d) IP TOS = 0x%02x\n", is_outer, ip->tos);
		header_info->tos_count++;
		dest[counter] = ip->tos;
		counter++;
	}

	/* part 2 */
	if(is_changed(changed_f, MOD_TTL) || header_info->ttl_count < MAX_FO_COUNT)
	{
		rohc_debugf(3, "(outer = %d) IP TTL = 0x%02x\n", is_outer, ip->ttl);
		header_info->ttl_count++;
		dest[counter] = ip->ttl;
		counter++;
	}

	/* part 3 */
	if(is_changed(changed_f, MOD_PROTOCOL) || header_info->protocol_count < MAX_FO_COUNT)
	{
		rohc_debugf(3, "(outer = %d) IP Protocol = 0x%02x\n", is_outer, ip->protocol);
		header_info->protocol_count++;
		dest[counter] = ip->protocol;
		counter++;
	}

	/* part 5 */
	if(is_outer)
	{
		if((nr_ip_id_bits > 0 && header_info->rnd == 0) ||
		   (header_info->rnd_count - 1 < MAX_FO_COUNT && header_info->rnd == 0))
		{
			memcpy(&dest[counter], &ip->id, 2);
			rohc_debugf(3, "(outer = %d) IP ID = 0x%02x 0x%02x\n", is_outer,
			            dest[counter], dest[counter + 1]);
			counter += 2;
		}
	}

	return counter;
}


/**
 * @brief Decide what extension shall be used in the UO-2 packet.
 *
 * @param context The compression context
 * @return        The extension code among PACKET_NOEXT, PACKET_EXT_0,
 *                PACKET_EXT_1 and PACKET_EXT_3
 */
int decide_extension(struct c_context *context)
{
	struct c_generic_context *g_context;
	int send_static;
	int send_dynamic;
	int nr_ip_id_bits;
	int nr_ip_id_bits2;
	int nr_sn_bits;
	int ext;
	
	g_context = (struct c_generic_context *) context->specific;
	send_static = g_context->tmp_variables.send_static;
	send_dynamic = g_context->tmp_variables.send_dynamic;
	nr_ip_id_bits = g_context->tmp_variables.nr_ip_id_bits;
	nr_ip_id_bits2 = g_context->tmp_variables.nr_ip_id_bits2;
	nr_sn_bits = g_context->tmp_variables.nr_sn_bits;

	ext = PACKET_EXT_3; /* default extension */

	if(g_context->tmp_variables.nr_of_ip_hdr == 1)
	{
		if(send_static > 0)
			ext = PACKET_EXT_3;
		else if(send_dynamic > 0)
			ext = PACKET_EXT_3;
		else if(nr_sn_bits < 5 && (nr_ip_id_bits == 0 || g_context->ip_flags.rnd == 1))
			ext = PACKET_NOEXT;
		else if(nr_sn_bits <= 8 && nr_ip_id_bits <= 3)
			ext = PACKET_EXT_0;
		else if(nr_sn_bits <= 8 && nr_ip_id_bits <= 11)
			ext = PACKET_EXT_1;
	}
	else
	{
		if(send_static > 0 || send_dynamic > 0)
			ext = PACKET_EXT_3;
		else if(nr_sn_bits < 5 && (nr_ip_id_bits == 0 || g_context->ip_flags.rnd == 1) &&
		        (nr_ip_id_bits2 == 0 || g_context->ip2_flags.rnd == 1))
			ext = PACKET_NOEXT;
		else if(nr_sn_bits <= 8 && nr_ip_id_bits <= 3 &&
		        (nr_ip_id_bits2 == 0 || g_context->ip2_flags.rnd == 1))
			ext = PACKET_EXT_0;
		else if(nr_sn_bits <= 8 && nr_ip_id_bits <= 11 &&
		        (nr_ip_id_bits2 == 0 || g_context->ip2_flags.rnd == 1))
			ext = PACKET_EXT_1;
		else if(nr_sn_bits <= 3 && nr_ip_id_bits <= 11 &&
		        nr_ip_id_bits2 <= 8)
			ext = PACKET_EXT_2;
	}

	return ext;
}


/**
 * @brief Check if the static parts of the context changed in any of the two
 *        IP headers.
 *
 * @param context The compression context
 * @param ip      The outer IP header
 * @param ip2     The inner IP header
 * @return        The number of fields that changed
 */
int changed_static_both_hdr(struct c_context *context,
                            const struct iphdr *ip,
                            const struct iphdr *ip2)
{
	int nb_fields = 0; /* number of fields that changed */
	unsigned short changed_fields;
	unsigned short changed_fields2;
	struct c_generic_context *g_context;

	g_context = (struct c_generic_context *) context->specific;
	changed_fields = g_context->tmp_variables.changed_fields;
	changed_fields2 = g_context->tmp_variables.changed_fields2;

	nb_fields = changed_static_one_hdr(changed_fields,
	                                   &g_context->ip_flags,
	                                   ip, context);

	if(g_context->tmp_variables.nr_of_ip_hdr > 1)
		nb_fields += changed_static_one_hdr(changed_fields2,
		                                    &g_context->ip2_flags,
		                                    ip2, context);

	return nb_fields;
}


/**
 * @brief Check if the static part of the context changed in the new IP packet.
 *
 * @param changed_fields The fields that changed, created by the function
 *                       changed_fields
 * @param header_info    The header info stored in the profile
 * @param ip             The header of the new IP packet
 * @param context        The compression context
 * @return               The number of fields that changed
 */
int changed_static_one_hdr(unsigned short changed_fields,
                           struct ip_header_info *header_info,
                           const struct iphdr *ip,
                           struct c_context *context)
{
	int nb_fields = 0; /* number of fields that changed */
	struct c_generic_context *g_context;

	g_context = (struct c_generic_context *) context->specific;

	if(is_changed(changed_fields, MOD_PROTOCOL) ||
	   header_info->protocol_count < MAX_FO_COUNT)
	{
		rohc_debugf(2, "protocol_count %d\n", header_info->protocol_count);
		
		if(is_changed(changed_fields, MOD_PROTOCOL))
		{
			header_info->protocol_count = 0;
			g_context->fo_count = 0;
		}
		nb_fields += 1;
	}

	return nb_fields;
}


/**
 * @brief Check if the dynamic parts of the context changed in any of the two
 *        IP headers.
 *
 * @param context The compression context
 * @param ip      The outer IP header
 * @param ip2     The inner IP header
 * @return        The number of fields that changed
 */
int changed_dynamic_both_hdr(struct c_context *context,
                             const struct iphdr *ip,
                             const struct iphdr *ip2)
{
	int nb_fields = 0; /* number of fields that changed */
	unsigned short changed_fields;
	unsigned short changed_fields2;
	struct c_generic_context *g_context;

	g_context = (struct c_generic_context *) context->specific;
	changed_fields = g_context->tmp_variables.changed_fields;
	changed_fields2 = g_context->tmp_variables.changed_fields2;

	rohc_debugf(3, "check for changed fields in the outer IP header\n");
	nb_fields = changed_dynamic_one_hdr(changed_fields, &g_context->ip_flags,
	                                    ip, context);

	if(g_context->tmp_variables.nr_of_ip_hdr > 1)
	{
		rohc_debugf(3, "check for changed fields in the inner IP header\n");
		nb_fields += changed_dynamic_one_hdr(changed_fields2,
		                                     &g_context->ip2_flags,
		                                     ip2, context);
	}

	return nb_fields;
}


/**
 * @brief Check if the dynamic part of the context changed in the IP packet.
 *
 * @param changed_fields The fields that changed, created by the function
 *                       changed_fields
 * @param header_info    The header info stored in the profile
 * @param ip             The header of the new IP packet
 * @param context        The compression context
 * @return               The number of fields that changed
 */
int changed_dynamic_one_hdr(unsigned short changed_fields,
                            struct ip_header_info *header_info,
                            const struct iphdr *ip,
                            struct c_context *context)
{
	int nb_fields = 0; /* number of fields that changed */
	int nb_flags = 0; /* number of flags that changed */
	struct c_generic_context *g_context;

	g_context = (struct c_generic_context *) context->specific;

	if(is_changed(changed_fields, MOD_TOS) ||
	   header_info->tos_count < MAX_FO_COUNT)
	{
		if(is_changed(changed_fields, MOD_TOS))
		{
			header_info->tos_count = 0;
			g_context->fo_count = 0;
		}
		nb_fields += 1;
	}

	if(is_changed(changed_fields, MOD_TTL) ||
	   header_info->ttl_count < MAX_FO_COUNT)
	{
		if(is_changed(changed_fields, MOD_TTL))
		{
			header_info->ttl_count = 0;
			g_context->fo_count = 0;
		}
		nb_fields += 1;
	}

	if(GET_DF(ip->frag_off) != GET_DF(header_info->old_ip.frag_off) ||
	   header_info->df_count < MAX_FO_COUNT)
	{
		if(GET_DF(ip->frag_off) != GET_DF(header_info->old_ip.frag_off))
		{
			header_info->ttl_count = 0;
			g_context->fo_count = 0;
		}
		nb_fields += 1;
	}

	if(header_info->rnd != header_info->old_rnd ||
	   header_info->rnd_count < MAX_FO_COUNT)
	{
		if(header_info->rnd != header_info->old_rnd)
		{
			rohc_debugf(1, "RND changed (%x -> %x), reset counter\n",
			            header_info->old_rnd, header_info->rnd);
			header_info->rnd_count = 0;
			g_context->fo_count = 0;
		}
		nb_flags += 1;
	}

	if(header_info->nbo != header_info->old_nbo ||
	   header_info->nbo_count < MAX_FO_COUNT)
	{
		if(header_info->nbo != header_info->old_nbo)
		{
			rohc_debugf(1, "NBO changed (%x -> %x), reset counter\n",
			            header_info->old_nbo, header_info->nbo);
			header_info->nbo_count = 0;
			g_context->fo_count = 0;
		}
		nb_flags += 1;
	}

	if(nb_flags > 0)
		nb_fields += 1;

	return nb_fields;
}


/**
 * @brief Find the IP fields that changed between the profile and a new
 *        IP packet.
 *
 * @param header_info    The header info stored in the profile
 * @param ip             The header of the new IP packet
 * @return               The bitpattern that indicates which field changed
 */
unsigned short changed_fields(struct ip_header_info *header_info,
                              const struct iphdr *ip)
{
	unsigned short ret_value = 0;

	if(header_info->old_ip.tos != ip->tos)
		ret_value |= MOD_TOS;
	if(header_info->old_ip.tot_len != ip->tot_len)
		ret_value |= MOD_TOT_LEN;
	if(header_info->old_ip.id != ip->id)
		ret_value |= MOD_ID;
	if(header_info->old_ip.frag_off != ip->frag_off)
		ret_value |= MOD_FRAG_OFF;
	if(header_info->old_ip.ttl != ip->ttl)
		ret_value |= MOD_TTL;
	if(header_info->old_ip.protocol != ip->protocol)
		ret_value |= MOD_PROTOCOL;
	if(header_info->old_ip.check != ip->check)
		ret_value |= MOD_CHECK;
	if(header_info->old_ip.saddr != ip->saddr)
		ret_value |= MOD_SADDR;
	if(header_info->old_ip.daddr != ip->daddr)
		ret_value |= MOD_DADDR;

	return ret_value;
}


/**
 * @brief Determine whether the IP Identification field of one IP header is
 *        random and/or in Network Bit Order (NBO).
 *
 * @param header_info  The header info stored in the profile
 * @param ip           One IP header
 */
void check_ip_identification(struct ip_header_info *header_info,
                             const struct iphdr *ip)
{
	int old_id, new_id;
	int nbo = -1;

	old_id = ntohs(header_info->old_ip.id);
	new_id = ntohs(ip->id);

	rohc_debugf(2, "1) old_id = 0x%04x new_id = 0x%04x\n", old_id, new_id);

	if((new_id - old_id) < IPID_MAX_DELTA && (new_id - old_id) > 0)
		nbo = 1;
	else if((old_id + IPID_MAX_DELTA) > 0xffff &&
	        new_id < ((old_id + IPID_MAX_DELTA) & 0xffff))
		nbo = 1;

	if(nbo == -1)
	{
		/* change byte ordering and check nbo = 0 */
		old_id = (old_id >> 8) | ((old_id << 8) & 0xff00);
		new_id = (new_id >> 8) | ((new_id << 8) & 0xff00);

		rohc_debugf(2, "2) old_id = 0x%04x new_id = 0x%04x\n", old_id, new_id);

		if((new_id - old_id) < IPID_MAX_DELTA && (new_id - old_id) > 0)
			nbo = 0;
		else if((old_id + IPID_MAX_DELTA) > 0xffff &&
		        new_id < ((old_id + IPID_MAX_DELTA) & 0xffff))
			nbo = 0;
	}

	if(nbo == -1)
	{
		rohc_debugf(2, "RND detected\n");
		header_info->rnd = 1;
	}
	else
	{
		rohc_debugf(2, "NBO = %d\n", nbo);
		header_info->rnd = 0;
		header_info->nbo = nbo;
	}
}

