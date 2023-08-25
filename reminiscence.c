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
#include <libswscale/swscale.h>

#define FPS "60"
#define FRAMES 100
#define WIDTH 1920
#define HEIGHT 1080

const char *output_name = "realout.mp4";

void fill_frame(AVFrame *frame)
{
    /* prepare a dummy image */
    /* Y */
    for(int y=0;y<HEIGHT;y++) {
        for(int x=0;x<WIDTH;x++) {
            frame->data[0][y * frame->linesize[0] + x] = x + y * 3;
        }
    }

/* Cb and Cr */
    for(int y=0;y<HEIGHT/2;y++) {
        for(int x=0;x<WIDTH/2;x++) {
            frame->data[1][y * frame->linesize[1] + x] = 128 + y * 2;
            frame->data[2][y * frame->linesize[2] + x] = 64 + x * 5;
        }
    }
}

int main(void)
{
    avdevice_register_all();
    av_log_set_level(AV_LOG_DEBUG);

    //Output stuff
    const AVOutputFormat *pAVOutputFormat = av_guess_format(NULL, output_name, NULL);
    assert(pAVOutputFormat != NULL);

    AVIOContext *pAVIOOutputContext;
    /** Open the output file to write to it. */
    int ret = avio_open(&pAVIOOutputContext, output_name, AVIO_FLAG_WRITE);
    assert(ret >= 0);

    AVFormatContext *pAVOutputFormatContext = avformat_alloc_context();
    assert(pAVOutputFormatContext != NULL);

    pAVOutputFormatContext->pb = pAVIOOutputContext;
    pAVOutputFormatContext->oformat = av_guess_format(NULL, output_name, NULL);
    assert(pAVOutputFormatContext->oformat != NULL);

    AVStream *pAVOutputStream = avformat_new_stream(pAVOutputFormatContext, NULL);
    assert(pAVOutputStream != NULL);

    const AVCodec *pCodecOutput = avcodec_find_encoder(AV_CODEC_ID_H264);
    assert(pCodecOutput != NULL);

    AVCodecContext *c = avcodec_alloc_context3(pCodecOutput);
    assert(pCodecOutput != NULL);

    /* put sample parameters */
    c->bit_rate = 400000;
    /* resolution must be a multiple of two */
    c->width = WIDTH;
    c->height = HEIGHT;
    /* frames per second */
    c->time_base= (AVRational){1,25},
    c->framerate = (AVRational){25, 1},
    c->gop_size = 10; /* emit one intra frame every ten frames */
    c->max_b_frames=1;
    c->pix_fmt = AV_PIX_FMT_YUV420P;

    ret = avcodec_parameters_from_context(pAVOutputStream->codecpar, c);
    assert(ret >= 0);

    ret = avcodec_open2(c, pCodecOutput, NULL);
    assert(ret == 0);

    ret = avformat_write_header(pAVOutputFormatContext, NULL);
    if (ret == AVSTREAM_INIT_IN_WRITE_HEADER || ret == AVSTREAM_INIT_IN_INIT_OUTPUT)
    {
        printf("Wrote header\n");
    }
    else
    {
        printf("Failed to write header\n");
        return 1;
    }


    int out_size, size, x, y, outbuf_size;
    AVFrame *picture;
    uint8_t *outbuf, *picture_buf;

    picture = av_frame_alloc();
    picture->width = c->width;
    picture->height = c->height;
    picture->format = c->pix_fmt;

    ret = av_frame_get_buffer(picture, 0);
    assert(ret == 0);

    /* alloc image and output buffer */

    // picture->data[0] = picture_buf;
    // picture->data[1] = picture->data[0] + size;
    // picture->data[2] = picture->data[1] + size / 4;
    // picture->linesize[0] = c->width;
    // picture->linesize[1] = c->width / 2;
    // picture->linesize[2] = c->width / 2;


    AVPacket *pAVPacketOut = av_packet_alloc();
    assert(pAVPacketOut != NULL);

    int frame_rate = 25; // Change this to your desired frame rate
    int64_t time_base_num = 1;
    int64_t time_base_den = frame_rate;

    for(int f = 0; f < FRAMES; f++) 
    {
        fill_frame(picture);
        picture->pts = f*6000; // Not sure how to set the fps here

        ret = avcodec_send_frame(c, picture);
        if (ret < 0)
        {
            printf("avcodec_send_frame: %d = %s\n", ret, av_err2str(ret));
            return 1;
        }

        printf("send_frame success!\n");


        while (1)
        {
            ret = avcodec_receive_packet(c, pAVPacketOut);

            // pAVPacketOut->pts = i;
            // pAVPacketOut->dts
            // pAVPacketOut->duration
            // https://ffmpeg.org/doxygen/trunk/structAVPacket.html#ab5793d8195cf4789dfb3913b7a693903
            // pAVPacketOut->pos = -1;

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0)
            {
                printf("avcodec_receive_packet: %d = %s\n", ret, av_err2str(ret));
                return 1;
            }

            ret = av_write_frame(pAVOutputFormatContext, pAVPacketOut);
            assert(ret == 0);
            printf("Wrote data\n");
        }
    }


        ret = av_write_trailer(pAVOutputFormatContext);
        assert(ret == 0);

        av_packet_unref(pAVPacketOut);
        av_frame_free(&picture);
        avcodec_free_context(&c);
        avformat_free_context(pAVOutputFormatContext);
    
    
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

