/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Waves Audio Ltd. All rights reserved.
 *
 * Author: Oleksandr Strelchenko <oleksandr.strelchenko@waves.com>
 */

#ifndef __WAVES_RENDER_H__
#define __WAVES_RENDER_H__

#include <stdint.h>

#include "MaxxEffect/MaxxEffect.h"
#include "MaxxEffect/MaxxStream.h"
#include "MaxxEffect/MaxxStatus.h"
#include "MaxxEffect/Initialize/MaxxEffect_Initialize.h"
#include "MaxxEffect/Process/MaxxEffect_Process.h"
#include "MaxxEffect/Process/MaxxEffect_Reset.h"
#include "MaxxEffect/Control/RPC/MaxxEffect_RPC_Server.h"


enum wr_state {
	WR_DISABLED,
	WR_INITIALIZED,
	WR_PREPARED,
	WR_RUNNIN
};

struct wr_config {
	// for simplicity left the same as codec adapter struct ca_config
	// to be changed lated if needed
	uint32_t                codec_id;
	uint32_t                reserved;
	uint32_t                sample_rate;
	uint32_t                sample_width;
	uint32_t                channels;
};

struct wr_data {
	struct wr_config        config;
	enum wr_state           state;
	struct comp_buffer      *sink;
	struct comp_buffer      *source;
	uint32_t                sample_rate;
	uint64_t                reserved;
	MaxxEffect_t            *effect;
	uint32_t                effect_size;
	MaxxStreamFormat_t      i_format;
	MaxxStreamFormat_t      o_format;
	MaxxStream_t            i_stream;
	MaxxStream_t            o_stream;
};

#endif//__WAVES_RENDER_H__
