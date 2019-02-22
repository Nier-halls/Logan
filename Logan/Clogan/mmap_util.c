/*
 * Copyright (c) 2018-present, 美团点评
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include "console_util.h"
#include <string.h>
#include "mmap_util.h"
#include <errno.h>
#include<android/log.h>

#define  LOG_TAG    "NativeLogan"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


/*
 * 创建MMAP缓存buffer或者内存buffer
 *  
 *  _filepath  缓存地址，在/data/user/0/test.logan.dianping.com.logan/files/logan_cache
 *  buffer     对应外部的_logan_buffer指针地址, 也就是mmap内存映射的地址,具体映射的文件就是上面_filepath处的logan.mmap2
 *  cache      对应外部的_cache_buffer_buffer指针地址
*/
int open_mmap_file_clogan(char *_filepath, unsigned char **buffer, unsigned char **cache) {
    int back = LOGAN_MMAP_FAIL;
    if (NULL == _filepath || 0 == strnlen(_filepath, 128)) {
        back = LOGAN_MMAP_MEMORY;
    } else {
        unsigned char *p_map = NULL;
        int size = LOGAN_MMAP_LENGTH;
		//O_RDWR | O_CREAT 读写模式打开或者创建文件
		//S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP 代表用户读写以及用户群组的读写权限
        int fd = open(_filepath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP); //后两个添加权限
        int isNeedCheck = 0; //是否需要检查mmap缓存文件重新检查
        if (fd != -1) { //保护
            int isFileOk = 0;
            FILE *file = fopen(_filepath, "rb+"); //先判断文件是否有值，再mmap内存映射
            if (NULL != file) {
                fseek(file, 0, SEEK_END);// fseek将文件指针移动到文件的末尾
                long longBytes = ftell(file);// ftell获取当前文件的指针为止

				LOGD("longBytes >>> %ld", longBytes);
                if (longBytes < LOGAN_MMAP_LENGTH) { //检查文件是否大于150kb
                    fseek(file, 0, SEEK_SET);
                    char zero_data[size];
                    memset(zero_data, 0, size);
                    size_t _size = 0;
                    _size = fwrite(zero_data, sizeof(char), size, file);//这里为什么要填充‘0’数据到文件中?
                    fflush(file);//每次打开缓存文件都会把文件中的所有数据全部填充成0?有什么套路吗?
                    //打日志看看是不是确实被全部都置为null了
                    fseek(file, 0, SEEK_SET);
                    char buff[10];
					fgets(buff, 10, file);
                    LOGD("longBytes >>> %s", buff);

                    if (_size == size) {
                        printf_clogan("copy data 2 mmap file success\n");
                        isFileOk = 1;
                        isNeedCheck = 1;
                    } else {
                        isFileOk = 0;
                    }
                } else {
                    isFileOk = 1;
                }
                fclose(file);
            } else {
                isFileOk = 0;
            }

			//这个加强保护是什么鬼?检查一下文件的长度?有没有超过150kb？
            if (isNeedCheck) { //加强保护，对映射的文件要有一个适合长度的文件
                FILE *file = fopen(_filepath, "rb");
                if (file != NULL) {
                    fseek(file, 0, SEEK_END);
                    long longBytes = ftell(file);
                    if (longBytes >= LOGAN_MMAP_LENGTH) {
                        isFileOk = 1;
                    } else {
                        isFileOk = 0;
                    }
                    fclose(file);
                } else {
                    isFileOk = 0;
                }
            }

            if (isFileOk) {
				/**
				 * mmap参数 
				 *
				 * addr mmap内存映射到的地址，如果传NULL由内核自己决定映射到哪里
				 * length 需要映射的文件长度
				 * prot mapping映射的权限
				 * flags 映射后更新会同时生效到文件显示在别的进程中的映射
				 * fd 需要mmap的文件描述符
				 * offset 映射的偏移量
				 */
                p_map = (unsigned char *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            }
            if (p_map != MAP_FAILED && NULL != p_map && isFileOk) {
                back = LOGAN_MMAP_MMAP;
            } else {
                back = LOGAN_MMAP_MEMORY;
                printf_clogan("open mmap fail , reason : %s \n", strerror(errno));

            }
            close(fd);

            if (back == LOGAN_MMAP_MMAP &&
                access(_filepath, F_OK) != -1) { //在返回mmap前,做最后一道判断，如果有mmap文件才用mmap
                back = LOGAN_MMAP_MMAP;
                *buffer = p_map;
            } else {
                back = LOGAN_MMAP_MEMORY;
                if (NULL != p_map)
                    munmap(p_map, size);
            }
        } else {
            printf_clogan("open(%s) fail: %s\n", _filepath, strerror(errno));
        }
    }

    int size = LOGAN_MEMORY_LENGTH;//内存缓存的大小，也是150kb
    unsigned char *tempData = malloc(size);
    if (NULL != tempData) {
        memset(tempData, 0, size);
        *cache = tempData;
        if (back != LOGAN_MMAP_MMAP) {
            *buffer = tempData;
            back = LOGAN_MMAP_MEMORY; //如果文件打开失败、如果mmap映射失败，走内存缓存
        }
    } else {
        if (back != LOGAN_MMAP_MMAP)
            back = LOGAN_MMAP_FAIL;
    }
    return back;
}
