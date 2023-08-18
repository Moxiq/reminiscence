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
const char *output_name = "realout.mp4";

int main(void)
{

    avdevice_register_all();

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
    c->width = 352;
    c->height = 288;
    /* frames per second */
    c->time_base= (AVRational){1,25},
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
    picture->width = 352;
    picture->height = 288;
    picture->format = AV_PIX_FMT_YUV420P;

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

    /* encode 1 second of video */
    for(int i=0;i<25;i++) {
        fflush(stdout);
        /* prepare a dummy image */
        /* Y */
        for(y=0;y<c->height;y++) {
            for(x=0;x<c->width;x++) {
                picture->data[0][y * picture->linesize[0] + x] = x + y + i * 3;
            }
        }

    /* Cb and Cr */
        for(y=0;y<c->height/2;y++) {
            for(x=0;x<c->width/2;x++) {
                picture->data[1][y * picture->linesize[1] + x] = 128 + y + i * 2;
                picture->data[2][y * picture->linesize[2] + x] = 64 + x + i * 5;
            }
        }

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
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0)
            {
                printf("avcodec_receive_packet: %d = %s\n", ret, av_err2str(ret));
                return 1;
            }

            ret = av_interleaved_write_frame(pAVOutputFormatContext, pAVPacketOut);
            assert(ret == 0);
            printf("Wrote data\n");
        }
    }

        ret = av_write_trailer(pAVOutputFormatContext);
        assert(ret == 0);

        av_packet_unref(pAVPacketOut);
        av_frame_free(&picture);
        avcodec_free_context(&c);

    // Send raw data to the output encoder

    // Receive the encoded data from the encoder
    // ret = avcodec_receive_packet(pCodecContextOut, pAVPacketOut);
    // assert(ret == 0);

    // Write the data to output file
    
    
    // while (av_read_frame(pFormatContextIn, pAVPacket) >= 0 && i++ < frame_limit)
    // {
    //     if (pAVPacket->stream_index != video_index)
    //     {
    //         av_packet_unref(pAVPacket);
    //         continue;
    //     }
    //         assert(avcodec_send_packet(pCodecContextIn, pAVPacket) == 0);
                
    //         printf("Packet Counter: %zu/%zu\n", i, frame_limit); fflush(stdout);

    //         size_t ii = 0;
    //         while (avcodec_receive_frame(pCodecContextIn, pFrame) == 0)
    //         {
    //             // Optional scaling here

    //             avcodec_send_frame(pCodecContextOut, pFrame);

    //             int err = avcodec_receive_packet(pCodecContextOut, pAVPacket);
    //             printf("err=%d: %s\n", err, av_err2str(err));

    //             // while (avcodec_receive_packet(pCodecContextOut, pAVPacket) == 0)
    //             // {
    //             //     printf("Wrote frame\n");
    //             //     assert(av_interleaved_write_frame(pFormatContextOut, pAVPacket) == 0);
    //             //     av_packet_unref(pAVPacket);
    //             // }


    //             printf("Frame Counter: %zu\n", ++ii); fflush(stdout);
    //         }

    //     av_packet_unref(pAVPacket);
    // }

    // err = av_write_trailer(pFormatContextOut);
    // if (err != 0)
    // {
    //     printf("Failed to write trailer: %s\n", av_err2str(err));
    // }


    // avformat_free_context(pFormatContextIn);
    // avformat_free_context(pAVOutputFormatContext);
    // // avcodec_free_context(&pCodecContextOut);
    // avcodec_free_context(&pCodecContextIn);
    // av_frame_free(&pFrame);
    
    
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

