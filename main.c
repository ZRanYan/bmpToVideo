
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
int fd = -1;

char *mapped = NULL;
off_t file_size;


// #define MAP 

int readFile(int fileId)
{
    char nameString[20];
    sprintf(nameString, "file_%d.bin", fileId);
    fd = open(nameString, O_RDONLY);
#if MAP
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        close(fd);
        return EXIT_FAILURE;
    }
    file_size = st.st_size;
    printf("-readFile--%d\n",file_size);
    mapped = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return EXIT_FAILURE;
    }
#endif
    return 0;
}

void closeFile()
{
#if MAP
    if (munmap(mapped, file_size) < 0) {
        perror("munmap");
    }
#endif
    close(fd);
}

void printVersion()
{
     printf("FFmpeg version information:\n");
    printf("libavcodec version: %d.%d.%d\n", 
           LIBAVCODEC_VERSION_MAJOR, 
           LIBAVCODEC_VERSION_MINOR, 
           LIBAVCODEC_VERSION_MICRO);
    printf("libavformat version: %d.%d.%d\n", 
           LIBAVFORMAT_VERSION_MAJOR, 
           LIBAVFORMAT_VERSION_MINOR, 
           LIBAVFORMAT_VERSION_MICRO);
    printf("libavutil version: %d.%d.%d\n", 
           LIBAVUTIL_VERSION_MAJOR, 
           LIBAVUTIL_VERSION_MINOR, 
           LIBAVUTIL_VERSION_MICRO);
}

void listCodec()
{
    const AVCodec *codec = NULL;
    void *iter = NULL;
    while ((codec = av_codec_iterate(&iter))) {
        if (codec->type == AVMEDIA_TYPE_VIDEO) {
            printf("Codec ID: %d, Name: %s\n", codec->id, codec->name);
        }
    }
}

#define WIDTH 1024
#define HEIGHT 128
#define FRAMERATE 10

void encode_frame(AVFormatContext *fmt_ctx, AVCodecContext *codec_ctx, AVFrame *frame, AVPacket *pkt, int frame_index) {
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;

    int got_output;
    int ret = avcodec_encode_video2(codec_ctx, pkt, frame, &got_output);
    if (ret < 0) {
        fprintf(stderr, "Error encoding frame: %s\n", av_err2str(ret));
        exit(1);
    }

    if (got_output) {
        pkt->stream_index = fmt_ctx->streams[0]->index;

        // 设置时间戳
        pkt->pts = av_rescale_q(frame_index, codec_ctx->time_base, fmt_ctx->streams[0]->time_base);
        pkt->dts = pkt->pts;
        pkt->duration = av_rescale_q(1, codec_ctx->time_base, fmt_ctx->streams[0]->time_base);

        ret = av_write_frame(fmt_ctx, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error writing frame: %s\n", av_err2str(ret));
            exit(1);
        }
    }
}

char* capture_image(int i)
{
    static char imageData[WIDTH*HEIGHT];
    int bytesRead = pread64(fd, imageData, sizeof(imageData), i*WIDTH*HEIGHT);
    printf("i:%d  %d\n", i, bytesRead);
    return imageData;
}


int main(int argc, char *argv[]) {

    if(argc != 3)
    {
        printf("error input %d \n", argc);
        exit(-1);
    }
    int fileId = atoi(argv[1]);
    int frameNum = atoi(argv[2]);
    char videoFile[30];
    sprintf(videoFile, "out_%d_%d.avi", fileId, frameNum);
    
    printf("--------start-----videoFile:%s-----\n", videoFile);
    readFile(fileId);

    AVFormatContext *fmt_ctx = NULL;
    avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, videoFile);
    if (!fmt_ctx) {
        fprintf(stderr, "Could not create output context\n");
        return -1;
    }

    AVStream *video_stream = avformat_new_stream(fmt_ctx, NULL);
    if (!video_stream) {
        fprintf(stderr, "Could not create video stream\n");
        return -1;
    }

     // 寻找 FFV1 编码器
    AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_FFV1);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        return -1;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return -1;
    }
    codec_ctx->codec_id = AV_CODEC_ID_FFV1;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->width = WIDTH;
    codec_ctx->height = HEIGHT;
    codec_ctx->pix_fmt = AV_PIX_FMT_GRAY8;
    codec_ctx->time_base = (AVRational){1, FRAMERATE};

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return -1;
    }

     // 设置视频流的参数
    if (avcodec_parameters_from_context(video_stream->codecpar, codec_ctx) < 0) {
        fprintf(stderr, "Could not copy codec parameters to stream\n");
        return -1;
    }

    if (avio_open(&fmt_ctx->pb, videoFile, AVIO_FLAG_WRITE) < 0) {
        fprintf(stderr, "Could not open output file\n");
        return -1;
    }

    if (avformat_write_header(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        return -1;
    }

    // 分配视频帧
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        return -1;
    }
    frame->format = codec_ctx->pix_fmt;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;

    int ret = av_image_alloc(frame->data, frame->linesize, codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate raw picture buffer\n");
        return -1;
    }
    struct SwsContext *sws_ctx = sws_getContext(WIDTH, HEIGHT, AV_PIX_FMT_GRAY8, WIDTH, HEIGHT, codec_ctx->pix_fmt, SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        fprintf(stderr, "Could not initialize the conversion context\n");
        return -1;
    }
    AVPacket pkt;
    uint8_t *image_data = NULL;

    for (int i = 0; i < frameNum; i++) {
         // 这里你需要加载 BMP 图像数据到 image_data
        printf("----i:%d\n", i);
        #if MAP
            image_data = (uint8_t *)(mapped + i*WIDTH * HEIGHT);
        #else
            image_data = capture_image(i);
        #endif
        uint8_t *src_data[1] = { image_data };
        int src_linesize[1] = { WIDTH };
        sws_scale(sws_ctx, src_data, src_linesize, 0, HEIGHT, frame->data, frame->linesize);
        // 设置帧时间戳
        frame->pts = i;
        // 编码帧并写入文件
        encode_frame(fmt_ctx, codec_ctx, frame, &pkt, i);
    }

     // 写文件尾
    if (av_write_trailer(fmt_ctx) < 0) {
        fprintf(stderr, "Error writing trailer\n");
        return -1;
    }

    // 清理
    av_frame_free(&frame);
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_free_context(fmt_ctx);
    sws_freeContext(sws_ctx);

    closeFile();
    return 0;
}







