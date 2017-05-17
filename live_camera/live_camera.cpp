// live_camera.cpp : 定义控制台应用程序的入口点。
// Author: tea1896@gmail.com

#include "stdafx.h"

using namespace std;

#include <vector>
#include <iostream>
#include <fstream>
#include <string>

#ifdef __cplusplus
extern "C"
{
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "libavutil/mathematics.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
};
#endif


#define TMP_FILE "tmp.txt"
#define LINE_SIZE 1024

#define LC_SUCCESS 0
#define LC_FAIL	   -1

vector<string> g_Device_array;

void av_log_selfcallback(void* ptr, int level, const char* fmt, va_list vl)
{
	char line[LINE_SIZE];
	vsnprintf(line, sizeof(line), fmt, vl);
	string strLine(line);

	string::size_type  pos = strLine.find_first_not_of(' ');
	string strtmp = strLine.substr(pos, strLine.size() - pos);

	/* Delete space */
	strtmp.erase(strtmp.find_last_not_of("\n\r\t") + 1);

	/* Delete "" */
	string::size_type  pos1 = strtmp.find_first_not_of('\"');
	string strnew = strtmp.substr(pos1, strtmp.size() - pos);

	strnew.erase(strnew.find_last_not_of("\"\"") + 1);

	g_Device_array.push_back(strnew);
}


void showAllVideoDeivces()
{
	int i = 0;
	vector<string>::iterator it;

	/* Set avlog callback function to save results of avformat_open_input */
	av_log_set_callback(av_log_selfcallback);

	/* Get all video devices */
	AVFormatContext *pFmtCtx = avformat_alloc_context();
	AVDictionary * options = NULL;
	av_dict_set(&options, "list_devices", "true", 0);
	AVInputFormat * iFormat = av_find_input_format("dshow");

	avformat_open_input(&pFmtCtx, "video=dummy", iFormat, &options);

	/* Print all devices */
	for (it = g_Device_array.begin(); it < g_Device_array.end(); it++)
	{
		cout << "Device - " << i++ << ":" << *it << endl;
	}

	/* Set avlog default callback function */
	av_log_set_callback(av_log_default_callback);
}



int main()
{
	/* Vars */
	vector<string>::iterator it;
	char VideoDeviceName[1024] = { 0 };
	char AudioDeviceName[1024] = { 0 };

	int video_stream_index = 0;
	int audio_stream_index = 0;
	int VideoDeviceIndex = 0;
	int AudioDeviceIndex = 0;
	int ret = 0;
	int i = 0;
	const char * output_format = "mpegts";
	const char * output_path = "udp://227.10.20.80:1234?pkt_size=1316";

	AVFormatContext * ifmt_ctx_v = NULL;
	AVFormatContext * ifmt_ctx_a = NULL;
	AVFormatContext * ofmt_ctx = NULL;
	AVInputFormat * ifmt = NULL;
	AVCodec * pCodec_v = NULL;
	AVCodecContext * pCodecCtx_v = NULL;
	AVCodec * pCodec_a = NULL;
	AVCodecContext * pCodecCtx_a = NULL;
	AVDictionary *param = NULL;
	AVStream * video_stream = NULL;
	AVStream * audio_stream = NULL;
	AVPacket  * dec_pkt_v = NULL;
	AVPacket * enc_pkt_v = NULL;
	AVPacket  * dec_pkt_a = NULL;
	AVPacket * enc_pkt_a = NULL;
	AVFrame * pFrame = NULL;
	AVFrame * pFrameYUV = NULL;
	struct SwsContext  * img_convert_ctx = NULL;
	struct SwrContext * aud_convert_ctx = NULL;

	/* Initialize ffmpeg components*/
	av_register_all();
	avdevice_register_all();
	avformat_network_init();

	av_log_set_level(AV_LOG_INFO);

	/* 1. List all video and audio devices */
	showAllVideoDeivces();

	/* 2. Select video device */
	cout << "Please select video capture device:" << endl;
	cin >> VideoDeviceIndex;
	cout << "Selected video device is " << g_Device_array[VideoDeviceIndex] << endl;

	/* 3. Select audio device */
	cout << "Please select video capture device:" << endl;
	cin >> AudioDeviceIndex;
	cout << "Selected audio device is " << g_Device_array[AudioDeviceIndex] << endl;

	/* 4. Open video capture device */
	AVDictionary * device_param = 0;
	ifmt = av_find_input_format("dshow");

	snprintf(VideoDeviceName, sizeof(VideoDeviceName), "video=%s", g_Device_array[VideoDeviceIndex].c_str());
	printf("video is %s\n", VideoDeviceName);
	ret = avformat_open_input(&ifmt_ctx_v, VideoDeviceName, ifmt, &device_param);
	if (0 != ret)
	{
		av_log(ifmt_ctx_v, AV_LOG_ERROR, "Video device %s can't open!\n", VideoDeviceName);
		return LC_FAIL;
	}
	else
	{
		av_log(ifmt_ctx_v, AV_LOG_INFO, "Video device %s open successfully!\n", VideoDeviceName);
	}

	ret = avformat_find_stream_info(ifmt_ctx_v, NULL);
	if (ret < 0)
	{
		av_log(ifmt_ctx_v, AV_LOG_ERROR, "Video get stream info failed!\n");
		return LC_FAIL;
	}
	video_stream_index = -1;
	for (i = 0; i<ifmt_ctx_v->nb_streams; i++)
	{
		if (ifmt_ctx_v->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_stream_index = i;
		}
	}
	if (-1 == video_stream_index)
	{
		av_log(ifmt_ctx_v, AV_LOG_ERROR, "Can't find a video stream!\n");
		return LC_FAIL;
	}

	/* 5. Open video capture device */
	snprintf(AudioDeviceName, sizeof(AudioDeviceName), "audio=%s", g_Device_array[AudioDeviceIndex].c_str());
	printf("audio is %s\n", AudioDeviceName);
	ret = avformat_open_input(&ifmt_ctx_a, AudioDeviceName, ifmt, &device_param);
	if (ret < 0)
	{
		av_log(ifmt_ctx_a, AV_LOG_ERROR, "Audio device %s can't open!\n", AudioDeviceName);
		return LC_FAIL;
	}
	else
	{
		av_log(ifmt_ctx_a, AV_LOG_INFO, "Audio device %s open successfully!\n", AudioDeviceName);
	}
	audio_stream_index = -1;
	for (i = 0; i<ifmt_ctx_a->nb_streams; i++)
	{
		if (AVMEDIA_TYPE_AUDIO == ifmt_ctx_a->streams[i]->codec->codec_type)
		{
			audio_stream_index = i;
		}
	}
	if (-1 == audio_stream_index)
	{
		av_log(ifmt_ctx_v, AV_LOG_ERROR, "Can't find a audio stream!\n");
		return LC_FAIL;
	}

	/* 6. Initialize output */
	ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, output_format, output_path);
	if (ret < 0)
	{
		av_log(ifmt_ctx_a, AV_LOG_ERROR, "Initialize output failed!\n");
		return LC_FAIL;
	}

	/* 7. Open video encoder and initialize it */
	pCodec_v = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (NULL == pCodec_v)
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Can't find video encoder!\n");
		return LC_FAIL;
	}
	pCodecCtx_v = avcodec_alloc_context3(pCodec_v);
	pCodecCtx_v->pix_fmt = AV_PIX_FMT_YUV420P;
	pCodecCtx_v->width = ifmt_ctx_v->streams[video_stream_index]->codec->width;
	pCodecCtx_v->height = ifmt_ctx_v->streams[video_stream_index]->codec->height;
	pCodecCtx_v->time_base.num = 1;
	pCodecCtx_v->time_base.den = 25;
	pCodecCtx_v->bit_rate = 1000000;
	pCodecCtx_v->gop_size = 50;

	/* Some formats want stream headers to be separate. */
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
	{
		pCodecCtx_v->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	/* Open video encoder */
	if (avcodec_open2(pCodecCtx_v, pCodec_v, &param) < 0)
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Open video encoder failed\n");
		return LC_FAIL;
	}

	/* Add a new stream to output,should be called by the user before avformat_write_header() for muxing */
	video_stream = avformat_new_stream(ofmt_ctx, pCodec_v);
	if (video_stream == NULL)
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Create video stream failed\n");
		return LC_FAIL;
	}
	video_stream->time_base.num = pCodecCtx_v->time_base.num;
	video_stream->time_base.den = pCodecCtx_v->time_base.den;
	video_stream->codec = pCodecCtx_v;


	/* 8. Open audio encoder and initialize it */
	pCodec_a = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!pCodec_a)
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Can't find audio encoder!\n");
		return LC_FAIL;
	}
	pCodecCtx_a = avcodec_alloc_context3(pCodec_a);
	pCodecCtx_a->channels = 2;
	pCodecCtx_a->channel_layout = av_get_default_channel_layout(2);
	pCodecCtx_a->sample_rate = ifmt_ctx_a->streams[audio_stream_index]->codec->sample_rate;
	pCodecCtx_a->sample_fmt = pCodec_a->sample_fmts[0];
	pCodecCtx_a->bit_rate = 32000;
	pCodecCtx_a->time_base.num = 1;
	pCodecCtx_a->time_base.den = pCodecCtx_a->sample_rate;
	/** Allow the use of the experimental AAC encoder */
	pCodecCtx_a->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
	/* Some formats want stream headers to be separate. */
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
	{
		pCodecCtx_a->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	if (avcodec_open2(pCodecCtx_a, pCodec_a, NULL) < 0)
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Open audio encoder failed!\n");
		return LC_FAIL;
	}

	/* 10. Add a new stream to output,should be called by the user before avformat_write_header() for muxing */
	audio_stream = avformat_new_stream(ofmt_ctx, pCodec_a);
	if (audio_stream == NULL)
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Create audio stream failed!\n");
		return LC_FAIL;
	}
	audio_stream->time_base.num = 1;
	audio_stream->time_base.den = pCodecCtx_a->sample_rate;
	audio_stream->codec = pCodecCtx_a;

	/* 11. Open avio url */
	ret = avio_open(&ofmt_ctx->pb,  output_path,  1);
	if( ret < 0 )
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Avio open failed!\n");
		return LC_FAIL;
	}

	/* 12.Show output information */
	av_dump_format(ofmt_ctx, 0, output_path, 1);

	/* 13.Write header */
	avformat_write_header(ofmt_ctx,  NULL);

	/* 14.Prepare for decode */
	dec_pkt_v = av_packet_alloc();

	/* 15.Convertion of video from input (RGB , jpg, and so on) to YUV420P */
	img_convert_ctx = sws_getContext(ifmt_ctx_v->streams[video_stream_index]->codec->width, ifmt_ctx_v->streams[video_stream_index]->codec->height,  ifmt_ctx_v->streams[video_stream_index]->codec->pix_fmt, 
								    ifmt_ctx_v->streams[video_stream_index]->codec->width, ifmt_ctx_v->streams[video_stream_index]->codec->height, AV_PIX_FMT_YUV420P,  SWS_BICUBIC, 
	                                                     NULL, NULL, NULL);

	/* 16. Resample of audio form input to output */
	aud_convert_ctx = swr_alloc_set_opts(NULL,  
									av_get_default_channel_layout(pCodecCtx_a->channels), 
									pCodecCtx_a->sample_fmt,
	                                                          pCodecCtx_a->sample_rate, 
	                                                          av_get_default_channel_layout(ifmt_ctx_a->streams[audio_stream_index]->codec->channels), 
	                                                          ifmt_ctx_a->streams[audio_stream_index]->codec->sample_fmt, 
	                                                          ifmt_ctx_a->streams[audio_stream_index]->codec->sample_rate, 
	                                                          0, NULL);

	/* 17. Prepare buffers */
	/* Prepare buffer to store yuv data to be encoded */
	pFrameYUV = av_frame_alloc();
	uint8_t *  output_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, 
															  pCodecCtx_v->width,
															  pCodecCtx_v->height));
	avpicture_fill((AVPicture *) pFrameYUV, output_buffer, AV_PIX_FMT_YUV420P, pCodecCtx_v->width, pCodecCtx_v->height);


	/* Prepare fifo to store audio sample to be encoded */
	AVAudioFifo * audio_fifo = NULL;
	audio_fifo = av_audio_fifo_alloc(pCodecCtx_a->sample_fmt, 
		                                          pCodecCtx_a->channels, 
		                                          1);
	


	system("pause");

	return LC_SUCCESS;
}
