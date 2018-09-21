// Copyright (C) 2013-2018 Altera Corporation, San Jose, California, USA. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify, merge,
// publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to
// whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// This agreement shall be governed in all respects by the laws of the State of California and
// by the laws of the United States of America.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <fstream>
#include <set>
#include <iostream>
#include<opencv2/core/core.hpp>   //定义了许多数据结构和基本的函数
#include<opencv2/highgui/highgui.hpp>    //很方便的图像捕获接口
#include <opencv2/opencv.hpp>
#include<opencv/cv.h>
#include<opencv/highgui.h>

#include "threadpool.hpp"
using namespace cv;

using namespace std;

#ifndef MB
#define MB(x)     ((x) * 1024 * 1024)
#endif // MB
// Images are processed in batches. The batches are streamed to overlap
// computation and memory access
#define STREAMS 4 // number of parallel streams
#define MAX_DATA_SIZE (STREAMS * 100000000) // memory required for a batch

// Data structures required to orchestrate image batches
int runs = 2400, batch = 24;
unsigned char *image;
unsigned int totalOffset = 0;
unsigned int outDataSize = 0;

// Data structures maintained only during parsing of image headers

int length;
const unsigned char *pos;
int size;
int image_size;
int width, height;
bool downsample;

bool skipBytes(int count)
{
	pos += count;
	size -= count;
	length -= count;
	return size >= 0;
}

bool readLength()
{
	if (size < 2) return false;
	length = (pos[0] << 8) | pos[1];
	if (length > size) return false;
	return skipBytes(2);
}

// Parsing headers
bool startOfFrame()
{
	readLength();
	if (length < 15) return false;
	if (pos[0] != 8) return false;
	height = (pos[1] << 8) | pos[2];
	width = (pos[3] << 8) | pos[4];
	if (pos[5] != 3)
	{
		std::cout << "unsupported JPEG type in " << __LINE__ << std::endl;
		return false; // supports only 3 component color JPEGS
	}
	skipBytes(6);
	for (int i = 0; i < 3; ++i)
	{
		int ssx, ssy;
		if (!(ssx = pos[1] >> 4)) return false;
		if (ssx & (ssx - 1)) return false;
		if (!(ssy = pos[1] & 15)) return false;
		if (ssy & (ssy - 1)) return false;
		if (ssx != ssy)
		{
			std::cout << "unsupported JPEG type in " << __LINE__ << std::endl;
			return false;
		}
		if (ssx != 1 && ssx != 2)
		{
			std::cout << "unsupported JPEG type in " << __LINE__ << std::endl;
			return false;
		}
		if (i == 0) downsample = ssx > 1;
		skipBytes(3);
	}
	return skipBytes(length);
}

// The device requires the Huffman tables in a special format
// This function converts the Huffman tables in this format
bool computeDHT()
{
	int codelen, currcnt, spread, i;
	static unsigned char counts[16];
	readLength();
	while (length >= 17)
	{
		i = pos[0];
		if (i & 0xEC) return false;
		if (i & 0x02) return false;
		i = (i | (i >> 3)) & 3;  // combined DC/AC + tableid value
		for (codelen = 1; codelen <= 16; ++codelen)
		{
			counts[codelen - 1] = pos[codelen];
		}
		skipBytes(17);
		spread = 65536;
		int counter = 0;
		for (codelen = 1; codelen <= 16; ++codelen)
		{
			spread >>= 1;
			currcnt = counts[codelen - 1];
			if (currcnt)
			{
				if (length < currcnt) return false;
			}
			for (int j = 0; j < currcnt; ++j)
			{
				unsigned char code = pos[j];
				for (int k = spread; k > 0; k -= codelen > 11 ? 1 : 32)
				{
					if (codelen > 11) counter++; else counter += 32;
				}
			}
			if (currcnt) skipBytes(currcnt);
		}
	}
	return !length;
}

// Extracts the quantifier tables from the image headers
bool setDQT()
{
	int i;
	readLength();
	while (length >= 65)
	{
		i = pos[0];
		if (i & 0xFC) return false;
		skipBytes(65);
	}
	return !length;
}

// Extracts the scan data from the input image - this does not decompress the
// data
bool extractScan()
{
	readLength();
	if (length < 10) return false;
	if (pos[0] != 3) return false;
	skipBytes(1);
	for (int i = 0; i < 3; ++i)
	{
		if (pos[1] & 0xEE) return false;
		skipBytes(2);
	}
	if (pos[0] || (pos[1] != 63) || pos[2]) return false;
	skipBytes(length);
	unsigned short ds = downsample ? 1 : 0;

	int sz = size;


	if (pos[sz - 1] != 0xD9 || pos[sz - 2] != 0xFF) printf("WARN NOT END WITH 0xFFD9\n");
	while (pos[sz - 1] != 0xD9 || pos[sz - 2] != 0xFF) sz--;
	while ((sz % 4) != 2)
	{
		sz++;
	}
	image_size = 4 * ((sz + 3) / 4); // round size to cover all 4 bytes
	return true;
}

bool skipBytesMarker()
{
	if (!readLength()) return false;
	skipBytes(length);
	return true;
}

// Top-level decoding of a single image
bool decode(const void* jpeg, const int sz)
{
	pos = (const unsigned char*)jpeg;
	size = sz & 0x7FFFFFFF;
	if (size < 2)
	{
		printf("unknown error\n");
		exit(-1);
		return false;
	}
	if ((pos[0] ^ 0xFF) | (pos[1] ^ 0xD8))
	{
		printf("Header failed\n");
		return false;
	}
	skipBytes(2);
	bool scanned = false;
	while (!scanned)
	{
		if ((size < 2) || (pos[0] != 0xFF))
		{
			printf("Skip failed\n");
			return false;
		}
		skipBytes(2);
		switch (pos[-1])
		{
			case 0xC0: if (!startOfFrame())
				{
					printf("Decode SOF failed\n");
					return false;
				}
				break;
			case 0xC4: if (!computeDHT())
				{
					printf("Decode DHT failed\n");
					return false;
				}
				break;
			case 0xDB: if (!setDQT())
				{
					printf("Decode DQT failed\n");
					return false;
				}
				break;
			case 0xDA: if (!extractScan())
				{
					printf("Decode Scan failed\n");
					return false;
				}
				scanned = true;
				break;
			default:
				// We add comment
				if (((pos[-1] & 0xF0) == 0xE0) || (pos[-1] == 0xDD) || (pos[-1] == 0xFE))
				{
					if (!skipBytesMarker()) return false;
				}
				else
				{
					printf("Unidentified section %d\n", pos[-1]);
					return false;
				}
		}
	}
	return true;
}

bool parseWidthHeight(const void* jpeg, const int sz)
{
	pos = (const unsigned char*)jpeg;
	size = sz & 0x7FFFFFFF;
	if (size < 2) return false;
	if ((pos[0] ^ 0xFF) | (pos[1] ^ 0xD8))
	{
		printf("Header failed\n");
		return false;
	}
	for (int i = 2; i < size - 1; i++)
	{
		if (pos[i] == 0xFF && pos[i + 1] == 0xC0)
		{
			length = (pos[i + 2] << 8) | pos[i + 3];
			if (length < 9) return false;
			if (pos[i + 4] != 8) return false;
			height = (pos[i + 5] << 8) | pos[i + 6];
			width = (pos[i + 7] << 8) | pos[i + 8];
		}
	}
	return true;
}



std::vector<string> load_manifest(const char* manifest_path)
{
	///*
	std::vector<string> manifest;
	std::ifstream infile(manifest_path);
	if (!infile.good())
	{
		std::cout << "can not open file: " << manifest_path << std::endl;
		exit(0);
	}
	printf("open file: %s\n", manifest_path);
	string filename;
	int label;
	while (infile >> filename >> label)
	{
		//std::cout << filename << "," << label << std::endl;
		manifest.push_back(filename);
	}
	std::cout << "A total of " << manifest.size() << " images." << std::endl;
	infile.close();
	return manifest;
}

string cvresize(const char* input, const char* output);
int main_loop(const char* path, int index )
{
	static int nums = 0;
	char * buf;
	int sz;
	const char * filename = path;
	// Read in input file.
	FILE * f;
	f = fopen(filename, "rb");
	if (!f)
	{
		printf("Error opening the input file: %s.\n", filename);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	sz = (int)ftell(f);
	buf = (char *)malloc(sz);
	if (!buf)
	{
		printf("Not enough memory\n");
		fclose(f);
		return 1;
	}
	fseek(f, 0, SEEK_SET);
	sz = (int)fread(buf, 1, sz, f);
	fclose(f);

	int res = decode(buf, sz);
	if (!res)
	{
		nums++;
		printf("%d in %d ERROR %s\n", nums, index, filename);
		// if (index >= 0)
		// {
		// 	string abc = cvresize(filename, NULL);
		// 	main_loop(abc.c_str(), -1);
		// }
		// else
		// {
		// 	printf("cannot process this image......\n");
		// 	exit(0);
		// }
	}
	if (index < 0)
	{
		printf("decode status is: %d\n", res);
	}
	/*else
		printf("PASS %s\n", filename);
	*/

	// Allocate host memory for a batch of images on each parallel stream
	free(buf);
	buf = NULL;

	return 0;
}

string cvresize(const char* input, const char* output)
{
	Mat src_image = imread(input);
	Mat dst_image;
	float min_height = 375.0;
	float scale = std::min(src_image.cols / min_height, src_image.rows / min_height);
	resize(src_image, dst_image, Size(src_image.cols / scale, src_image.rows / scale), 0, 0, INTER_LINEAR);
	if (output == NULL)
		output = "tmp.jpg";
	imwrite(output, dst_image);
	return output;
}


#define _resize_thread_pool_
//#define __resize__

int main(int argc, char** argv)
{
#ifdef _resize_thread_pool_
	ThreadPool tp(30);
#endif
	string root_path = "/mnt/dc_p3700/imagenet/train/";
	const char* manifest_path = "/home/yang/project/imagenet/train.txt";
	std::vector<string> manifest = load_manifest(manifest_path);
	int index = 0;
	for (auto& each_file : manifest)
	{
		string abs_path = root_path + each_file;
#ifndef _resize_thread_pool_
#ifdef __resize__
		cvresize(abs_path.c_str(), abs_path.c_str());
#endif
		main_loop(abs_path.c_str(), index);
#endif
#ifdef _resize_thread_pool_
		tp.runTask([&]()
		{
			string _abs_path = root_path + each_file;
			cvresize(_abs_path.c_str(), _abs_path.c_str());
		});
#endif
		if (index++ % 1000 == 0)
		{
#ifdef _resize_thread_pool_
			tp.waitWorkComplete();
#endif
			printf("finished processing : %d imgs\n", index);
		}
	}
#ifdef _resize_thread_pool_
	tp.waitWorkComplete();
#endif
	printf("done....\n");
	return 0;
}
