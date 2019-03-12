#include "common/RnNoiseCommonPlugin.h"

#include <cstring>
#include <ios>
#include <limits>
#include <algorithm>

#include <rnnoise/rnnoise.h>

RnNoiseCommonPlugin::RnNoiseCommonPlugin() :
  m_errorStr   (NULL),
  m_initialized(false),
  m_resample   (false)
{
}

void RnNoiseCommonPlugin::setSampleRate(unsigned long sampleRate)
{
  m_downRatio = (double)k_denoiseSampleRate / (double)sampleRate;
  m_upRatio   = (double)sampleRate          / (double)k_denoiseSampleRate;
  m_resample  = sampleRate != 48000;
}

bool RnNoiseCommonPlugin::init() {
  int err;

  if (m_initialized)
    deinit();

  m_srcIn = std::shared_ptr<SRC_STATE>(
    src_new(SRC_SINC_BEST_QUALITY, 1, &err),
    [](SRC_STATE *st)
    {
      src_delete(st);
    }
  );

  if (err)
  {
    m_errorStr = src_strerror(err);
    return false;
  }

  m_srcOut = std::shared_ptr<SRC_STATE>(
    src_new(SRC_SINC_BEST_QUALITY, 1, &err),
    [](SRC_STATE *st)
    {
      src_delete(st);
    }
  );

  if (err)
  {
    m_srcIn.reset();
    m_errorStr = src_strerror(err);
    return false;
  }

  m_denoiseState = std::shared_ptr<DenoiseState>(
    rnnoise_create(),
    [](DenoiseState *st)
    {
      rnnoise_destroy(st);
    }
  );

  src_set_ratio(m_srcIn.get(), m_downRatio);
  src_set_ratio(m_srcOut  .get(), m_upRatio  );

  m_inBuffer .resize(k_denoiseFrameSize);
  m_outBuffer.resize(k_denoiseFrameSize * 2);
  m_outBufferR = 0;
  m_outBufferW = 0;
  m_outBufferA = 0;

  m_initialized = true;
  m_errorStr    = NULL;
  return true;
}

void RnNoiseCommonPlugin::deinit() {
  m_denoiseState.reset();
  m_srcIn       .reset();
  m_srcOut      .reset();
  m_initialized = false;
}

void RnNoiseCommonPlugin::process(const float *in, float *out, int32_t sampleFrames)
{
  const float mul = 1.0f / std::numeric_limits<short>::max();
  if (!sampleFrames)
    return;

  if (!m_initialized)
    init();

  SRC_DATA srcIn;
  srcIn.data_in       = in;
  srcIn.input_frames  = sampleFrames;
  srcIn.end_of_input  = 0;
  srcIn.src_ratio     = m_downRatio;
  srcIn.data_out      = &m_inBuffer[0];
  srcIn.output_frames = m_inBuffer.size();

  SRC_DATA srcOut;
  srcOut.data_out      = out;
  srcOut.output_frames = sampleFrames;
  srcOut.end_of_input  = 0;
  srcOut.src_ratio     = m_upRatio;

  long frames = 0;
  while(srcIn.input_frames)
  {
    if (m_resample)
    {
      // resample the samples and then scale them
      src_process(m_srcIn.get(), &srcIn);
      for(long i = 0; i < srcIn.output_frames_gen; ++i)
        m_inBuffer[i] *= std::numeric_limits<short>::max();
    }
    else
    {
      // just copy the data and scale it
      srcIn.input_frames_used = srcIn.input_frames;
      if (srcIn.input_frames_used > srcIn.output_frames)
        srcIn.input_frames_used = srcIn.output_frames;
      srcIn.output_frames_gen = srcIn.input_frames_used;

      for(long i = 0; i < srcIn.output_frames_gen; ++i)
        m_inBuffer[i] = in[i] * std::numeric_limits<short>::max();
    }

    srcIn.data_in      += srcIn.input_frames_used;
    srcIn.input_frames -= srcIn.input_frames_used;

    float *denoise_in = &m_inBuffer[0];
    while(srcIn.output_frames_gen)
    {
      const int wrote        = rnnoise_add_samples(m_denoiseState.get(), denoise_in, srcIn.output_frames_gen);
      denoise_in            += wrote;
      srcIn.output_frames_gen -= wrote;

      if (rnnoise_get_needed(m_denoiseState.get()) == 0)
      {
        rnnoise_process_frame(m_denoiseState.get(), &m_outBuffer[m_outBufferW]);

        // scale the levels back to normal
        for(int32_t i = 0; i < k_denoiseFrameSize; ++i)
          m_outBuffer[m_outBufferW + i] *= mul;

        m_outBufferW += k_denoiseFrameSize;
        m_outBufferA += k_denoiseFrameSize;
        if (m_outBufferW == m_outBuffer.size())
          m_outBufferW = 0;
      }

      // resample what we can to the output
      while(m_outBufferA && srcOut.output_frames)
      {
        srcOut.data_in       = &m_outBuffer[m_outBufferR];
        srcOut.input_frames  = m_outBufferW < m_outBufferR ? m_outBuffer.size() - m_outBufferR : m_outBufferW - m_outBufferR;

        if (m_resample)
          src_process(m_srcOut.get(), &srcOut);
        else
        {
          // simply copy the buffer if we are not resampling
          srcOut.input_frames_used = srcOut.input_frames;
          if (srcOut.input_frames_used > srcOut.output_frames)
            srcOut.input_frames_used = srcOut.output_frames;
          memcpy(srcOut.data_out, srcOut.data_in, srcOut.input_frames_used * sizeof(float));
        }

        if (!srcOut.input_frames_used && !srcOut.output_frames_gen)
          break;

        m_outBufferR         += srcOut.input_frames_used;
        m_outBufferA         -= srcOut.input_frames_used;

        srcOut.data_out      += srcOut.output_frames_gen;
        srcOut.output_frames -= srcOut.output_frames_gen;
        frames               += srcOut.output_frames_gen;

        if (m_outBufferR == m_outBuffer.size())
          m_outBufferR = 0;
      }
    }
  }

  // if we generated less frames then wanted, pad them across to the right
  if (frames && frames < sampleFrames)
  {
    const size_t pad = sampleFrames - frames;
    memmove(out + pad, out, frames);
    memset(out, 0, pad);
  }
}