// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Waves Audio Ltd. All rights reserved.
//
// Author: Oleksandr Strelchenko <oleksandr.strelchenko@waves.com>
//
#include "sof/audio/codec_adapter/codec/generic.h"
#include "sof/audio/codec_adapter/codec/waves.h"

#include "MaxxEffect/MaxxEffect.h"
#include "MaxxEffect/MaxxStream.h"
#include "MaxxEffect/MaxxStatus.h"
#include "MaxxEffect/Initialize/MaxxEffect_Initialize.h"
#include "MaxxEffect/Process/MaxxEffect_Process.h"
#include "MaxxEffect/Process/MaxxEffect_Reset.h"
#include "MaxxEffect/Control/RPC/MaxxEffect_RPC_Server.h"

struct waves_codec_data {
	uint32_t                sample_rate; // [Hz]
	uint32_t                buffer_bytes; // bytes
	uint32_t                buffer_samples; // multichannel samples
	uint32_t                sample_size_in_bytes;
	uint64_t                reserved;

	MaxxEffect_t            *effect;
	uint32_t                effect_size;
	MaxxStreamFormat_t      i_format;
	MaxxStreamFormat_t      o_format;
	MaxxStream_t            i_stream;
	MaxxStream_t            o_stream;
	MaxxBuffer_t            i_buffer;
	MaxxBuffer_t            o_buffer;
	uint32_t                response_max_bytes;
	uint32_t                request_max_bytes;
	void                    *response;
};

static uint32_t sample_convert_format_to_bytes(MaxxBuffer_Format_t fmt)
{
	uint32_t res;

	switch (fmt) {
	case MAXX_BUFFER_FORMAT_Q1_15:
		res = 2;
		break;
	case MAXX_BUFFER_FORMAT_Q1_23:
		res = 3;
		break;
	case MAXX_BUFFER_FORMAT_Q9_23:
	case MAXX_BUFFER_FORMAT_Q1_31:
	case MAXX_BUFFER_FORMAT_FLOAT:
	case MAXX_BUFFER_FORMAT_Q5_27:
		res = 4;
		break;
	default:
		res = -1;
		break;
	}
	return res;
}

static MaxxBuffer_Format_t sample_format_convert_sof_to_me(enum sof_ipc_frame fmt)
{
	MaxxBuffer_Format_t res;

	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
		res = MAXX_BUFFER_FORMAT_Q1_15;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		res = MAXX_BUFFER_FORMAT_Q9_23;
		break;
	case SOF_IPC_FRAME_S32_LE:
		res = MAXX_BUFFER_FORMAT_Q1_31;
		break;
	case SOF_IPC_FRAME_FLOAT:
		res = MAXX_BUFFER_FORMAT_FLOAT;
		break;
	default:
		res = -1;
		break;
	}
	return res;
}

static MaxxBuffer_Layout_t buffer_format_convert_sof_to_me(uint32_t fmt)
{
	MaxxBuffer_Layout_t res;

	switch (fmt) {
	case SOF_IPC_BUFFER_INTERLEAVED:
		res = MAXX_BUFFER_LAYOUT_INTERLEAVED;
		break;
	case SOF_IPC_BUFFER_NONINTERLEAVED:
		res = MAXX_BUFFER_LAYOUT_DEINTERLEAVED;
		break;
	default:
		res = -1;
		break;
	}
	return res;
}

int waves_codec_init(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = NULL;
	MaxxStatus_t status = 0;

	comp_dbg(dev, "waves_codec_init() start");

	waves_codec = codec_allocate_memory(dev, sizeof(struct waves_codec_data), 0);
	if (!waves_codec) {
		comp_err(dev, "waves_codec_init() error: failed to allocate memory for struct waves_codec_data");
		return -ENOMEM;
	}
	codec->private = waves_codec;

	status = MaxxEffect_GetEffectSize(&waves_codec->effect_size);
	if (status) {
		comp_err(dev, "waves_codec_init() error: MaxxEffect_GetEffectSize() returned %d",
			 status);
		codec_free_memory(dev, waves_codec);
		goto out;
	}

	waves_codec->effect =
		(MaxxEffect_t *)codec_allocate_memory(dev, waves_codec->effect_size, 16);
	if (!waves_codec->effect) {
		comp_err(dev, "waves_codec_init() error: failed to allocate %d bytes effect",
			waves_codec->effect_size);
		codec_free_memory(dev, waves_codec);
		goto out;
	}

	comp_dbg(dev, "waves_codec_init(): allocated %d bytes for effect",
		waves_codec->effect_size);

	comp_dbg(dev, "waves_codec_init() done");
out:
	return status;
}

static void trace_array(const struct comp_dev *dev, const uint32_t *arr, uint32_t len)
{
	uint32_t i;

	for (i = 0; i < len; i++)
		comp_dbg(dev, "trace_array() data[%03d]:0x%08x", i, *(arr + i));
}

static int waves_codec_configure(struct comp_dev *dev, enum codec_cfg_type type)
{
	struct codec_config *cfg;
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;
	int ret = 0;
	uint32_t response_size = 0;
	MaxxStatus_t status;

	comp_dbg(dev, "waves_codec_configure() start");

	cfg = (type == CODEC_CFG_SETUP) ? &codec->s_cfg : &codec->r_cfg;

	if (!cfg->avail || !cfg->size) {
		comp_err(dev, "waves_codec_configure() error: no config for type %d, avail %d, size %d",
			type, (unsigned int)cfg->avail, (unsigned int)cfg->size);
		ret = -EIO;
		goto err;
	}

	comp_dbg(dev, "trace config");
	trace_array(dev, (const uint32_t *)cfg->data, cfg->size / 4);

	// at time of writing codec adapter does not support reading something from codec
	// no response is available

	status = MaxxEffect_Message(waves_codec->effect, cfg->data, cfg->size,
		waves_codec->response, &response_size);
	if (status) {
		comp_err(dev, "waves_codec_configure() error: MaxxEffect_Message() error %d:",
			status);
		ret = -EIO;
		goto err;
	}

	if (response_size) {
		comp_dbg(dev, "trace response");
		trace_array(dev, (const uint32_t *)waves_codec->response, response_size / 4);
	}

	comp_dbg(dev, "waves_codec_configure() done");

err:
	return ret;
}

int waves_codec_prepare(struct comp_dev *dev)
{
	int ret = 0;
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;
	struct comp_data *component = comp_get_drvdata(dev);

	const struct audio_stream *src_fmt = &component->ca_source->stream;
	const struct audio_stream *snk_fmt = &component->ca_sink->stream;

	MaxxStatus_t status;
	MaxxBuffer_Format_t sample_format;
	MaxxBuffer_Layout_t buffer_format;
	MaxxStreamFormat_t *i_formats[1] = { &waves_codec->i_format };
	MaxxStreamFormat_t *o_formats[1] = { &waves_codec->o_format };

	comp_dbg(dev, "waves_codec_prepare() start");

	// resampling not supported
	if (src_fmt->rate != snk_fmt->rate) {
		comp_err(dev, "waves_codec_prepare() error: sorce %d sink %d rate mismatch",
			src_fmt->rate, snk_fmt->rate);
		ret = -EIO;
		goto err;
	}

	// upmix/downmix not supported
	if (src_fmt->channels != snk_fmt->channels) {
		comp_err(dev, "waves_codec_prepare() error: sorce %d sink %d channels mismatch",
			src_fmt->channels, snk_fmt->channels);
		ret = -EIO;
		goto err;
	}

	// different frame format not supported
	if (src_fmt->frame_fmt != snk_fmt->frame_fmt) {
		comp_err(dev, "waves_codec_prepare() error: sorce %d sink %d sample format mismatch",
			src_fmt->frame_fmt, snk_fmt->frame_fmt);
		ret = -EIO;
		goto err;
	}

	// float samples are not supported
	if (src_fmt->frame_fmt == SOF_IPC_FRAME_FLOAT) {
		comp_err(dev, "waves_codec_prepare() error: float samples not supported");
		ret = -EIO;
		goto err;
	}

	// different interleaving is not supported
	if (component->ca_source->buffer_fmt != component->ca_sink->buffer_fmt) {
		comp_err(dev, "waves_codec_prepare() error: source %d sink %d buffer format mismatch");
		ret = -EIO;
		goto err;
	}

	// only interleaved is supported
	if (component->ca_source->buffer_fmt != SOF_IPC_BUFFER_INTERLEAVED) {
		comp_err(dev, "waves_codec_prepare() error: non interleaved buffer format not supported");
		ret = -EIO;
		goto err;
	}

	// check sof stream parameters matching ME requirements
	// for next releases (with different requirements) ME scenario mechanism should be used
	// for now requirements can be hardcoded

	// match sampling rate
	if ((src_fmt->rate != 48000) && (src_fmt->rate != 44100)) {
		comp_err(dev, "waves_codec_prepare() error: sof rate %d not supported",
			src_fmt->rate);
		ret = -EIO;
		goto err;
	}

	// match number of channels
	if (src_fmt->channels != 2) {
		comp_err(dev, "waves_codec_prepare() error: sof channels %d not supported",
			src_fmt->channels);
		ret = -EIO;
		goto err;
	}

	// match sample format
	sample_format = sample_format_convert_sof_to_me(src_fmt->frame_fmt);
	if (sample_format == -1) {
		comp_err(dev, "waves_codec_prepare() error: sof sample format %d not supported",
			src_fmt->frame_fmt);
		ret = -EIO;
		goto err;
	}

	// match buffer format
	buffer_format = buffer_format_convert_sof_to_me(component->ca_source->buffer_fmt);
	if (buffer_format == -1) {
		comp_err(dev, "waves_codec_prepare() error: sof buffer format %d not supported",
			component->ca_source->buffer_fmt);
		ret = -EIO;
		goto err;
	}

	waves_codec->i_buffer = 0;
	waves_codec->o_buffer = 0;
	waves_codec->i_format.sampleRate = src_fmt->rate;
	waves_codec->i_format.numChannels = src_fmt->channels;
	waves_codec->i_format.samplesFormat = sample_format;
	waves_codec->i_format.samplesLayout = buffer_format;
	waves_codec->o_format = waves_codec->i_format;
	waves_codec->sample_size_in_bytes = sample_convert_format_to_bytes(sample_format);
	waves_codec->buffer_samples = src_fmt->rate * 2 / 1000; // 2 ms io buffers
	waves_codec->buffer_bytes = waves_codec->buffer_samples * src_fmt->channels *
		waves_codec->sample_size_in_bytes;
	waves_codec->request_max_bytes = 0;
	waves_codec->response_max_bytes = 0;
	waves_codec->response = 0;

	status = MaxxEffect_Initialize(waves_codec->effect, i_formats, 1, o_formats, 1);

	if (status) {
		comp_err(dev, "waves_codec_prepare() error: MaxxEffect_Initialize() error %d",
			status);
		ret = -EIO;
		goto err;
	}

	{
		const char* revision = NULL;

		status = MaxxEffect_Revision_Get(waves_codec->effect, &revision, NULL);

		if (status) {
			comp_err(dev, "waves_codec_prepare() error: MaxxEffect_Revision_Get() error %d",
				status);
			ret = -EIO;
			goto err;
		}

		comp_info(dev, "[WAVES CE][MA]: %s", revision);
	}

	// allocate buffer for response
	// SOF does not support getting infor from codec adapter right now
	// response will be stored in internal buffer to be dumped into logs
	status = MaxxEffect_GetMessageMaxSize(waves_codec->effect,
		&waves_codec->request_max_bytes, &waves_codec->response_max_bytes);

	if (status) {
		comp_err(dev, "waves_codec_prepare() error: MaxxEffect_GetMessageMaxSize() error %d",
			status);
		ret = -EIO;
		goto err;
	}

	waves_codec->response = codec_allocate_memory(dev, waves_codec->response_max_bytes, 16);
	if (!waves_codec->response) {
		comp_err(dev, "waves_codec_prepare() error: allocate memory for response");
		ret = -EINVAL;
		goto err;
	}

	comp_dbg(dev, "waves_codec_prepare(): allocated %d bytes for response",
		waves_codec->response_max_bytes);

	if (!codec->s_cfg.avail && !codec->s_cfg.size) {
		comp_err(dev, "waves_codec_prepare() error %x: no setup configuration available!");
		ret = -EIO;
		goto err;
	} else if (!codec->s_cfg.avail) {
		comp_info(dev, "waves_codec_prepare(): no new setup config, using the old");
		codec->s_cfg.avail = true;
	}
	ret = waves_codec_configure(dev, CODEC_CFG_SETUP);
	if (ret) {
		comp_err(dev, "waves_codec_prepare() error %x: failed to apply setup config", ret);
		goto err;
	}

	codec->s_cfg.avail = false;

	// allocate memory for input/output buffers
	waves_codec->i_buffer = codec_allocate_memory(dev, waves_codec->buffer_bytes, 16);
	if (!waves_codec->i_buffer) {
		codec_free_memory(dev, waves_codec->response);
		comp_err(dev, "waves_codec_prepare() error: allocate memory for i_buffer");
		ret = -EINVAL;
		goto err;
	}

	waves_codec->o_buffer = codec_allocate_memory(dev, waves_codec->buffer_bytes, 16);
	if (!waves_codec->o_buffer) {
		codec_free_memory(dev, waves_codec->response);
		codec_free_memory(dev, waves_codec->i_buffer);
		comp_err(dev, "waves_codec_prepare() error: allocate memory for o_buffer");
		ret = -EINVAL;
		goto err;
	}

	codec->cpd.in_buff = waves_codec->i_buffer;
	codec->cpd.in_buff_size = waves_codec->buffer_bytes;
	codec->cpd.out_buff = waves_codec->o_buffer;
	codec->cpd.out_buff_size = waves_codec->buffer_bytes;

	comp_dbg(dev, "waves_codec_prepare() done");

	return 0;
err:
	return ret;
}

int waves_codec_process(struct comp_dev *dev)
{
	int ret;
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;

	comp_dbg(dev, "waves_codec_process() start");

	MaxxStream_t *i_streams[1] = { &waves_codec->i_stream };
	MaxxStream_t *o_streams[1] = { &waves_codec->o_stream };
	MaxxStatus_t status;
	uint32_t num_input_samples = waves_codec->buffer_samples;

	// here input buffer should always be filled up as requested
	// since noone updates it`s size except code in prepare
	// kinda odd, but this is how codec adapter operates
	// on the other hand there is available/produced counters in cpd, so we check them anyways

	if (codec->cpd.avail != waves_codec->buffer_bytes) {
		comp_warn(dev, "waves_codec_process(): input buffer %d is not full %d",
			codec->cpd.avail, waves_codec->buffer_bytes);
		num_input_samples = codec->cpd.avail /
			(waves_codec->sample_size_in_bytes * waves_codec->i_format.numChannels);
	}

	waves_codec->i_stream.buffersArray = &waves_codec->i_buffer;
	waves_codec->i_stream.numAvailableSamples = num_input_samples;
	waves_codec->i_stream.numProcessedSamples = 0;
	waves_codec->i_stream.maxNumSamples = waves_codec->buffer_samples;

	waves_codec->o_stream.buffersArray = &waves_codec->o_buffer;
	waves_codec->o_stream.numAvailableSamples = 0;
	waves_codec->o_stream.numProcessedSamples = 0;
	waves_codec->o_stream.maxNumSamples = waves_codec->buffer_samples;

	status = MaxxEffect_Process(waves_codec->effect, i_streams, o_streams);
	if (status) {
		ret = status;
		comp_err(dev, "waves_codec_process(): MaxxEffect_Process error %d", status);
		goto err;
	}

	codec->cpd.produced = waves_codec->o_stream.numAvailableSamples *
		waves_codec->o_format.numChannels * waves_codec->sample_size_in_bytes;

	comp_dbg(dev, "waves_codec_process() done");

	return 0;
err:
	return ret;
}

int waves_codec_apply_config(struct comp_dev *dev)
{
	return waves_codec_configure(dev, CODEC_CFG_RUNTIME);
}

int waves_codec_reset(struct comp_dev *dev)
{
	MaxxStatus_t status;
	int ret = 0;
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;

	comp_dbg(dev, "waves_codec_reset() start");

	status = MaxxEffect_Reset(waves_codec->effect);
	if (status) {
		comp_err(dev, "waves_codec_reset(): MaxxEffect_Reset error %d", status);
		ret = status;
	}
	comp_dbg(dev, "waves_codec_reset() done");

	return ret;
}

int waves_codec_free(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;

	comp_dbg(dev, "waves_codec_free() start");

	if (waves_codec->effect)
		codec_free_memory(dev, waves_codec->effect);
	if (waves_codec->response)
		codec_free_memory(dev, waves_codec->response);
	if (waves_codec->i_buffer)
		codec_free_memory(dev, waves_codec->i_buffer);
	if (waves_codec->o_buffer)
		codec_free_memory(dev, waves_codec->o_buffer);

	comp_dbg(dev, "waves_codec_free() done");

	return 0;
}
