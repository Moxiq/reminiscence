#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>


const int FPS = 60;

int main(void)
{

    avdevice_register_all();
    AVFormatContext *pInputFormatContext = avformat_alloc_context();
    const AVInputFormat *pAVInputFormat = av_find_input_format("x11grab");
    AVCodecContext *pAVCodexContext;
    assert(pInputFormatContext != NULL);
    assert(avformat_open_input(&pInputFormatContext, ":0.0+0,0", pAVInputFormat, NULL) == 0 && "Could not open input stream");


    AVDictionary *options = NULL;
    char fps_string[3];
    snprintf(fps_string, sizeof(fps_string), "%d", FPS);

    // Set fps
    assert(av_dict_set(&options, "framerate", fps_string, 0) >= 0);
    // Set preset
    assert(av_dict_set(&options, "preset", "medium", 0) >= 0);

    int video_index = -1;
    for (int i = 0; i < pInputFormatContext->nb_streams; i++)
    {
        if (pInputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_index = i;
            break;
        }
    }

    assert(video_index >= 0 && "video_index was less than 0");


    const AVCodec *pAVCodec = avcodec_find_decoder(pInputFormatContext->streams[video_index]->codecpar->codec_id);
    assert(pAVCodec != NULL && "pAVCodec was NULL");

    AVCodecContext *pCodecContext = avcodec_alloc_context3(pAVCodec);
    assert(avcodec_parameters_to_context(pCodecContext, pInputFormatContext->streams[video_index]->codecpar) >= 0 && "Failed to create AVCodecContext");  


    assert(avcodec_open2(pCodecContext, pAVCodec, NULL) == 0);


    const AVCodec *pCodecEnc = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
    assert(pCodecEnc != NULL);

    // allocate codec context
    AVCodecContext *pCodecCtxEnc = avcodec_alloc_context3(pCodecEnc);
    assert(pCodecCtxEnc != NULL && "pCodecCtxEnc pointer is null!");

    // put sample parameters
    pCodecCtxEnc->bit_rate = 400000;
    pCodecCtxEnc->width = 1400;
    pCodecCtxEnc->height = 1050;
    pCodecCtxEnc->time_base= (AVRational){1,25};
    pCodecCtxEnc->gop_size = 10;
    pCodecCtxEnc->pix_fmt = AV_PIX_FMT_YUV420P;
    pCodecCtxEnc->max_b_frames = 2;
	pCodecCtxEnc->time_base.num = 1;
	pCodecCtxEnc->time_base.den = 30; // 15fps

    assert(avcodec_open2(pCodecCtxEnc, pCodecEnc, NULL) >= 0);


    // allocate video frame
    AVFrame *pFrame = av_frame_alloc();
    assert(pFrame != NULL);

    // allocate an AVFrame structure
    AVFrame *pFrameOut = av_frame_alloc();
    assert(pFrameOut != NULL);

    const int ALIGN = 32;

    int nbytes = av_image_get_buffer_size(pCodecCtxEnc->pix_fmt, pCodecCtxEnc->width, pCodecCtxEnc->height, ALIGN); // Internet said 32
    uint8_t* outbuffer = (uint8_t*)av_malloc(nbytes);
    assert(outbuffer != NULL);

    int nbytes2 = av_image_fill_arrays(pFrameOut->data, pFrameOut->linesize, outbuffer, pCodecCtxEnc->pix_fmt, pCodecCtxEnc->width, pCodecCtxEnc->height, ALIGN);
    assert(nbytes == nbytes2);

    size_t i = 0;
    const size_t frame_limit = 100;
    AVPacket *pAVPacketOut = av_packet_alloc();
    while (av_read_frame(pInputFormatContext, pAVPacketOut) >= 0 && i++ < frame_limit)
    {
        if (pAVPacketOut->stream_index != video_index) continue;

        char str[64];

        int err = avcodec_send_packet(pCodecCtxEnc, pAVPacketOut);
        assert(err == 0);

        while (1)
        {
            // There can be multiple frames in each packet? for video usually one?
            err = avcodec_receive_frame(pCodecCtxEnc, pFrameOut);
            if (err == AVERROR_EOF) break;
            assert(err > 0);

        }
        


        printf("Frame Counter: %zu/%zu\r", i, frame_limit); fflush(stdout);
        av_packet_unref(pAVPacketOut);
    }


    av_dict_free(&options);
    avformat_free_context(pInputFormatContext);
    avcodec_free_context(&pCodecCtxEnc);
    avcodec_free_context(&pCodecContext);
    av_frame_free(&pFrame);
    av_frame_free(&pFrameOut);
    av_free(outbuffer);
    
    
    return 0;
}

// int main(void)
// {

//     // https://github.com/leandromoreira/ffmpeg-libav-tutorial#video---what-you-see
//     // https://github.com/abdullahfarwees/screen-recorder-ffmpeg-cpp/blob/master/src/ScreenRecorder.cpp

//     AVFormatContext *pFormatContext = avformat_alloc_context();
//     avformat_open_input(&pFormatContext, "luddegragas.mp4", NULL, NULL);


    
//     AVPacket *pkt = av_packet_alloc();
//     av_new_packet(pkt, 200000);

//     size_t vframe = 0;
//     size_t aframe = 0;
//     while (av_read_frame(pFormatContext, pkt) == 0)
//     {
//         AVStream *stream = pFormatContext->streams[pkt->stream_index];
//         switch (stream->codecpar->codec_type)
//         {
//             case AVMEDIA_TYPE_VIDEO:
//                 printf("video fsize: %zu\n", pkt->buf->size);
//                 printf("video dur: %ld\n", pkt->duration);
//                 vframe++;
//                 break;
//             case AVMEDIA_TYPE_AUDIO:
//                 printf("audio fsize: %zu\n", pkt->buf->size);
//                 aframe++;
//                 break;
                
//         }
//         av_packet_unref(pkt);
//     }

//     printf("vframe=%zu\n", vframe);
//     printf("aframe=%zu\n", aframe);


//     av_packet_free(&pkt);


    

//     return 0;
// }

