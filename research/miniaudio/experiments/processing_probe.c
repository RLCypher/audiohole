/* Isolated, original research harness. Does not open audio devices. */
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DEVICE_IO
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_ENGINE
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#include "../upstream/miniaudio/miniaudio.h"
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>

int main(void)
{
    ma_resampler r;
    ma_linear_resampler l;
    ma_resampler_config c = ma_resampler_config_init(ma_format_f32,1,48000,48000,ma_resample_algorithm_linear);
    ma_linear_resampler_config lc = ma_linear_resampler_config_init(ma_format_f32,1,48000,48000);
    const float ratios[] = {1.000100f,0.999900f,1.000001f};
    size_t i;
    printf("version=%s; pointer_bits=%zu\n",ma_version_string(),sizeof(void*)*8);
    assert(ma_resampler_init(&c,NULL,&r)==MA_SUCCESS);
    assert(ma_linear_resampler_init(&lc,NULL,&l)==MA_SUCCESS);
    for(i=0;i<sizeof(ratios)/sizeof(ratios[0]);++i) {
        assert(ma_resampler_set_rate_ratio(&r,ratios[i])==MA_SUCCESS);
        assert(ma_linear_resampler_set_rate_ratio(&l,ratios[i])==MA_SUCCESS);
        printf("requested=%.9f generic=%u/%u effective=%.9f linear=%u/%u effective=%.9f\n",
            ratios[i],r.sampleRateIn,r.sampleRateOut,(double)r.sampleRateIn/r.sampleRateOut,
            l.config.sampleRateIn,l.config.sampleRateOut,(double)l.config.sampleRateIn/l.config.sampleRateOut);
    }
    ma_resampler_uninit(&r,NULL);
    ma_linear_resampler_uninit(&l,NULL);

    /* Reach a fractional phase through public processing, then change rate. */
    lc=ma_linear_resampler_config_init(ma_format_f32,1,1000001,1000000);
    lc.lpfOrder=0;
    assert(ma_linear_resampler_init(&lc,NULL,&l)==MA_SUCCESS);
    {
        ma_uint64 in=10001,out=10000;
        float samples[10001]={0},output[10000];
        ma_uint32 oldFrac,oldDen,newDen,actual;
        ma_uint64 expected;
        assert(ma_linear_resampler_process_pcm_frames(&l,samples,&in,output,&out)==MA_SUCCESS);
        oldFrac=l.inTimeFrac; oldDen=l.config.sampleRateOut;
        assert(ma_linear_resampler_set_rate(&l,1000003,1000000)==MA_SUCCESS);
        newDen=l.config.sampleRateOut; actual=l.inTimeFrac;
        expected=(ma_uint64)oldFrac*newDen/oldDen;
        printf("phase_change old_frac=%u old_den=%u new_den=%u expected_u64=%" PRIu64 " actual=%u in=%" PRIu64 " out=%" PRIu64 "\n",
            oldFrac,oldDen,newDen,(uint64_t)expected,actual,(uint64_t)in,(uint64_t)out);
        assert(expected!=actual); /* Regression witness for this pinned revision. */
    }
    ma_linear_resampler_uninit(&l,NULL);
    {
        ma_data_converter cv;
        ma_data_converter_config cc=ma_data_converter_config_init(ma_format_f32,ma_format_f32,2,2,48000,48000);
        assert(ma_data_converter_init(&cc,NULL,&cv)==MA_SUCCESS);
        printf("static_equal_rate has_resampler=%u passthrough=%u set_rate_result=%d\n",cv.hasResampler,cv.isPassthrough,ma_data_converter_set_rate(&cv,48001,48000));
        assert(!cv.hasResampler && cv.isPassthrough);
        ma_data_converter_uninit(&cv,NULL);
        cc.allowDynamicSampleRate=MA_TRUE;
        assert(ma_data_converter_init(&cc,NULL,&cv)==MA_SUCCESS);
        printf("dynamic_equal_rate has_resampler=%u passthrough=%u set_rate_result=%d lpf_order=%u\n",cv.hasResampler,cv.isPassthrough,ma_data_converter_set_rate(&cv,48001,48000),cv.resampler.state.linear.config.lpfOrder);
        assert(cv.hasResampler && !cv.isPassthrough);
        ma_data_converter_uninit(&cv,NULL);
    }
    {
        ma_duplex_rb rb;
        assert(ma_duplex_rb_init(ma_format_f32,2,48000,48000,480,NULL,&rb)==MA_SUCCESS);
        printf("duplex_helper_equal capacity_frames=%u initial_readable_frames=%u\n",ma_pcm_rb_get_subbuffer_size(&rb.rb),ma_pcm_rb_available_read(&rb.rb));
        ma_duplex_rb_uninit(&rb);
        assert(ma_duplex_rb_init(ma_format_f32,2,48000,44100,441,NULL,&rb)==MA_SUCCESS);
        printf("duplex_helper_48k_client_44k1_capture capacity_frames=%u initial_readable_frames=%u\n",ma_pcm_rb_get_subbuffer_size(&rb.rb),ma_pcm_rb_available_read(&rb.rb));
        ma_duplex_rb_uninit(&rb);
    }
    {
        ma_channel_converter cv;
        const ma_channel mapIn[2]={MA_CHANNEL_FRONT_LEFT,MA_CHANNEL_FRONT_RIGHT};
        const ma_channel mapOut[2]={MA_CHANNEL_FRONT_RIGHT,MA_CHANNEL_FRONT_LEFT};
        ma_channel_converter_config cc=ma_channel_converter_config_init(ma_format_s24,2,mapIn,2,mapOut,ma_channel_mix_mode_simple);
        const unsigned char input[6]={1,2,3,4,5,6};
        unsigned char output[12];
        memset(output,0xAA,sizeof(output));
        assert(ma_channel_converter_init(&cc,NULL,&cv)==MA_SUCCESS);
        assert(ma_channel_converter_process_pcm_frames(&cv,output,input,1)==MA_SUCCESS);
        printf("s24_shuffle logical_output_bytes=6 guarded_storage_bytes=12 actual=");
        for(i=0;i<sizeof(output);++i) printf("%02x%s",output[i],i+1==sizeof(output)?"\n":" ");
        assert(output[8]!=0xAA); /* Witness: byte beyond logical six-byte output is changed. */
        ma_channel_converter_uninit(&cv,NULL);
    }
    puts("All source-behavior witnesses completed; no hardware measurements.");
    return 0;
}
