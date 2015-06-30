// Please note that this will only work on Unity 5.2 or higher.

#include "AudioPluginUtil.h"

extern float hrtfSrcData[];
extern float reverbmixbuffer[];

namespace Spatializer
{
	enum
	{
		P_DISTANCEATTN,
		P_FIXEDVOLUME,
		P_NUM
	};

	const int HRTFLEN = 512;
	
	const float GAINCORRECTION = 1.0f;

	class HRTFData
	{
		struct CircleCoeffs
		{
			int numangles;
			float* hrtf;
			float* angles;
		
			void GetHRTF(UnityComplexNumber* h, float angle, float mix)
			{
				int index1 = 0;
				while (index1 < numangles && angles[index1] < angle)
					index1++;
				if(index1 > 0)
					index1--;
				int index2 = (index1 + 1) % numangles;
				float* hrtf1 = hrtf + HRTFLEN * 4 * index1;
				float* hrtf2 = hrtf + HRTFLEN * 4 * index2;
				float f = (angle - angles[index1]) / (angles[index2] - angles[index1]);
				for(int n = 0; n < HRTFLEN * 2; n++)
				{
					h[n].re += (hrtf1[0] + (hrtf2[0] - hrtf1[0]) * f - h[n].re) * mix;
					h[n].im += (hrtf1[1] + (hrtf2[1] - hrtf1[1]) * f - h[n].im) * mix;
					hrtf1 += 2;
					hrtf2 += 2;
				}
			}
		};

	public:
		CircleCoeffs hrtfChannel[2][14];

	public:
		HRTFData()
		{
			float* p = hrtfSrcData;
			for(int c = 0; c < 2; c++)
			{
				for(int e = 0; e < 14; e++)
				{
					CircleCoeffs& coeffs = hrtfChannel[c][e];
					coeffs.numangles = (int)(*p++);
					coeffs.angles = p;
					p += coeffs.numangles;
					coeffs.hrtf = new float [coeffs.numangles * HRTFLEN * 4];
					float* dst = coeffs.hrtf;
					UnityComplexNumber h[HRTFLEN * 2];
					for(int a = 0; a < coeffs.numangles; a++)
					{
						memset(h, 0, sizeof(h));
						for(int n = 0; n < HRTFLEN; n++)
							h[n].re = p[HRTFLEN - 1 - n];
						p += HRTFLEN;
						FFT::Forward(h, HRTFLEN * 2);
						for(int n = 0; n < HRTFLEN * 2; n++)
						{
							*dst++ = h[n].re;
							*dst++ = h[n].im;
						}
					}
				}
			}
		}
	};

	static HRTFData sharedData;

	struct InstanceChannel
	{
		UnityComplexNumber h[HRTFLEN * 2];
		UnityComplexNumber x[HRTFLEN * 2];
		UnityComplexNumber y[HRTFLEN * 2];
		float buffer[HRTFLEN * 2];
	};

    struct EffectData
    {
        float p[P_NUM];
		InstanceChannel ch[2];
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        RegisterParameter(definition, "Distance Attn", "", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, P_DISTANCEATTN, "AudioSource distance attenuation");
		RegisterParameter(definition, "Fixed Volume", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_FIXEDVOLUME, "Fixed volume amount");
		definition.flags |= UnityAudioEffectDefinitionFlags_IsSpatializer;
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = new EffectData;
        memset(effectdata, 0, sizeof(EffectData));
        state->effectdata = effectdata;
        InitParametersFromDefinitions(InternalRegisterEffectDefinition, effectdata->p);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        delete data;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState* state, int index, float value)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        data->p[index] = value;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState* state, int index, float* value, char *valuestr)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        if (value != NULL)
            *value = data->p[index];
        if (valuestr != NULL)
            valuestr[0] = 0;
        return UNITY_AUDIODSP_OK;
    }

    int UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state, const char* name, float* buffer, int numsamples)
    {
        return UNITY_AUDIODSP_OK;
    }

	static void GetHRTF (int channel, UnityComplexNumber* h, float azimuth, float elevation)
	{
		float e = FastClip (elevation * 0.1f + 4, 0, 12);
		float f = floorf (e);
		int index1 = (int)f;
		if(index1 < 0)
			index1 = 0;
		else if(index1 > 12)
			index1 = 12;
		int index2 = index1 + 1;
		if(index2 > 12)
			index2 = 12;
		sharedData.hrtfChannel[channel][index1].GetHRTF (h, azimuth, 1.0f);
		sharedData.hrtfChannel[channel][index2].GetHRTF (h, azimuth, e - f);
	}
	
    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
		// Somewhat convoluted error checking here because hostapiversion is only supported from SDK version 1.03 and onwards.
		if(inchannels != 2 ||
		   outchannels != 2 ||
		   state->structsize < sizeof(UnityAudioEffectState) ||
		   state->hostapiversion < UNITY_AUDIO_PLUGIN_API_VERSION ||
		   state->spatializerdata == NULL)
		{
			memcpy(outbuffer, inbuffer, length * outchannels * sizeof(float));
	        return UNITY_AUDIODSP_OK;
		}

        EffectData* data = state->GetEffectData<EffectData>();

		static const float kRad2Deg = 180.0f / kPI;

		float* m = state->spatializerdata->listenermatrix;
		float* s = state->spatializerdata->sourcematrix;

		state->spatializerdata->distanceattenuationwrite = state->spatializerdata->distanceattenuationread * data->p[P_DISTANCEATTN] + data->p[P_FIXEDVOLUME];

		// Currently we ignore source orientation and only use the position
		float px = s[12];
		float py = s[13];
		float pz = s[14];

		float dir_x = m[ 0] * px + m[ 4] * py + m[ 8] * pz + m[12];
		float dir_y = m[ 1] * px + m[ 5] * py + m[ 9] * pz + m[13];
		float dir_z = m[ 2] * px + m[ 6] * py + m[10] * pz + m[14];

		float azimuth = (fabsf(dir_z) < 0.001f) ? 0.0f : atan2f(dir_x, dir_z);
		if (azimuth < 0.0f)
			azimuth += 2.0f * kPI;
		azimuth = FastClip(azimuth * kRad2Deg, 0.0f, 360.0f);
			
		float elevation = atan2f(dir_y, sqrtf(dir_x * dir_x + dir_z * dir_z) + 0.001f) * kRad2Deg;
		float spatialblend = state->spatializerdata->spatialblend;
		float reverbmix = state->spatializerdata->reverbzonemix;

		GetHRTF (0, data->ch[0].h, azimuth, elevation);
		GetHRTF (1, data->ch[1].h, azimuth, elevation);
		
		float* reverb = reverbmixbuffer;
		for(int sampleOffset = 0; sampleOffset < length; sampleOffset += HRTFLEN)
		{
			for(int c = 0; c < 2; c++)
			{
				InstanceChannel& ch = data->ch[c];

				for (int n = 0; n < HRTFLEN; n++)
				{
					ch.buffer[n          ] = ch.buffer[n + HRTFLEN];
					ch.buffer[n + HRTFLEN] = inbuffer[n * 2 + c];
				}

				for (int n = 0; n < HRTFLEN * 2; n++)
				{
					ch.x[n].re = ch.buffer[HRTFLEN * 2 - 1 - n];
					ch.x[n].im = 0.0f;
				}

				FFT::Forward (ch.x, HRTFLEN * 2);

				for (int n = 0; n < HRTFLEN * 2; n++)
					UnityComplexNumber::Mul(ch.x[n], ch.h[n], ch.y[n]);
				
				FFT::Backward (ch.y, HRTFLEN * 2);
				
				for (int n = 0; n < HRTFLEN; n++)
				{
					float s = inbuffer[n * 2 + c];
					float y = s + (ch.y[n].re * GAINCORRECTION - s) * spatialblend;
					outbuffer[n * 2 + c] = y;
					reverb[n * 2 + c] += y * reverbmix;
				}
			}

			inbuffer += HRTFLEN * 2;
			outbuffer += HRTFLEN * 2;
			reverb += HRTFLEN * 2;
		}

        return UNITY_AUDIODSP_OK;
    }
}
