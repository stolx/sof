// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Waves Audio Ltd. All rights reserved.
//
// Author: Oleksandr Strelchenko <oleksandr.strelchenko@waves.com>
//
#include "sof/audio/codec_adapter/codec/generic.h"
#include "sof/audio/codec_adapter/codec/waves.h"

#define MAX_CONFIG_SIZE_BYTES (8192) // this is SOF limitation

static int32_t sample_convert_format_to_bytes(MaxxBuffer_Format_t fmt)
{
	// converts MaxxBuffer_Format_t to number of bytes it requires
	// -1 if undefined format
	int32_t res;

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
	// converts enum sof_ipc_frame to MaxxBuffer_Format_t
	// -1 if conversion is unsuccessful
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
	// converts sof frame format to MaxxBuffer_Layout_t
	// -1 if conversion is unsuccessful
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

static void trace_array(const struct comp_dev *dev, const uint32_t *arr, uint32_t len)
{
	// traces uint32_t array of len elements into debug messages
	uint32_t i;

	for (i = 0; i < len; i++)
		comp_dbg(dev, "trace_array() data[%03d]:0x%08x", i, *(arr + i));
}

static void waves_codec_data_clear(struct waves_codec_data *waves_codec)
{
	// initiallizes waves_codec_data to default values
	int i;
	uint8_t *byte = (uint8_t *) waves_codec;

	for (i = 0; i < sizeof(struct waves_codec_data); i++)
		*byte++ = 0;
}

static int waves_effect_check(struct comp_dev *dev)
{
	// checks if stream parameters fit MaxxEffect
	// returnes non-zero value in case something is not supported
	// can be extended to utilize MaxxEffect scenarios for future products
	struct comp_data *component = comp_get_drvdata(dev);
	const struct audio_stream *src_fmt = &component->ca_source->stream;
	const struct audio_stream *snk_fmt = &component->ca_sink->stream;

	comp_dbg(dev, "waves_effect_check() start");

	// resampling not supported
	if (src_fmt->rate != snk_fmt->rate) {
		comp_err(dev, "waves_effect_check() sorce %d sink %d rate mismatch",
			src_fmt->rate, snk_fmt->rate);
		return -EIO;
	}

	// upmix/downmix not supported
	if (src_fmt->channels != snk_fmt->channels) {
		comp_err(dev, "waves_effect_check() sorce %d sink %d channels mismatch",
			src_fmt->channels, snk_fmt->channels);
		return -EIO;
	}

	// different frame format not supported
	if (src_fmt->frame_fmt != snk_fmt->frame_fmt) {
		comp_err(dev, "waves_effect_check() sorce %d sink %d sample format mismatch",
			src_fmt->frame_fmt, snk_fmt->frame_fmt);
		return -EIO;
	}

	// float samples are not supported
	if (src_fmt->frame_fmt == SOF_IPC_FRAME_FLOAT) {
		comp_err(dev, "waves_effect_check() float samples not supported");
		return -EIO;
	}

	// different interleaving is not supported
	if (component->ca_source->buffer_fmt != component->ca_sink->buffer_fmt) {
		comp_err(dev, "waves_effect_check() source %d sink %d buffer format mismatch");
		return -EIO;
	}

	// only interleaved is supported
	if (component->ca_source->buffer_fmt != SOF_IPC_BUFFER_INTERLEAVED) {
		comp_err(dev, "waves_effect_check() non interleaved not supported");
		return -EIO;
	}

	// match sampling rate
	if ((src_fmt->rate != 48000) && (src_fmt->rate != 44100)) {
		comp_err(dev, "waves_effect_check() rate %d not supported",
			src_fmt->rate);
		return -EIO;
	}

	// match number of channels
	if (src_fmt->channels != 2) {
		comp_err(dev, "waves_effect_check() channels %d not supported",
			src_fmt->channels);
		return -EIO;
	}

	comp_dbg(dev, "waves_effect_check() done");
	return 0;
}

static int waves_effect_init(struct comp_dev *dev)
{
	// initializes MaxxEffect based on stream parameters
	// returnes non-zero value in case of error
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;
	struct comp_data *component = comp_get_drvdata(dev);

	const struct audio_stream *src_fmt = &component->ca_source->stream;

	MaxxStatus_t status;
	MaxxBuffer_Format_t sample_format;
	MaxxBuffer_Layout_t buffer_format;
	MaxxStreamFormat_t *i_formats[1] = { &waves_codec->i_format };
	MaxxStreamFormat_t *o_formats[1] = { &waves_codec->o_format };

	comp_dbg(dev, "waves_effect_init() start");

	sample_format = sample_format_convert_sof_to_me(src_fmt->frame_fmt);
	if (sample_format == -1) {
		comp_err(dev, "waves_effect_init() sof sample format %d not supported",
			src_fmt->frame_fmt);
		return -EIO;
	}

	buffer_format = buffer_format_convert_sof_to_me(component->ca_source->buffer_fmt);
	if (buffer_format == -1) {
		comp_err(dev, "waves_effect_init() sof buffer format %d not supported",
			component->ca_source->buffer_fmt);
		return -EIO;
	}

	waves_codec->request_max_bytes = 0;
	waves_codec->response_max_bytes = 0;
	waves_codec->response = 0;
	waves_codec->i_buffer = 0;
	waves_codec->o_buffer = 0;

	waves_codec->i_format.sampleRate = src_fmt->rate;
	waves_codec->i_format.numChannels = src_fmt->channels;
	waves_codec->i_format.samplesFormat = sample_format;
	waves_codec->i_format.samplesLayout = buffer_format;

	waves_codec->o_format = waves_codec->i_format;

	waves_codec->sample_size_in_bytes = (uint32_t)sample_convert_format_to_bytes(sample_format);
	waves_codec->buffer_samples = src_fmt->rate * 2 / 1000; // 2 ms io buffers
	waves_codec->buffer_bytes = waves_codec->buffer_samples * src_fmt->channels *
		waves_codec->sample_size_in_bytes;

	comp_info(dev, "waves_effect_init() rate %d, channels %d",
		waves_codec->i_format.sampleRate,
		waves_codec->i_format.numChannels);

	comp_info(dev, "waves_effect_init() format %d, layout %d, frame %d",
		waves_codec->i_format.samplesFormat,
		waves_codec->i_format.samplesLayout,
		waves_codec->buffer_samples);

	status = MaxxEffect_Initialize(waves_codec->effect, i_formats, 1, o_formats, 1);

	if (status) {
		comp_err(dev, "waves_effect_init() MaxxEffect_Initialize() error %d",
			status);
		return status;
	}

	comp_dbg(dev, "waves_effect_init() done");
	return 0;
}

static int waves_effect_revision(struct comp_dev *dev)
{
	// dump MaxxEffect revision into trace
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;
	const char *revision = NULL;
	MaxxStatus_t status;

	comp_dbg(dev, "waves_effect_revision() start");

	status = MaxxEffect_Revision_Get(waves_codec->effect, &revision, NULL);

	if (status) {
		comp_err(dev, "waves_effect_revision() MaxxEffect_Revision_Get() error %d",
			status);
		return -EIO;
	}

	// todo log binary revision
	comp_info(dev, "waves_effect_revision() %s", revision);

	comp_dbg(dev, "waves_effect_revision() done");
	return 0;
}

static int waves_effect_allocate(struct comp_dev *dev)
{
	// allocate additional buffers for MaxxEffect
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;
	MaxxStatus_t status;

	comp_dbg(dev, "waves_effect_allocate() start");

	// memmory for response
	// SOF does not support getting infor from codec adapter right now
	// response will be stored in internal buffer to be dumped into logs
	status = MaxxEffect_GetMessageMaxSize(waves_codec->effect,
		&waves_codec->request_max_bytes, &waves_codec->response_max_bytes);

	if (status) {
		comp_err(dev, "waves_effect_allocate() MaxxEffect_GetMessageMaxSize() error %d",
			status);
		return -EIO;
	}

	waves_codec->response = codec_allocate_memory(dev, waves_codec->response_max_bytes, 16);
	if (!waves_codec->response) {
		comp_err(dev, "waves_effect_allocate() memory for response");
		return -ENOMEM;
	}

	// memory for input/output buffers
	waves_codec->i_buffer = codec_allocate_memory(dev, waves_codec->buffer_bytes, 16);
	if (!waves_codec->i_buffer) {
		codec_free_memory(dev, waves_codec->response);
		comp_err(dev, "waves_effect_allocate() allocate memory for i_buffer");
		return -ENOMEM;
	}

	waves_codec->o_buffer = codec_allocate_memory(dev, waves_codec->buffer_bytes, 16);
	if (!waves_codec->o_buffer) {
		codec_free_memory(dev, waves_codec->response);
		codec_free_memory(dev, waves_codec->i_buffer);
		comp_err(dev, "waves_effect_allocate() allocate memory for o_buffer");
		return -ENOMEM;
	}

	comp_info(dev, "waves_effect_allocate() size response %d, i_buffer %d, o_buffer %d",
		waves_codec->response_max_bytes,
		waves_codec->buffer_bytes,
		waves_codec->buffer_bytes);

	comp_info(dev, "waves_effect_allocate() addr response %p, i_buffer %p, o_buffer %p",
		waves_codec->response,
		waves_codec->i_buffer,
		waves_codec->o_buffer);

	codec->cpd.in_buff = waves_codec->i_buffer;
	codec->cpd.in_buff_size = waves_codec->buffer_bytes;
	codec->cpd.out_buff = waves_codec->o_buffer;
	codec->cpd.out_buff_size = waves_codec->buffer_bytes;

	comp_dbg(dev, "waves_effect_allocate() done");
	return 0;
}

static int waves_effect_config(struct comp_dev *dev, enum codec_cfg_type type)
{
	struct codec_config *cfg;
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;
	struct codec_param *param;
	uint32_t index;
	uint32_t param_number = 0;

	comp_info(dev, "waves_codec_configure() start type %d", type);

	cfg = (type == CODEC_CFG_SETUP) ? &codec->s_cfg : &codec->r_cfg;

	comp_info(dev, "waves_codec_configure() config %p, size %d, avail %d",
		cfg->data, cfg->size, cfg->avail);

	if (!cfg->avail || !cfg->size) {
		comp_err(dev, "waves_codec_configure() no config for type %d, avail %d, size %d",
			type, (unsigned int)cfg->avail, (unsigned int)cfg->size);
		return -EIO;
	}

	if (cfg->size > MAX_CONFIG_SIZE_BYTES) {
		comp_err(dev, "waves_codec_configure() size is too big %d", cfg->size);
		return -EIO;
	}

	// incoming data in cfg->data is arranged according to struct codec_param
	// there migh be more than one struct codec_param inside cfg->data, glued back to back
	for (index = 0; index < cfg->size; param_number++) {
		uint32_t response_size = 0;
		uint32_t param_data_size;
		MaxxStatus_t status;

		param = (struct codec_param *)((char *)cfg->data + index);
		param_data_size = param->size - sizeof(param->size) - sizeof(param->id);

		comp_info(dev, "waves_codec_configure() PARAM %02d T %02d L %04d V",
			param_number, param->id, param->size);

		if (param->size > MAX_CONFIG_SIZE_BYTES) {
			comp_err(dev, "waves_codec_configure() codec param size too big %d",
				param->size);
			return -EIO;
		}

		trace_array(dev, (const uint32_t *)param->data, param_data_size / sizeof(uint32_t));

		status = MaxxEffect_Message(waves_codec->effect, param->data, param_data_size,
			waves_codec->response, &response_size);

		if (status) {
			comp_err(dev, "waves_codec_configure() MaxxEffect_Message() error %d",
				status);
			return -EIO;
		}

		// at time of writing codec adapter does not support getting something from codec
		// so response is stored to internal structure and dumped into trace messages
		if (response_size) {
			comp_dbg(dev, "waves_codec_configure() trace response");
			trace_array(dev, (const uint32_t *)waves_codec->response,
				response_size / sizeof(uint32_t));
		}

		index += param->size;
	}

	comp_dbg(dev, "waves_codec_configure() done");
	return 0;
}

static int waves_effect_setup_config(struct comp_dev *dev)
{
	// apply setup config if present
	struct codec_data *codec = comp_get_codec(dev);
	int ret = 0;

	comp_dbg(dev, "waves_effect_setup_config() start");

	if (!codec->s_cfg.avail && !codec->s_cfg.size) {
		comp_err(dev, "waves_effect_startup_config() no setup config");
		return 0;
	}

	if (!codec->s_cfg.avail) {
		comp_warn(dev, "waves_effect_startup_config() no new setup config, using old");
		codec->s_cfg.avail = true;
	}

	ret = waves_effect_config(dev, CODEC_CFG_SETUP);
	codec->s_cfg.avail = false;

	comp_dbg(dev, "waves_effect_setup_config() done");
	return ret;
}

int waves_codec_init(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = NULL;
	MaxxStatus_t status = 0;
	int ret = 0;

	comp_dbg(dev, "waves_codec_init() start");

	waves_codec = codec_allocate_memory(dev, sizeof(struct waves_codec_data), 16);
	if (!waves_codec) {
		comp_err(dev, "waves_codec_init() allocate memory for waves_codec_data");
		ret = -ENOMEM;
		goto err;
	}

	waves_codec_data_clear(waves_codec);
	codec->private = waves_codec;

	status = MaxxEffect_GetEffectSize(&waves_codec->effect_size);
	if (status) {
		comp_err(dev, "waves_codec_init() MaxxEffect_GetEffectSize() error %d",
			status);
		ret = status;
		codec_free_memory(dev, waves_codec);
		goto err;
	}

	waves_codec->effect = (MaxxEffect_t *)codec_allocate_memory(dev,
		waves_codec->effect_size, 16);

	if (!waves_codec->effect) {
		comp_err(dev, "waves_codec_init() allocate %d bytes for effect",
			waves_codec->effect_size);
		codec_free_memory(dev, waves_codec);
		ret = -ENOMEM;
		goto err;
	}

	comp_info(dev, "waves_codec_init() allocated %d bytes for effect",
		waves_codec->effect_size);

	comp_dbg(dev, "waves_codec_init() done");
	return 0;

err:
	comp_err(dev, "waves_codec_init() failed %d", ret);
	return ret;
}

int waves_codec_prepare(struct comp_dev *dev)
{
	int ret = 0;

	comp_dbg(dev, "waves_codec_prepare() start");

	ret = waves_effect_check(dev);
	if (ret)
		goto err;

	ret = waves_effect_init(dev);
	if (ret)
		goto err;

	ret = waves_effect_revision(dev);
	if (ret)
		goto err;

	ret = waves_effect_allocate(dev);
	if (ret)
		goto err;

	ret = waves_effect_setup_config(dev);
	if (ret)
		goto err;

	comp_dbg(dev, "waves_codec_prepare() done");
	return 0;

err:
	comp_err(dev, "waves_codec_prepare() failed %d", ret);
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
		comp_warn(dev, "waves_codec_process() input buffer %d is not full %d",
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
		comp_err(dev, "waves_codec_process() MaxxEffect_Process error %d", status);
		goto err;
	}

	codec->cpd.produced = waves_codec->o_stream.numAvailableSamples *
		waves_codec->o_format.numChannels * waves_codec->sample_size_in_bytes;

	comp_dbg(dev, "waves_codec_process() done");
	return 0;

err:
	comp_err(dev, "waves_codec_process() failed %d", ret);
	return ret;
}

int waves_codec_apply_config(struct comp_dev *dev)
{
	int ret;

	comp_dbg(dev, "waves_codec_apply_config() start");
	ret =  waves_effect_config(dev, CODEC_CFG_RUNTIME);
	if (ret)
		goto err;

	comp_dbg(dev, "waves_codec_apply_config() done");
	return 0;

err:
	comp_err(dev, "waves_codec_apply_config() failed %d", ret);
	return ret;
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
		comp_err(dev, "waves_codec_reset() MaxxEffect_Reset error %d", status);
		ret = status;
		goto err;
	}

	comp_dbg(dev, "waves_codec_reset() done");
	return ret;

err:
	comp_err(dev, "waves_codec_reset() failed %d", ret);
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
