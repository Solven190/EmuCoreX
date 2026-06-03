// SPDX-License-Identifier: GPL-3.0+

#include "Host/AudioStream.h"

#include "common/Assertions.h"
#include "common/Error.h"

#include <aaudio/AAudio.h>
#include <android/log.h>

namespace
{
constexpr const char* LOG_TAG = "EmuCoreX";

class AndroidAAudioStream final : public AudioStream
{
public:
	AndroidAAudioStream(u32 sample_rate, const AudioStreamParameters& parameters)
		: AudioStream(sample_rate, parameters)
	{
	}

	~AndroidAAudioStream() override
	{
		CloseDevice();
	}

	void SetPaused(bool paused) override
	{
		if (m_paused == paused || !m_stream)
			return;

		const aaudio_result_t result = paused ? AAudioStream_requestPause(m_stream) : AAudioStream_requestStart(m_stream);
		if (result != AAUDIO_OK)
			__android_log_print(ANDROID_LOG_WARN, LOG_TAG, "AAudio pause/start failed: %s", AAudio_convertResultToText(result));
		m_paused = paused;
	}

	bool OpenDevice(bool stretch_enabled, Error* error)
	{
		pxAssert(!m_stream);

		static constexpr const std::array<SampleReader, static_cast<size_t>(AudioExpansionMode::Count)> sample_readers = {{
			&StereoSampleReaderImpl,
			&SampleReaderImpl<AudioExpansionMode::StereoLFE, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT, READ_CHANNEL_LFE>,
			&SampleReaderImpl<AudioExpansionMode::Quadraphonic, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
				READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>,
			&SampleReaderImpl<AudioExpansionMode::QuadraphonicLFE, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
				READ_CHANNEL_LFE, READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>,
			&SampleReaderImpl<AudioExpansionMode::Surround51, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
				READ_CHANNEL_FRONT_CENTER, READ_CHANNEL_LFE, READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>,
			&SampleReaderImpl<AudioExpansionMode::Surround71, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
				READ_CHANNEL_FRONT_CENTER, READ_CHANNEL_LFE, READ_CHANNEL_SIDE_LEFT, READ_CHANNEL_SIDE_RIGHT,
				READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>,
		}};

		AAudioStreamBuilder* builder = nullptr;
		aaudio_result_t result = AAudio_createStreamBuilder(&builder);
		if (result != AAUDIO_OK || !builder)
		{
			Error::SetStringFmt(error, "AAudio_createStreamBuilder() failed: {}", AAudio_convertResultToText(result));
			return false;
		}

		AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
		AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
		AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
		AAudioStreamBuilder_setSampleRate(builder, static_cast<int32_t>(m_sample_rate));
		AAudioStreamBuilder_setChannelCount(builder, static_cast<int32_t>(m_output_channels));
		AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
		const int32_t requested_buffer_frames =
			static_cast<int32_t>(AudioStream::GetBufferSizeForMS(m_sample_rate, m_parameters.buffer_ms));
		AAudioStreamBuilder_setBufferCapacityInFrames(builder, requested_buffer_frames);
		AAudioStreamBuilder_setDataCallback(builder, &AndroidAAudioStream::DataCallback, this);
		AAudioStreamBuilder_setErrorCallback(builder, &AndroidAAudioStream::ErrorCallback, this);

		result = AAudioStreamBuilder_openStream(builder, &m_stream);
		AAudioStreamBuilder_delete(builder);
		if (result != AAUDIO_OK || !m_stream)
		{
			Error::SetStringFmt(error, "AAudioStreamBuilder_openStream() failed: {}", AAudio_convertResultToText(result));
			m_stream = nullptr;
			return false;
		}

		BaseInitialize(sample_readers[static_cast<size_t>(m_parameters.expansion_mode)], stretch_enabled);

		const aaudio_result_t buffer_size_result = AAudioStream_setBufferSizeInFrames(m_stream, requested_buffer_frames);
		if (buffer_size_result < 0)
		{
			__android_log_print(ANDROID_LOG_WARN, LOG_TAG, "AAudio buffer resize to %d frames failed: %s",
				requested_buffer_frames, AAudio_convertResultToText(buffer_size_result));
		}

		result = AAudioStream_requestStart(m_stream);
		if (result != AAUDIO_OK)
		{
			Error::SetStringFmt(error, "AAudioStream_requestStart() failed: {}", AAudio_convertResultToText(result));
			CloseDevice();
			return false;
		}

		__android_log_print(ANDROID_LOG_INFO, LOG_TAG,
			"AAudio stream started rate=%u channels=%u buffer=%d/%d frames",
			m_sample_rate, static_cast<unsigned>(m_output_channels), AAudioStream_getBufferSizeInFrames(m_stream),
			AAudioStream_getBufferCapacityInFrames(m_stream));
		return true;
	}

private:
	void CloseDevice()
	{
		if (!m_stream)
			return;

		AAudioStream_requestStop(m_stream);
		AAudioStream_close(m_stream);
		m_stream = nullptr;
	}

	static aaudio_data_callback_result_t DataCallback(
		AAudioStream*, void* userdata, void* audio_data, int32_t num_frames)
	{
		AndroidAAudioStream* stream = static_cast<AndroidAAudioStream*>(userdata);
		if (!stream || stream->m_paused || num_frames <= 0)
			return AAUDIO_CALLBACK_RESULT_CONTINUE;

		stream->ReadFrames(static_cast<SampleType*>(audio_data), static_cast<u32>(num_frames));
		return AAUDIO_CALLBACK_RESULT_CONTINUE;
	}

	static void ErrorCallback(AAudioStream*, void*, aaudio_result_t error)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "AAudio stream error: %s", AAudio_convertResultToText(error));
	}

	AAudioStream* m_stream = nullptr;
};
} // namespace

std::unique_ptr<AudioStream> AudioStream::CreateSDLAudioStream(u32 sample_rate, const AudioStreamParameters& parameters,
	bool stretch_enabled, Error* error)
{
	std::unique_ptr<AndroidAAudioStream> stream = std::make_unique<AndroidAAudioStream>(sample_rate, parameters);
	if (!stream->OpenDevice(stretch_enabled, error))
		stream.reset();
	return stream;
}
