#pragma once
#ifdef __cplusplus
extern "C" {
#else
#if defined(_MSC_VER) && !defined(inline)
#define inline __inline
#endif
#endif

/*
* ����������Ƶ�߳� 
*/
bool __declspec(dllimport) startup_capture_send_video_thread();

#ifdef __cplusplus
}
#endif