/* Copy from https://github.com/tea1896/ffmpeg_camera_streamer */
 
 
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
int exit_thread = 0;

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

int flush_video_encoder(AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx, unsigned int stream_index, int framecnt)
{
    int ret;
    int got_frame;
    AVPacket enc_pkt;
    if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities & CODEC_CAP_DELAY))
    {
        return 0;
    }
    
    while (1) 
    {
        enc_pkt.data = NULL;
        enc_pkt.size = 0;
        av_init_packet(&enc_pkt);
        ret = avcodec_encode_video2(ofmt_ctx->streams[stream_index]->codec, &enc_pkt, NULL, &got_frame);
        av_frame_free(NULL);
        if (ret < 0)
            break;
        if (!got_frame)
        {
            ret = 0;
            break;
        }
        av_log(NULL , AV_LOG_ERROR, "Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n", enc_pkt.size);
		framecnt++;
        AVRational time_base = ofmt_ctx->streams[stream_index]->time_base;//{ 1, 1000 };
        AVRational r_framerate1 = ifmt_ctx->streams[0]->r_frame_rate;// { 50, 2 };
        AVRational time_base_q = { 1, AV_TIME_BASE };
        int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));	//内部时间戳
        enc_pkt.pts = av_rescale_q(framecnt*calc_duration, time_base_q, time_base);
        enc_pkt.dts = enc_pkt.pts;
        enc_pkt.duration = av_rescale_q(calc_duration, time_base_q, time_base);

        /* copy packet*/
        enc_pkt.pos = -1;
        
        /* mux encoded frame */
        ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
        if (ret < 0)
            break;
    }
    return ret;
}

int flush_audio_encoder(AVFormatContext *ifmt_ctx_a, AVFormatContext *ofmt_ctx, unsigned int stream_index, int nb_samples){
	int ret;
	int got_frame;
	AVPacket enc_pkt;
	if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities & CODEC_CAP_DELAY))
    {   
		return 0;
    }
    
	while (1) 
    {
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_audio2(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
			NULL, &got_frame);
		av_frame_free(NULL);
		if (ret < 0)
			break;
		if (!got_frame)
        {
			ret = 0;
			break;
		}
		av_log(NULL , AV_LOG_ERROR, "Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n", enc_pkt.size);
		nb_samples+=ofmt_ctx->streams[stream_index]->codec->frame_size;
		//Write PTS
		AVRational time_base = ofmt_ctx->streams[stream_index]->time_base;//{ 1, 1000 };
		AVRational r_framerate1 = { ifmt_ctx_a->streams[0]->codec->sample_rate, 1 };
		AVRational time_base_q = { 1, AV_TIME_BASE };
		//Duration between 2 frames (us)
		int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));	//内部时间戳
		//Parameters
		enc_pkt.pts = av_rescale_q(nb_samples*calc_duration, time_base_q, time_base);
		enc_pkt.dts = enc_pkt.pts;
		enc_pkt.duration = ofmt_ctx->streams[stream_index]->codec->frame_size;

		/* copy packet*/
		//转换PTS/DTS（Convert PTS/DTS）
		enc_pkt.pos = -1;
		
		//ofmt_ctx->duration = enc_pkt.duration * nb_samples;

		/* mux encoded frame */
		ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
		if (ret < 0)
			break;
	}
	return ret;
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
	//const char * output_format = "mp4";
	//const char * output_path = "test.mp4";

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
	AVPacket  enc_pkt_v;
	AVPacket  * dec_pkt_a = NULL;
	AVPacket  enc_pkt_a;
	AVFrame * pFrame = NULL;
	AVFrame * pFrameYUV = NULL;
	struct SwsContext  * img_convert_ctx = NULL;
	struct SwrContext * aud_convert_ctx = NULL;
	int aud_next_pts = 0;
	int vid_next_pts = 0;
	int encode_video = 1;
	int encode_audio = 1;
	int dec_got_frame = 0;
	int enc_got_frame = 0;
	int dec_got_frame_a = 0;
	int enc_got_frame_a = 0;
	int frame_cnt_v = 0;
	int nb_samples = 0;

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
	av_log(NULL, AV_LOG_INFO, "Selected video capture device is %s\n", VideoDeviceName);

	//if not setting rtbufsize, error messages will be shown in cmd, but you can still watch or record the stream correctly in most time
	//setting rtbufsize will erase those error messages, however, larger rtbufsize will bring latency
	av_dict_set(&device_param, "rtbufsize", "10M", 0);

	ret = avformat_open_input(&ifmt_ctx_v, VideoDeviceName, ifmt, &device_param);
	if (0 != ret)
	{
		av_log(ifmt_ctx_v, AV_LOG_ERROR, "Video device %s can't open!\n", VideoDeviceName);
		goto app_end;
	}
	else
	{
		av_log(ifmt_ctx_v, AV_LOG_INFO, "Video device %s open successfully!\n", VideoDeviceName);
	}

	ret = avformat_find_stream_info(ifmt_ctx_v, NULL);
	if (ret < 0)
	{
		av_log(ifmt_ctx_v, AV_LOG_ERROR, "Video get stream info failed!\n");
		goto app_end;
	}
	video_stream_index = -1;
	for (i = 0; i<ifmt_ctx_v->nb_streams; i++)
	{
		if (ifmt_ctx_v->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_stream_index = i;
			break;
		}
	}
	if (-1 == video_stream_index)
	{
		av_log(ifmt_ctx_v, AV_LOG_ERROR, "Can't find a video stream!\n");
		goto app_end;
	}

	/* Find input video decoder */
	if (avcodec_open2(ifmt_ctx_v->streams[video_stream_index]->codec, avcodec_find_decoder(ifmt_ctx_v->streams[video_stream_index]->codec->codec_id), NULL) < 0)
	{
		av_log(ifmt_ctx_v, AV_LOG_ERROR, "Can't find a video decoder!\n");
		return -1;
	}

	/* 5. Open audio capture device */
	snprintf(AudioDeviceName, sizeof(AudioDeviceName), "audio=%s", g_Device_array[AudioDeviceIndex].c_str());
	av_log(NULL, AV_LOG_DEBUG, "Selected audio capture device is %s\n", AudioDeviceName);
	ret = avformat_open_input(&ifmt_ctx_a, AudioDeviceName, ifmt, &device_param);
	if (ret < 0)
	{
		av_log(ifmt_ctx_a, AV_LOG_ERROR, "Audio device %s can't open!\n", AudioDeviceName);
		goto app_end;
	}
	else
	{
		av_log(ifmt_ctx_a, AV_LOG_INFO, "Audio device %s open successfully!\n", AudioDeviceName);
	}
	ret = avformat_find_stream_info(ifmt_ctx_a, NULL);
	if (ret < 0)
	{
		av_log(ifmt_ctx_v, AV_LOG_ERROR, "Audio get stream info failed!\n");
		goto app_end;
	}
	audio_stream_index = -1;
	for (i = 0; i<ifmt_ctx_a->nb_streams; i++)
	{
		if (AVMEDIA_TYPE_AUDIO == ifmt_ctx_a->streams[i]->codec->codec_type)
		{
			audio_stream_index = i;
			break;
		}
	}
	if (-1 == audio_stream_index)
	{
		av_log(ifmt_ctx_v, AV_LOG_ERROR, "Can't find a audio stream!\n");
		goto app_end;
	}

	/* Find input audio decoder */
	if (avcodec_open2(ifmt_ctx_a->streams[audio_stream_index]->codec, avcodec_find_decoder(ifmt_ctx_a->streams[audio_stream_index]->codec->codec_id), NULL) < 0)
	{
		av_log(ifmt_ctx_v, AV_LOG_ERROR, "Can't find a audio decoder!\n");
		return -1;
	}

	/* 6. Initialize output */
	ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, output_format, output_path);
	if (ret < 0)
	{
		av_log(ifmt_ctx_a, AV_LOG_ERROR, "Initialize output failed!\n");
		goto app_end;
	}

	/* 7. Open video encoder and initialize it */
	pCodec_v = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (NULL == pCodec_v)
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Can't find video encoder!\n");
		goto app_end;
	}
	pCodecCtx_v = avcodec_alloc_context3(pCodec_v);
	pCodecCtx_v->pix_fmt = AV_PIX_FMT_YUV420P;
	pCodecCtx_v->width = ifmt_ctx_v->streams[video_stream_index]->codec->width;
	pCodecCtx_v->height = ifmt_ctx_v->streams[video_stream_index]->codec->height;
	pCodecCtx_v->time_base.num = 1;
	pCodecCtx_v->time_base.den = 30;
	pCodecCtx_v->bit_rate = 1000000;
	pCodecCtx_v->gop_size = 25;
	/* Some formats want stream headers to be separate. */
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
	{
		pCodecCtx_v->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	pCodecCtx_v->qmin = 10;
	pCodecCtx_v->qmax = 51;
	pCodecCtx_v->max_b_frames = 0;

	/* Open video encoder */
	AVDictionary *CodecCtx_v_param = 0;
	av_dict_set(&CodecCtx_v_param, "preset", "fast", 0);
	av_dict_set(&CodecCtx_v_param, "tune", "zerolatency", 0);
	if (avcodec_open2(pCodecCtx_v, pCodec_v, &CodecCtx_v_param) < 0)
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Open video encoder failed\n");
		goto app_end;
	}

	/* Add a new stream to output,should be called by the user before avformat_write_header() for muxing */
	video_stream = avformat_new_stream(ofmt_ctx, pCodec_v);
	if (video_stream == NULL)
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Create video stream failed\n");
		goto app_end;
	}
	//video_stream->time_base.num = pCodecCtx_v->time_base.num;
	//video_stream->time_base.den = pCodecCtx_v->time_base.den;
	video_stream->time_base.num = 1;
	video_stream->time_base.den = 25;
	video_stream->codec = pCodecCtx_v;


	/* 8. Open audio encoder and initialize it */
	pCodec_a = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!pCodec_a)
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Can't find audio encoder!\n");
		goto app_end;
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
		goto app_end;
	}

	/* 10. Add a new stream to output,should be called by the user before avformat_write_header() for muxing */
	audio_stream = avformat_new_stream(ofmt_ctx, pCodec_a);
	if (audio_stream == NULL)
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Create audio stream failed!\n");
		goto app_end;
	}
	audio_stream->time_base.num = 1;
	audio_stream->time_base.den = pCodecCtx_a->sample_rate;
	audio_stream->codec = pCodecCtx_a;

	/* 11. Open avio url */
	ret = avio_open(&ofmt_ctx->pb, output_path, AVIO_FLAG_READ_WRITE);
	if (ret < 0)
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Avio open failed!\n");
		goto app_end;
	}

	/* 12.Show output information */
	av_dump_format(ofmt_ctx, 0, output_path, 1);

	/* 13.Write header */
	avformat_write_header(ofmt_ctx, NULL);

	/* 14.Prepare for decode */
	//dec_pkt_v = av_packet_alloc();
	dec_pkt_v = (AVPacket *)av_malloc(sizeof(AVPacket));

	/* 15.Convertion of video from input (RGB , jpg, and so on) to YUV420P */
	img_convert_ctx = sws_getContext(ifmt_ctx_v->streams[video_stream_index]->codec->width,
		ifmt_ctx_v->streams[video_stream_index]->codec->height,
		ifmt_ctx_v->streams[video_stream_index]->codec->pix_fmt,
		pCodecCtx_v->width,
		pCodecCtx_v->height,
		AV_PIX_FMT_YUV420P,
		SWS_BICUBIC, NULL, NULL, NULL);

	/* 16. Resample of audio form input to output */
	aud_convert_ctx = swr_alloc_set_opts(NULL,
		av_get_default_channel_layout(pCodecCtx_a->channels),
		pCodecCtx_a->sample_fmt,
		pCodecCtx_a->sample_rate,
		av_get_default_channel_layout(ifmt_ctx_a->streams[audio_stream_index]->codec->channels),
		ifmt_ctx_a->streams[audio_stream_index]->codec->sample_fmt,
		ifmt_ctx_a->streams[audio_stream_index]->codec->sample_rate,
		0, NULL);

	swr_init(aud_convert_ctx);

	/* 17. Prepare buffers */
	/* Prepare buffer to store yuv data to be encoded */
	pFrameYUV = av_frame_alloc();
	uint8_t *  output_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P,
		pCodecCtx_v->width,
		pCodecCtx_v->height));
	avpicture_fill((AVPicture *)pFrameYUV, output_buffer, AV_PIX_FMT_YUV420P, pCodecCtx_v->width, pCodecCtx_v->height);


	/* Prepare fifo to store audio sample to be encoded */
	AVAudioFifo * audio_fifo = NULL;
	audio_fifo = av_audio_fifo_alloc(pCodecCtx_a->sample_fmt,
		pCodecCtx_a->channels,
		1);

	/* Initialize the buffer to store converted samples to be encoded. */
	uint8_t **converted_input_samples = NULL;

	/**
	* Allocate as many pointers as there are audio channels.
	* Each pointer will later point to the audio samples of the corresponding
	* channels (although it may be NULL for interleaved formats).
	*/
	if (!(converted_input_samples = (uint8_t**)calloc(pCodecCtx_a->channels, sizeof(**converted_input_samples))))
	{
		av_log(ofmt_ctx, AV_LOG_ERROR, "Could not allocate converted input sample pointers\n");
		goto app_end;
	}

	int64_t start_time = av_gettime();
	AVRational time_base_q = { 1, AV_TIME_BASE };

	//av_log(NULL, AV_LOG_DEBUG, "To encoding .. \n");
	//system("pause");
	while (encode_video || encode_audio)
	{
		if (encode_video && (!encode_audio || av_compare_ts(vid_next_pts, time_base_q, aud_next_pts, time_base_q) <= 0))
		{
			if (1 == exit_thread)
			{
				break;
			}

			av_log(ofmt_ctx, AV_LOG_DEBUG, "Recoding a video frame!\n");

			/* Read a video packet */
			ret = av_read_frame(ifmt_ctx_v, dec_pkt_v);
			if (ret >= 0)
			{
				/* Decode video packet */
				pFrame = av_frame_alloc();
				if (NULL == pFrame)
				{
					av_log(ofmt_ctx, AV_LOG_ERROR, "Could not malloc frame\n");
					goto app_end;
				}

				ret = avcodec_decode_video2(ifmt_ctx_v->streams[dec_pkt_v->stream_index]->codec,
					pFrame,
					&dec_got_frame,
					dec_pkt_v);
				if (ret < 0)
				{
					av_free(pFrame);
					av_log(NULL, AV_LOG_ERROR, "Video decoding failed [%d]!\n", ret);
					goto app_end;
				}

				/* Scale decoded image */
				if (dec_got_frame > 0)
				{
					sws_scale(img_convert_ctx,
						(const uint8_t* const*)pFrame->data,
						pFrame->linesize,
						0,
						pCodecCtx_v->height,
						pFrameYUV->data,
						pFrameYUV->linesize);
					pFrameYUV->width = pFrame->width;
					pFrameYUV->height = pFrame->height;
					pFrameYUV->format = AV_PIX_FMT_YUV420P;

					/* Re-encode image */
					enc_pkt_v.data = NULL;
					enc_pkt_v.size = 0;
					av_init_packet(&enc_pkt_v);
					ret = avcodec_encode_video2(pCodecCtx_v,
						&enc_pkt_v,
						pFrameYUV,
						&enc_got_frame);
					if (0 != ret)
					{
						av_log(NULL, AV_LOG_DEBUG, "Vedio encoder failed [%d]\n", ret);
					}
					av_frame_free(&pFrame);

					/* Mux video */
					if (1 == enc_got_frame)
					{
						//av_log(NULL, AV_LOG_DEBUG, "Encode video start!\n");

						frame_cnt_v++;
						enc_pkt_v.stream_index = video_stream->index;

						/* Write pts */
						AVRational time_base_o = ofmt_ctx->streams[0]->time_base; // {1, 1000}
						AVRational framerate_v = ifmt_ctx_v->streams[video_stream_index]->r_frame_rate; // {50, 2}
						int64_t calc_duration = (double)(AV_TIME_BASE) * (1 / av_q2d(framerate_v));

						enc_pkt_v.pts = av_rescale_q(frame_cnt_v * calc_duration, time_base_q, time_base_o);
						enc_pkt_v.dts = enc_pkt_v.pts;
						enc_pkt_v.duration = av_rescale_q(calc_duration, time_base_q, time_base_o);
						enc_pkt_v.pos = -1;

						/* Delay */
						vid_next_pts = frame_cnt_v*calc_duration; //general timebase
						int64_t pts_time = av_rescale_q(enc_pkt_v.dts, time_base_o, time_base_q);
						int64_t now_time = av_gettime() - start_time;
						if ((pts_time > now_time)
							&& (vid_next_pts + pts_time - now_time)  < aud_next_pts)
						{
							av_log(NULL, AV_LOG_DEBUG, "ulseep %d!\n", pts_time - now_time);
							av_usleep(pts_time - now_time);
						}
                        //av_log(ofmt_ctx, AV_LOG_INFO, "Write video pts %lld !\n", enc_pkt_v.pts);

						ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt_v);
						//ret = av_write_frame(ofmt_ctx, &enc_pkt_v);
						if (0 != ret)
						{
							av_log(ofmt_ctx, AV_LOG_ERROR, "Write frame failed %d !\n", ret);
						}
						av_free_packet(&enc_pkt_v);
					}
					else
					{
						av_log(NULL, AV_LOG_DEBUG, "Can't encode video !\n");
					}
				}
				else
				{
					av_frame_free(&pFrame);
					av_log(NULL, AV_LOG_DEBUG, "Can't decode video successfully!\n");
				}
				av_free_packet(dec_pkt_v);
			}
			else
			{
				/* Can't read input video packet */
				if (AVERROR_EOF == ret)
				{
					av_log(NULL, AV_LOG_ERROR, "Read eof!\n");
					encode_video = 0;
				}
				else
				{
					av_log(NULL, AV_LOG_ERROR, "Can't read input video frame!\n");
                    goto app_end;
                    //return 0;
				}
			}
		}
		else
		{
            av_log(ofmt_ctx, AV_LOG_DEBUG, "Recoding a audio frame!\n");
        
			// audio transcoding 
			const int output_frame_size = pCodecCtx_a->frame_size;

			if (1 == exit_thread)
			{
				break;
			}

			av_log(NULL, AV_LOG_DEBUG, "Recoding a audio!\n");

			/**
			* Make sure that there is one frame worth of samples in the FIFO
			* buffer so that the encoder can do its work.
			* Since the decoder's and the encoder's frame size may differ, we
			* need to FIFO buffer to store as many frames worth of input samples
			* that they make up at least one frame worth of output samples.
			*/
			while (av_audio_fifo_size(audio_fifo) < output_frame_size)
			{
				/**
				* Decode one frame worth of audio samples, convert it to the
				* output sample format and put it into the FIFO buffer.
				*/
				AVFrame * input_frame = av_frame_alloc();
				if (!input_frame)
				{
					goto app_end;
				}

				/** Decode one frame worth of audio samples. */
				/** Packet used for temporary storage. */
				AVPacket input_packet;
				av_init_packet(&input_packet);
				input_packet.data = NULL;
				input_packet.size = 0;

				/** Read one audio frame from the input file into a temporary packet. */
				if ((ret = av_read_frame(ifmt_ctx_a, &input_packet)) < 0)
				{
					/** If we are at the end of the file, flush the decoder below. */
					if (ret == AVERROR_EOF)
					{
						encode_audio = 0;
                                            goto app_end;
					}
					else
					{
						av_log(NULL, AV_LOG_DEBUG, "Could not read audio frame\n");
						return ret;
					}
				}

				/**
				* Decode the audio frame stored in the temporary packet.
				* The input audio stream decoder is used to do this.
				* If we are at the end of the file, pass an empty packet to the decoder
				* to flush it.
				*/
				if ((ret = avcodec_decode_audio4(ifmt_ctx_a->streams[audio_stream_index]->codec, 
                                                                                  input_frame,
					                                              &dec_got_frame_a, &input_packet)) < 0)
				{
					av_log(NULL, AV_LOG_DEBUG, "Could not decode audio frame\n");
					return ret;
				}
				av_packet_unref(&input_packet);
				/** If there is decoded data, convert and store it */
				if (dec_got_frame_a)
				{
					/**
					* Allocate memory for the samples of all channels in one consecutive
					* block for convenience.
					*/
					if ((ret = av_samples_alloc(converted_input_samples, NULL,
						                                 pCodecCtx_a->channels,
						                                 input_frame->nb_samples,
						                                 pCodecCtx_a->sample_fmt, 0)) < 0)
					{
						av_log(NULL, AV_LOG_DEBUG, "Could not allocate converted input samples\n");
						av_freep(&(*converted_input_samples)[0]);
						free(*converted_input_samples);
						return ret;
					}

					/**
					* Convert the input samples to the desired output sample format.
					* This requires a temporary storage provided by converted_input_samples.
					*/
					/** Convert the samples using the resampler. */
					if ((ret = swr_convert(aud_convert_ctx,
						converted_input_samples, input_frame->nb_samples,
						(const uint8_t**)input_frame->extended_data, input_frame->nb_samples)) < 0)
					{
						av_log(NULL, AV_LOG_DEBUG, "Could not convert input samples\n");
						return ret;
					}

					/** Add the converted input samples to the FIFO buffer for later processing. */
					/**
					* Make the FIFO as large as it needs to be to hold both,
					* the old and the new samples.
					*/
					if ((ret = av_audio_fifo_realloc(audio_fifo, av_audio_fifo_size(audio_fifo) + input_frame->nb_samples)) < 0)
					{
						av_log(NULL, AV_LOG_DEBUG, "Could not reallocate FIFO\n");
						return ret;
					}

					/** Store the new samples in the FIFO buffer. */
					if (av_audio_fifo_write(audio_fifo, (void **)converted_input_samples,
						input_frame->nb_samples) < input_frame->nb_samples)
					{
						av_log(NULL, AV_LOG_DEBUG, "Could not write data to FIFO\n");
						return AVERROR_EXIT;
					}
				}
			}

			/**
			* If we have enough samples for the encoder, we encode them.
			* At the end of the file, we pass the remaining samples to
			* the encoder.
			*/
			if (av_audio_fifo_size(audio_fifo) >= output_frame_size)
				/**
				* Take one frame worth of audio samples from the FIFO buffer,
				* encode it and write it to the output file.
				*/
			{
				/** Temporary storage of the output samples of the frame written to the file. */
				AVFrame *output_frame = av_frame_alloc();
				if (!output_frame)
				{
					ret = AVERROR(ENOMEM);
					return ret;
				}
				/**
				* Use the maximum number of possible samples per frame.
				* If there is less than the maximum possible frame size in the FIFO
				* buffer use this number. Otherwise, use the maximum possible frame size
				*/
				const int frame_size = FFMIN(av_audio_fifo_size(audio_fifo), pCodecCtx_a->frame_size);

				/** Initialize temporary storage for one output frame. */
				/**
				* Set the frame's parameters, especially its size and format.
				* av_frame_get_buffer needs this to allocate memory for the
				* audio samples of the frame.
				* Default channel layouts based on the number of channels
				* are assumed for simplicity.
				*/
				output_frame->nb_samples = frame_size;
				output_frame->channel_layout = pCodecCtx_a->channel_layout;
				output_frame->format = pCodecCtx_a->sample_fmt;
				output_frame->sample_rate = pCodecCtx_a->sample_rate;

				/**
				* Allocate the samples of the created frame. This call will make
				* sure that the audio frame can hold as many samples as specified.
				*/
				if ((ret = av_frame_get_buffer(output_frame, 0)) < 0) 
                {
					av_log(NULL, AV_LOG_DEBUG, "Could not allocate output frame samples\n");
					av_frame_free(&output_frame);
					return ret;
				}

				/**
				* Read as many samples from the FIFO buffer as required to fill the frame.
				* The samples are stored in the frame temporarily.
				*/
				if (av_audio_fifo_read(audio_fifo, (void **)output_frame->data, frame_size) < frame_size)
                            {
					av_log(NULL, AV_LOG_DEBUG, "Could not read data from FIFO\n");
					return AVERROR_EXIT;
				}

				/** Encode one frame worth of audio samples. */
				/** Packet used for temporary storage. */
				AVPacket output_packet;
				av_init_packet(&output_packet);
				output_packet.data = NULL;
				output_packet.size = 0;

				/** Set a timestamp based on the sample rate for the container. */
				if (output_frame)
				{
					nb_samples += output_frame->nb_samples;
				}

				/**
				* Encode the audio frame and store it in the temporary packet.
				* The output audio stream encoder is used to do this.
				*/
				if ((ret = avcodec_encode_audio2(pCodecCtx_a, &output_packet,
					output_frame, &enc_got_frame_a)) < 0)
				{
					av_log(NULL, AV_LOG_DEBUG, "Could not encode frame\n");
					av_packet_unref(&output_packet);
					return ret;
				}

				/** Write one audio frame from the temporary packet to the output file. */
				if (enc_got_frame_a)
				{

					output_packet.stream_index = 1;

					AVRational time_base = ofmt_ctx->streams[1]->time_base;
					AVRational r_framerate1 = { ifmt_ctx_a->streams[audio_stream_index]->codec->sample_rate, 1 };// { 44100, 1};  
					int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));  

					output_packet.pts = av_rescale_q(nb_samples*calc_duration, time_base_q, time_base);
					output_packet.dts = output_packet.pts;
					output_packet.duration = output_frame->nb_samples;

					//printf("audio pts : %d\n", output_packet.pts);
					aud_next_pts = nb_samples*calc_duration;

					int64_t pts_time = av_rescale_q(output_packet.pts, time_base, time_base_q);
					int64_t now_time = av_gettime() - start_time;
					if ((pts_time > now_time) && ((aud_next_pts + pts_time - now_time)<vid_next_pts))
                    {               
						av_usleep(pts_time - now_time);
                    }

                    //av_log(ofmt_ctx, AV_LOG_INFO, "Write audio pts %lld !\n", (uint64_t)output_packet.pts);
					if ((ret = av_interleaved_write_frame(ofmt_ctx, &output_packet)) < 0)
					{
						av_log(ofmt_ctx, AV_LOG_ERROR, "Could not write frame\n");
						av_packet_unref(&output_packet);
						return ret;
					}

					av_packet_unref(&output_packet);
				}
				av_frame_free(&output_frame);
			}
		}
	}


    // Flush encoder
    ret = flush_video_encoder(ifmt_ctx_v, ofmt_ctx, 0, frame_cnt_v);
    if( ret < 0 )
    {
        av_log(ofmt_ctx, AV_LOG_ERROR, "Flushing video encoder failed!\n");     
    }

    ret = flush_audio_encoder(ifmt_ctx_a, ofmt_ctx, 0, nb_samples);
    if( ret < 0 )
    {
        av_log(ofmt_ctx, AV_LOG_ERROR, "Flushing audio encoder failed!\n");     
    }

app_end:
    // Close decoder
    if(video_stream)
    {
        avcodec_close(video_stream->codec);
    }

    if(audio_stream)
    {
        avcodec_close(audio_stream->codec);
    }

    // Close video decoder buffer
    av_free(output_buffer);

    if (converted_input_samples) 
    {
		av_freep(&converted_input_samples[0]);
	}

    // Close audio decode buffer
    if(audio_fifo)
    {
        av_audio_fifo_free(audio_fifo);
    }

    // Close output muxer I/O
    avio_close(ofmt_ctx->pb);

    // Close input muxer
    avformat_free_context(ifmt_ctx_v);
    avformat_free_context(ifmt_ctx_a);
    avformat_free_context(ofmt_ctx);

    
	system("pause");

	return LC_SUCCESS;
}


