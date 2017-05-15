// live_camera.cpp : 定义控制台应用程序的入口点。
//

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

	int VideoDeviceIndex = 0;
	int AudioDeviceIndex = 0;
	int ret = 0;

	AVFormatContext * ifmt_ctx_v = NULL;
	AVFormatContext * ifmt_ctx_a = NULL;
	AVInputFormat * ifmt = NULL;

	/* Initialize ffmpeg components*/
	av_register_all();
	avdevice_register_all();
	avformat_network_init();

	av_log_set_level(AV_LOG_INFO);

	/* List all video and audio devices */
	showAllVideoDeivces();

	/* Select video device */
	cout << "Please select video capture device:" << endl;
	cin >> VideoDeviceIndex;
	cout << "Selected video device is " << g_Device_array[VideoDeviceIndex] << endl;

	/* Find all valid audio capture devices */
	cout << "Please select video capture device:" << endl;
	cin >> AudioDeviceIndex;
	cout << "Selected audio device is " << g_Device_array[AudioDeviceIndex] << endl;

	/* Open video capture device */
	AVDictionary * device_param = 0;
	ifmt = av_find_input_format("dshow");

	snprintf(VideoDeviceName, sizeof(VideoDeviceName), "video=%s", g_Device_array[VideoDeviceIndex].c_str());
	printf("video is %s\n", VideoDeviceName);
	ret = avformat_open_input(&ifmt_ctx_v, VideoDeviceName, ifmt, &device_param);
	if (0 != ret)
	{
		av_log(ifmt_ctx_v, AV_LOG_ERROR, "Video device %s can't open!\n", VideoDeviceName);
	}
	else
	{
		av_log(ifmt_ctx_v, AV_LOG_INFO, "Video device %s open successfully!\n", VideoDeviceName);
	}

	snprintf(AudioDeviceName, sizeof(AudioDeviceName), "audio=%s", g_Device_array[AudioDeviceIndex].c_str());
	printf("audio is %s\n", AudioDeviceName);
	ret = avformat_open_input(&ifmt_ctx_a, AudioDeviceName, ifmt, &device_param);
	if (0 != ret)
	{
		av_log(ifmt_ctx_a, AV_LOG_ERROR, "Audio device %s can't open!\n", AudioDeviceName);
	}
	else
	{
		av_log(ifmt_ctx_a, AV_LOG_INFO, "Audio device %s open successfully!\n", AudioDeviceName);
	}

	/* Initialize output */

	/* Encode video and audio and push it by net */


	system("pause");

	return 0;
}
