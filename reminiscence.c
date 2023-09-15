#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#define FPS "60"
#define FRAMES 0 // set to 0 for continous recording
#define WIDTH 2560
#define HEIGHT 1440

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

// #define LOG_VERBOSE

void init_input_codec();
void init_output_codec();
int get_x11_frame(AVFrame *frame);
void int_handler();
void *timer_thread(void *args);
void clean_up();
void format_file_size(double sizeInBytes, char *res, size_t s_max);

const char *output_name = "realout.mp4";

// X11 input stuff
const AVInputFormat *pAVInputFormat;
AVFormatContext *pFormatContextIn;
const AVCodec *pAVCodec;
AVCodecContext *pCodecContextIn;
AVPacket *pAVPacketIn;
int video_index = -1;

void init_input_codec()
{
    int ret = 0;

    // Set input options
    AVDictionary *options = NULL;
    av_dict_set(&options, "video_size", "2560x1440", 0);
    av_dict_set(&options, "framerate", FPS, 0);
    av_dict_set(&options, "preset", "medium", 0);

    pAVInputFormat = av_find_input_format("x11grab");

    pFormatContextIn = avformat_alloc_context();
    assert(pFormatContextIn != NULL);

    ret = avformat_open_input(&pFormatContextIn, ":0.0+1920,0", pAVInputFormat, &options);
    assert(ret == 0 && "Could not open input stream");

    for (size_t i = 0; i < pFormatContextIn->nb_streams; i++)
    {
        if (pFormatContextIn->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_index = i;
            break;
        }
    }

    assert(video_index >= 0 && "video_index was less than 0");

    pAVCodec = avcodec_find_decoder(pFormatContextIn->streams[video_index]->codecpar->codec_id);
    assert(pAVCodec != NULL && "pAVCodec was NULL");

    pCodecContextIn = avcodec_alloc_context3(pAVCodec);
    assert(pCodecContextIn != NULL);

    ret = avcodec_parameters_to_context(pCodecContextIn, pFormatContextIn->streams[video_index]->codecpar);
    assert(ret >= 0 && "Failed to create AVCodecContext");

    ret = avcodec_open2(pCodecContextIn, pAVCodec, NULL);
    assert(ret == 0);
    av_dump_format(pFormatContextIn, video_index, ":0.0", 0);

    pAVPacketIn = av_packet_alloc();
    assert(pAVPacketIn != NULL);

}

const AVOutputFormat *pAVOutputFormat;
AVIOContext *pAVIOOutputContext;
AVFormatContext *pAVOutputFormatContext;
AVStream *pAVOutputStream;
const AVCodec *pCodecOutput;
AVCodecContext *pCodecContextOut;

void init_output_codec()
{
    //Output stuff
    pAVOutputFormat = av_guess_format(NULL, output_name, NULL);
    assert(pAVOutputFormat != NULL);

    /** Open the output file to write to it. */
    int ret = avio_open(&pAVIOOutputContext, output_name, AVIO_FLAG_WRITE);
    assert(ret >= 0);

    pAVOutputFormatContext = avformat_alloc_context();
    assert(pAVOutputFormatContext != NULL);

    pAVOutputFormatContext->pb = pAVIOOutputContext;
    pAVOutputFormatContext->oformat = av_guess_format(NULL, output_name, NULL);
    assert(pAVOutputFormatContext->oformat != NULL);

    pAVOutputStream = avformat_new_stream(pAVOutputFormatContext, NULL);
    assert(pAVOutputStream != NULL);

    pCodecOutput = avcodec_find_encoder(AV_CODEC_ID_H264);
    assert(pCodecOutput != NULL);

    pCodecContextOut = avcodec_alloc_context3(pCodecOutput);
    assert(pCodecOutput != NULL);

    /* put sample parameters */
    pCodecContextOut->bit_rate = 100000;
    /* resolution must be a multiple of two */
    pCodecContextOut->width = WIDTH;
    pCodecContextOut->height = HEIGHT;
    /* frames per second */
    pCodecContextOut->time_base= (AVRational){1,25},
    pCodecContextOut->framerate = (AVRational){25, 1},
    pCodecContextOut->gop_size = 10; /* emit one intra frame every ten frames */
    pCodecContextOut->max_b_frames=1;
    pCodecContextOut->pix_fmt = AV_PIX_FMT_YUV420P;

    ret = avcodec_parameters_from_context(pAVOutputStream->codecpar, pCodecContextOut);
    assert(ret >= 0);

    ret = avcodec_open2(pCodecContextOut, pCodecOutput, NULL);
    assert(ret == 0);
}

int get_x11_frame(AVFrame *frame)
{
    assert(pAVPacketIn != NULL);
    int ret = av_read_frame(pFormatContextIn, pAVPacketIn);
    assert(ret == 0);

    if (pAVPacketIn->stream_index != video_index)
        return 1;
    
    // Send the packet to the input decoder
    ret = avcodec_send_packet(pCodecContextIn, pAVPacketIn);
    if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
    {
        // We now have no frames left in the packet
        printf("avcodec_send_packet: %d\n", ret);
        return 1;
    }
    av_packet_unref(pAVPacketIn);

    return avcodec_receive_frame(pCodecContextIn, frame);
}

void *timer_thread(void *args)
{
    time_t start_time = time(NULL);
    FILE *f = fopen(output_name, "r");
    struct tm *tm;
    char tbuf[32];
    char fsize_buf[32];

    while (1)
    {       
        time_t diff = time(NULL) - start_time;

        tm = gmtime(&diff);
        strftime(tbuf, ARRAY_SIZE(tbuf), "%H:%M:%S", tm);
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        format_file_size((double)file_size, fsize_buf, ARRAY_SIZE(fsize_buf));
        printf("\33[2K\r");
        printf("    Recording for: %s, file size: %s\r", tbuf, fsize_buf);
        fflush(stdout);
        sleep(1);
    }

    fclose(f);
}

void int_handler()
{
    // Clean up
    clean_up();
    exit(0);
}

void clean_up()
{
    av_write_trailer(pAVOutputFormatContext);
}

int main(void)
{
    signal(SIGINT, int_handler);

    avdevice_register_all();

#ifdef LOG_VERBOSE
    av_log_set_level(AV_LOG_DEBUG);
#endif

    init_input_codec();
    init_output_codec();

    int ret = avformat_write_header(pAVOutputFormatContext, NULL);
    if (ret == AVSTREAM_INIT_IN_WRITE_HEADER || ret == AVSTREAM_INIT_IN_INIT_OUTPUT)
    {
        printf("Wrote header\n");
    }
    else
    {
        printf("Failed to write header\n");
        return 1;
    }

    AVFrame *picture;
    picture = av_frame_alloc();
    picture->width = pCodecContextOut->width;
    picture->height = pCodecContextOut->height;
    picture->format = pCodecContextOut->pix_fmt;
    ret = av_frame_get_buffer(picture, 0);
    assert(ret == 0);

    AVPacket *pAVPacketOut = av_packet_alloc();
    assert(pAVPacketOut != NULL);

    int frame_rate = 25; // Change this to your desired frame rate
    int64_t time_base_num = 1;
    int64_t time_base_den = frame_rate;

    AVFrame *scaledFrame = av_frame_alloc();
    assert(scaledFrame != NULL);
    scaledFrame->width = pCodecContextOut->width;
    scaledFrame->height = pCodecContextOut->height;
    scaledFrame->format = pCodecContextOut->pix_fmt;
    ret = av_frame_get_buffer(scaledFrame, 0);
    assert(ret == 0);

    // sws context to convert from x11 BGR0 pixel format to YUV420P
    struct SwsContext *swsContext = sws_getContext(
        pCodecContextIn->width, 
        pCodecContextIn->height, 
        pCodecContextIn->pix_fmt, 
        pCodecContextOut->width, 
        pCodecContextOut->height, 
        pCodecContextOut->pix_fmt, 
    SWS_BICUBIC, 0, 0, 0); 
    
    assert(swsContext != NULL);

    // Keep track of recording time
    pthread_t timer;
    pthread_create(&timer, NULL, timer_thread, NULL);

    for (int f = 0; f < FRAMES || FRAMES == 0; f++)
    {
        if (get_x11_frame(picture))
            continue;

        ret = sws_scale(swsContext, (const uint8_t *const *)picture->data, picture->linesize, 0, pCodecContextIn->height, scaledFrame->data, scaledFrame->linesize);
        assert(ret > 0);

        scaledFrame->pts = (f*90000)/60; // WHY IS THE TIMEBASE 90_000?

        ret = avcodec_send_frame(pCodecContextOut, scaledFrame);
        if (ret < 0)
        {
            printf("avcodec_send_frame: %d = %s\n", ret, av_err2str(ret));
            return 1;
        }

        while (1)
        {
            ret = avcodec_receive_packet(pCodecContextOut, pAVPacketOut);

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                av_packet_unref(pAVPacketOut);
                break;
            }
            else if (ret < 0)
            {
                printf("avcodec_receive_packet: %d = %s\n", ret, av_err2str(ret));
                av_packet_unref(pAVPacketOut);
                return 1;
            }

            // pAVPacketOut->pts = f*2000;

            ret = av_write_frame(pAVOutputFormatContext, pAVPacketOut);
            assert(ret == 0);
            // printf("Wrote data\n");
            av_packet_unref(pAVPacketOut);
        }
    }

        clean_up();
        printf("We should loop the packet and return multiple frames\n");
        printf("We should loop the packet and return multiple frames\n");
        printf("We should loop the packet and return multiple frames\n");
    
    
    return 0;
}

// Function to format file size
void format_file_size(double sizeInBytes, char *res, size_t s_max) {
    const char* units[] = {"Bytes", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    int unitIndex = 0;

    while (sizeInBytes >= 1024.0 && unitIndex < 8) {
        sizeInBytes /= 1024.0;
        unitIndex++;
    }

    snprintf(res, s_max, "%.2f %s", sizeInBytes, units[unitIndex]);
}