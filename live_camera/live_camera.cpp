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

	string::size_type  pos = strLine.find_first_not_of('   ');
	string strnew = strLine.substr(pos, strLine.size() - pos);

	g_Device_array.push_back(strnew);
}


void showAllVideoDeivces()
{
	int i = 0;
	vector<string>::iterator it;

	/* Set avlog callback function to save results */
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
		cout << "Device - " << i++ << ":"<< *it << endl;
	}
}



int main()
{		
	vector<string>::iterator it;
	string AudioDeviceName;

	/* Initialize ffmpeg components*/
	av_register_all();
	avdevice_register_all();
	avformat_network_init();
	
	/* List all video and audio devices */
	showAllVideoDeivces();

	/* Select video device */
	cout << "Please select video capture device:" << endl;
	

	/* Find all valid audio capture devices */
	for (it = g_Device_array.begin(); it < g_Device_array.end(); it++)
	{
		if (NULL != strstr(it->c_str(), "High Definition Audio"))
		{
			cout << "Find audio device: " << *it << endl;
		}
	}


	/* Open video capture device */


	/* Open audio capture device */
	

	/* Initialize output */

	/* Encode video and audio and push it by net */
	

	system("pause");

    return 0;
}

