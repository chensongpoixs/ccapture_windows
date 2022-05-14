/********************************************************************
created:	2022-05-08

author:		chensong


purpose:	capture hook
*********************************************************************/



#ifdef _MSC_VER
#pragma warning(disable : 4214) /* nonstandard extension, non-int bitfield */
#pragma warning(disable : 4054) /* function pointer to data pointer */
#endif

#include <windows.h>
#include "ccapture_hook.h"
// ������Ƿǳ���Ҫ�Ĺ� �������벻��ȥ�Ĺ� ^_^
#define COBJMACROS
#include <dxgi.h>
#include <d3d11.h>
#include <inttypes.h>
#include "gl-decs.h"
#include <psapi.h>
#include "include/detours.h"

#define DUMMY_WINDOW_CLASS_NAME L"graphics_hook_gl_dummy_window"
#include <stdio.h>
#include <stdlib.h>


//#include "C:\Work\cabroad_server\Server\Robot\ccloud_rendering_c.h"
//d3dx11.lib
//#pragma comment  (lib,"d3d11.lib")
/* clang-format off */

static const GUID GUID_IDXGIFactory1 =
{0x770aae78, 0xf26f, 0x4dba, {0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87}};
static const GUID GUID_IDXGIResource =
{0x035f3ab4, 0x482e, 0x4e50, {0xb4, 0x1f, 0x8a, 0x7f, 0x8b, 0xd8, 0x96, 0x0b}};

/* clang-format on */

typedef BOOL(WINAPI *PFN_SwapBuffers)(HDC);
typedef BOOL(WINAPI *PFN_WglSwapLayerBuffers)(HDC, UINT);
typedef BOOL(WINAPI *PFN_WglSwapBuffers)(HDC);
typedef BOOL(WINAPI *PFN_WglDeleteContext)(HGLRC);

PFN_SwapBuffers RealSwapBuffers = NULL;
PFN_WglSwapLayerBuffers RealWglSwapLayerBuffers = NULL;
PFN_WglSwapBuffers RealWglSwapBuffers = NULL;
PFN_WglDeleteContext RealWglDeleteContext = NULL;
typedef uint8_t bool;
#define false (0)
#define true (1)
static bool darkest_dungeon_fix = false;
static bool functions_initialized = false;
static FILE* out_gl_capture_ptr = NULL;


struct gl_data {
	HDC hdc;
	uint32_t cx;
	uint32_t cy;
	DXGI_FORMAT format;
	GLuint fbo;
	bool using_shtex;
	bool shmem_fallback;

	
		struct {
			struct shtex_data *shtex_info;
			ID3D11Device *d3d11_device;
			ID3D11DeviceContext *d3d11_context;
			ID3D11Texture2D *d3d11_tex;
			ID3D11Texture2D* d3d11_tex_video;
			IDXGISwapChain *dxgi_swap;
			HANDLE gl_device;
			HANDLE gl_dxobj;
			HANDLE handle;
			HWND hwnd;
			GLuint texture;
		};
	
		DWORD write_tick_count;
		bool capture_init;
};



struct gl_read_video
{
	int ready;
	void* handler;
};
static HMODULE gl = NULL;
static bool nv_capture_available = false;
static struct gl_data data = {0 };
static struct gl_read_video  gl_video_data = {0};
__declspec(thread) static int swap_recurse;
//static int read_cpu = 0;
static inline bool gl_error(const char *func, const char *str)
{
	 
	GLenum error = glGetError();
	if (error != 0) 
	{
		ERROR_EX_LOG("%s: %s: %lu", func, str, error);
		return true;
	}

	return false;
}

static void gl_free(void)
{
	 
	 

	if (data.using_shtex) {
		if (data.gl_dxobj)
			jimglDXUnregisterObjectNV(data.gl_device,
						  data.gl_dxobj);
		if (data.gl_device)
			jimglDXCloseDeviceNV(data.gl_device);
		if (data.texture)
			glDeleteTextures(1, &data.texture);
		if (data.d3d11_tex)
			ID3D11Resource_Release(data.d3d11_tex);
		if (data.d3d11_context)
			ID3D11DeviceContext_Release(data.d3d11_context);
		if (data.d3d11_device)
			ID3D11Device_Release(data.d3d11_device);
		if (data.d3d11_tex_video)
		{
			ID3D11Device_Release(data.d3d11_tex_video);
		}
		if (data.dxgi_swap)
			IDXGISwapChain_Release(data.dxgi_swap);
		if (data.hwnd)
			DestroyWindow(data.hwnd);
		
	}
	if (data.fbo)
	{
		glDeleteFramebuffers(1, &data.fbo);
	}
	data.capture_init = false;

	gl_error("gl_free", "GL error occurred on free");

	memset(&data, 0, sizeof(data));

	printf("------------------ gl capture freed ------------------");
}

static inline void *base_get_proc(const char *name)
{
	 
	return (void *)GetProcAddress(gl, name);
}

static inline void *wgl_get_proc(const char *name)
{
	 
	return (void *)jimglGetProcAddress(name);
}

static inline void *get_proc(const char *name)
{
	 
	void *func = wgl_get_proc(name);
	if (!func)
		func = base_get_proc(name);

	return func;
}



void* get_d3d11_device_context(void* cur_d3d11)
{
	ID3D11DeviceContext* d3d11_context_ptr = NULL;
	ID3D11Device_GetImmediateContext((ID3D11Device*)cur_d3d11,  &d3d11_context_ptr);
	return d3d11_context_ptr;
}

bool hook_captuer_ok(void )
{
	if (data.write_tick_count == 0 || !data.handle)
	{
		return false;
	}
	return true;
}

void * get_shared()
{
	gl_video_data.handler = data.handle;
	return &gl_video_data;
}



void send_video_data()
{
	
	c_cpp_rtc_texture((void*)get_shared(), data.cx, data.cy);
}
static void init_nv_functions(void)
{
	 
	jimglDXSetResourceShareHandleNV =
		get_proc("wglDXSetResourceShareHandleNV");
	jimglDXOpenDeviceNV = get_proc("wglDXOpenDeviceNV");
	jimglDXCloseDeviceNV = get_proc("wglDXCloseDeviceNV");
	jimglDXRegisterObjectNV = get_proc("wglDXRegisterObjectNV");
	jimglDXUnregisterObjectNV = get_proc("wglDXUnregisterObjectNV");
	jimglDXObjectAccessNV = get_proc("wglDXObjectAccessNV");
	jimglDXLockObjectsNV = get_proc("wglDXLockObjectsNV");
	jimglDXUnlockObjectsNV = get_proc("wglDXUnlockObjectsNV");

	nv_capture_available =
		!!jimglDXSetResourceShareHandleNV && !!jimglDXOpenDeviceNV &&
		!!jimglDXCloseDeviceNV && !!jimglDXRegisterObjectNV &&
		!!jimglDXUnregisterObjectNV && !!jimglDXObjectAccessNV &&
		!!jimglDXLockObjectsNV && !!jimglDXUnlockObjectsNV;

	if (nv_capture_available)
		printf("Shared-texture OpenGL capture available");
}

#define GET_PROC(cur_func, ptr, func)                                      \
	do {                                                               \
		ptr = get_proc(#func);                                     \
		if (!ptr) {                                                \
			ERROR_EX_LOG("%s: failed to get function '%s'", #cur_func, \
			     #func);                                       \
			success = false;                                   \
		}                                                          \
	} while (false)

static bool init_gl_functions(void)
{
	 
	bool success = true;

	jimglGetProcAddress = base_get_proc("wglGetProcAddress");
	if (!jimglGetProcAddress) {
		printf("init_gl_functions: failed to get wglGetProcAddress");
		return false;
	}

	GET_PROC(init_gl_functions, jimglMakeCurrent, wglMakeCurrent);
	GET_PROC(init_gl_functions, jimglGetCurrentDC, wglGetCurrentDC);
	GET_PROC(init_gl_functions, jimglGetCurrentContext,
		 wglGetCurrentContext);
	GET_PROC(init_gl_functions, glTexImage2D, glTexImage2D);
	GET_PROC(init_gl_functions, glReadBuffer, glReadBuffer);
	GET_PROC(init_gl_functions, glGetTexImage, glGetTexImage);
	GET_PROC(init_gl_functions, glDrawBuffer, glDrawBuffer);
	GET_PROC(init_gl_functions, glGetError, glGetError);
	GET_PROC(init_gl_functions, glBufferData, glBufferData);
	GET_PROC(init_gl_functions, glDeleteBuffers, glDeleteBuffers);
	GET_PROC(init_gl_functions, glDeleteTextures, glDeleteTextures);
	GET_PROC(init_gl_functions, glGenBuffers, glGenBuffers);
	GET_PROC(init_gl_functions, glGenTextures, glGenTextures);
	GET_PROC(init_gl_functions, glMapBuffer, glMapBuffer);
	GET_PROC(init_gl_functions, glUnmapBuffer, glUnmapBuffer);
	GET_PROC(init_gl_functions, glBindBuffer, glBindBuffer);
	GET_PROC(init_gl_functions, glGetIntegerv, glGetIntegerv);
	GET_PROC(init_gl_functions, glBindTexture, glBindTexture);
	GET_PROC(init_gl_functions, glGenFramebuffers, glGenFramebuffers);
	GET_PROC(init_gl_functions, glDeleteFramebuffers, glDeleteFramebuffers);
	GET_PROC(init_gl_functions, glBindFramebuffer, glBindFramebuffer);
	GET_PROC(init_gl_functions, glBlitFramebuffer, glBlitFramebuffer);
	GET_PROC(init_gl_functions, glFramebufferTexture2D,
		 glFramebufferTexture2D);

	init_nv_functions();
	return success;
}

static void get_window_size(HDC hdc, uint32_t *cx, uint32_t *cy)
{
	 
	HWND hwnd = WindowFromDC(hdc);
	RECT rc = {0};

	if (darkest_dungeon_fix) {
		*cx = 1920;
		*cy = 1080;
	} else {
		GetClientRect(hwnd, &rc);
		*cx = rc.right;
		*cy = rc.bottom;
	}
}

static inline bool gl_shtex_init_window(void)
{
	 
	data.hwnd = CreateWindowExW(
		0, DUMMY_WINDOW_CLASS_NAME, L"Dummy GL window, ignore",
		WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 2, 2, NULL,
		NULL, GetModuleHandle(NULL), NULL);
	if (!data.hwnd) {
		printf("gl_shtex_init_window: failed to create window: %d",
		     GetLastError());
		return false;
	}

	return true;
}

typedef HRESULT(WINAPI *create_dxgi_factory1_t)(REFIID, void **);

static const D3D_FEATURE_LEVEL feature_levels[] = {
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
};

static inline bool gl_shtex_init_d3d11(void)
{
	 
	D3D_FEATURE_LEVEL level_used;
	IDXGIFactory1 *factory;
	IDXGIAdapter *adapter;
	HRESULT hr;

	HMODULE d3d11 = load_system_library("d3d11.dll");
	if (!d3d11) {
		printf("gl_shtex_init_d3d11: failed to load D3D11.dll: %d",
		     GetLastError());
		return false;
	}

	HMODULE dxgi = load_system_library("dxgi.dll");
	if (!dxgi) {
		printf("gl_shtex_init_d3d11: failed to load DXGI.dll: %d",
		     GetLastError());
		return false;
	}

	DXGI_SWAP_CHAIN_DESC desc = {0};
	desc.BufferCount = 2;
	desc.BufferDesc.Format = data.format;
	desc.BufferDesc.Width = 2;
	desc.BufferDesc.Height = 2;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.SampleDesc.Count = 1;
	desc.Windowed = true;
	desc.OutputWindow = data.hwnd;

	create_dxgi_factory1_t create_factory =
		(void *)GetProcAddress(dxgi, "CreateDXGIFactory1");
	if (!create_factory) {
		printf("gl_shtex_init_d3d11: failed to load CreateDXGIFactory1 "
		     "procedure: %d",
		     GetLastError());
		return false;
	}

	PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN create =
		(void *)GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain");
	if (!create) {
		printf("gl_shtex_init_d3d11: failed to load "
		     "D3D11CreateDeviceAndSwapChain procedure: %d",
		     GetLastError());
		return false;
	}

	hr = create_factory(&GUID_IDXGIFactory1, (void **)&factory);
	if (FAILED(hr)) {
		printf("gl_shtex_init_d3d11: failed to create factory");
		return false;
	}

	hr = IDXGIFactory1_EnumAdapters1(factory, 0,
					 (IDXGIAdapter1 **)&adapter);
	IDXGIFactory1_Release(factory);

	if (FAILED(hr)) {
		printf("gl_shtex_init_d3d11: failed to create adapter");
		return false;
	}

	hr = create(adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, feature_levels,
		    sizeof(feature_levels) / sizeof(D3D_FEATURE_LEVEL),
		    D3D11_SDK_VERSION, &desc, &data.dxgi_swap,
		    &data.d3d11_device, &level_used, &data.d3d11_context);
	IDXGIAdapter_Release(adapter);

	if (FAILED(hr)) {
		printf("gl_shtex_init_d3d11: failed to create device");
		return false;
	}

	return true;
}

static inline bool gl_shtex_init_d3d11_tex(void)
{
	IDXGIResource* dxgi_res;
	HRESULT hr;

	D3D11_TEXTURE2D_DESC desc = {0};
	desc.Width = data.cx;
	desc.Height = data.cy;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = data.format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	hr = ID3D11Device_CreateTexture2D(data.d3d11_device, &desc, NULL,
					  &data.d3d11_tex);
	if (FAILED(hr)) {
		printf("gl_shtex_init_d3d11_tex: failed to create texture" );
		return false;
	}
	 
	//////////////////////////////////////////////////////////////////////////////////////////
	// ����D3D11��ͬ��GPUģʽ  
	hr = ID3D11Device_QueryInterface(data.d3d11_tex, &GUID_IDXGIResource,
		(void**)&dxgi_res);
	if (FAILED(hr))
	{
		printf("gl_shtex_init_d3d11_tex: failed to get IDXGIResource");
		return false;
	}

	hr = IDXGIResource_GetSharedHandle(dxgi_res, &data.handle);
	IDXGIResource_Release(dxgi_res);

	if (FAILED(hr))
	{
		printf("gl_shtex_init_d3d11_tex: failed to get shared handle");
		return false;
	}
	return true;
}
 
static inline bool gl_shtex_init_gl_tex(void)
{
	 //1. ��D3D11����Դת��ΪOpenGL����Դ
	data.gl_device = jimglDXOpenDeviceNV(data.d3d11_device);
	if (!data.gl_device) {
		printf("gl_shtex_init_gl_tex: failed to open device");
		return false;
	}
	// 2. �õ�OpenGL��������Ϣ
	glGenTextures(1, &data.texture);
	if (gl_error("gl_shtex_init_gl_tex", "failed to generate texture")) {
		return false;
	}
	// 3. ��D3D11��������Ϣӳ�䵽OpenGL����������Դ��ȥ
	data.gl_dxobj = jimglDXRegisterObjectNV(data.gl_device, data.d3d11_tex,
						data.texture, GL_TEXTURE_2D,
						WGL_ACCESS_WRITE_DISCARD_NV);
	if (!data.gl_dxobj) {
		printf("gl_shtex_init_gl_tex: failed to register object");
		return false;
	}

	return true;
}

/*

����FBO
����FBO�ķ�ʽ�����ڴ���VBO��ʹ��glGenFramebuffers

void glGenFramebuffers(
	GLsizei n,
	GLuint *ids);
1
2
3
n:������֡���������������
ids�����洴��֡����������ID��������߱���
���У�IDΪ0������ĺ��壬��ʾ����ϵͳ�ṩ��֡��������Ĭ�ϣ�
FBO����ʹ��֮��ʹ��glDeleteFramebuffersɾ����FBO

����FBO֮����ʹ��֮ǰ��Ҫ������ʹ��glBindFramebuffers

void glBindFramebuffer(GLenum target, GLuint id)
1
target:�󶨵�Ŀ�꣬�ò�����������Ϊ GL_FRAMEBUFFER
id����glGenFramebuffers������id
 
*/
static inline bool gl_init_fbo(void)
{     
	
	glGenFramebuffers(1, &data.fbo);
	return !gl_error("gl_init_fbo", "failed to initialize FBO");
}

static bool gl_shtex_init(HWND window)
{
	 // 1. ������ڳ�ʼ�� ��û�п���
	if (!gl_shtex_init_window()) 
	{
		return false;
	}
	// 2. �����豸 �뽻����
	if (!gl_shtex_init_d3d11()) 
	{
		return false;
	}
	// 3. ����Texture �ṹ
	if (!gl_shtex_init_d3d11_tex()) 
	{
		return false;
	}
	// 4. d3d11 ���豸ӳ�䵽OpenGL��ȥ 
	if (!gl_shtex_init_gl_tex()) 
	{
		return false;
	}
	if (!gl_init_fbo()) 
	{
		return false;
	}
	 

	printf("gl shared texture capture successful");
	return true;
}

 
 
#define INIT_SUCCESS 0
#define INIT_FAILED -1
#define INIT_SHTEX_FAILED -2

static int gl_init(HDC hdc)
{
	{
		SYSTEMTIME t1;
		GetSystemTime(&t1);
		DEBUG_EX_LOG("  cur = %u", t1.wMilliseconds);
	}
	HWND window = WindowFromDC(hdc);
	int ret = INIT_FAILED;
	bool success = false;
	RECT rc = {0};

	if (darkest_dungeon_fix) {
		data.cx = 1920;
		data.cy = 1080;
	} else {
		GetClientRect(window, &rc);
		data.cx = rc.right;
		data.cy = rc.bottom;
	}

	data.hdc = hdc;
	data.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	data.using_shtex = true;

	if (data.using_shtex) 
	{
		success = gl_shtex_init(window);
		if (!success) {
			ret = INIT_SHTEX_FAILED;
		}
	} 

	if (!success) 
	{
		gl_free();
	}
	else {
		ret = INIT_SUCCESS;
	}

	return ret;
}

static void gl_copy_backbuffer(GLuint dst)
{
	 
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, data.fbo);
	if (gl_error("gl_copy_backbuffer", "failed to bind FBO")) 
	{
		return;
	}

	glBindTexture(GL_TEXTURE_2D, dst);
	if (gl_error("gl_copy_backbuffer", "failed to bind texture")) 
	{
		return;
	}

	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, dst, 0);
	if (gl_error("gl_copy_backbuffer", "failed to set frame buffer")) 
	{
		return;
	}

	glReadBuffer(GL_BACK);

	/* darkest dungeon fix */
	darkest_dungeon_fix = glGetError() == GL_INVALID_OPERATION  && _strcmpi(process_name, "Darkest.exe") == 0 ;

	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	if (gl_error("gl_copy_backbuffer", "failed to set draw buffer")) 
	{
		return;
	}
	// TODO@chensong 2022-05-16   OpenGL ָ������λ�� ����  
	glBlitFramebuffer(0, data.cy, data.cx, 0, 0, 0, data.cx, data.cy,
			  GL_COLOR_BUFFER_BIT, GL_LINEAR);
	gl_error("gl_copy_backbuffer", "failed to blit");
}
 


 

 static void gl_shtex_capture(void)
 {

	 GLint last_fbo;
	 GLint last_tex;
	 // 1. ���� GPU���ڴ�
	 jimglDXLockObjectsNV(data.gl_device, 1, &data.gl_dxobj);

	 glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &last_fbo);
	 if (gl_error("gl_shtex_capture", "failed to get last fbo"))
	 {
		 return;
	 }

	 glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
	 if (gl_error("gl_shtex_capture", "failed to get last texture"))
	 {
		 return;
	 }
	 {
		 SYSTEMTIME t1;
		 GetSystemTime(&t1);
		 DEBUG_EX_LOG("  cur = %u", t1.wMilliseconds);
	 }
	 gl_copy_backbuffer(data.texture);
	 {
		 SYSTEMTIME t1;
		 GetSystemTime(&t1);
		 DEBUG_EX_LOG("  cur = %u", t1.wMilliseconds);
	 }
	 glBindTexture(GL_TEXTURE_2D, last_tex);
	 glBindFramebuffer(GL_DRAW_FRAMEBUFFER, last_fbo);


	 jimglDXUnlockObjectsNV(data.gl_device, 1, &data.gl_dxobj);


	 IDXGISwapChain_Present(data.dxgi_swap, 0, 0);
	
	 if (data.write_tick_count == 0)
	 {

		 c_set_send_video_callback(&g_send_video_callback);
	 }
	 data.write_tick_count = GetTickCount64();
	 //++read_cpu;
	 return;
	
 }
 
static void gl_capture(HDC hdc)
{ 
	static bool critical_failure = false;

	if (critical_failure) 
	{
		return;
	}

	if (!functions_initialized) {
		functions_initialized = init_gl_functions();
		if (!functions_initialized) {
			critical_failure = true;
			return;
		}
	}
	{
		SYSTEMTIME t1;
		GetSystemTime(&t1);
		DEBUG_EX_LOG("  cur = %u", t1.wMilliseconds);
	}
	/* reset error flag */
	glGetError();

	/*if (capture_should_stop()) {
		gl_free();
	}*/
	 
	if (!data.capture_init) {
		data.capture_init = true;
		if (gl_init(hdc) == INIT_SHTEX_FAILED) 
		{
			 // error info 
			return;
		}
	}
	{
		SYSTEMTIME t1;
		GetSystemTime(&t1);
		DEBUG_EX_LOG("  cur = %u", t1.wMilliseconds);
	}
	if (  hdc == data.hdc) {
		uint32_t new_cx;
		uint32_t new_cy;
		 
		/* reset capture if resized */
		get_window_size(hdc, &new_cx, &new_cy);
		if (new_cx != data.cx || new_cy != data.cy) 
		{
			if (new_cx != 0 && new_cy != 0) 
			{
				gl_free();
			}
			 
			return;
		}

		if (data.using_shtex)
		{
			{
				SYSTEMTIME t1;
				GetSystemTime(&t1);
				DEBUG_EX_LOG("  cur = %u", t1.wMilliseconds);
			}
			gl_shtex_capture();
			{
				SYSTEMTIME t1;
				GetSystemTime(&t1);
				DEBUG_EX_LOG("  cur = %u", t1.wMilliseconds);
			}
		} else {
			// error info 
		}
		 
	}

	 
}
// �������Ŀǰɶ���鶼û�и�
static inline void gl_swap_begin(HDC hdc)
{ 
   
}

static inline void gl_swap_end(HDC hdc)
{ 
	 
	 
	if ( gl_video_data.ready == 0)
	{
		{
		SYSTEMTIME t1;
		GetSystemTime(&t1);
		DEBUG_EX_LOG("capture -->> start cur = %u", t1.wMilliseconds);
		}
		// ����Ҫ��Ⱦ����Ļ��һ֡���ݿ���������GPU�Կ���ȥ �� 
		// ԭ����:
		// д��ʱ���ǣ�
		// ��ȡʱ������GPU�Կ���
		gl_capture(hdc);
		{
		SYSTEMTIME t1;
		GetSystemTime(&t1);
		DEBUG_EX_LOG("capture -->> end cur = %u", t1.wMilliseconds);
		}
	}
}

static BOOL WINAPI hook_swap_buffers(HDC hdc)
{
	 
	gl_swap_begin(hdc);

	const BOOL ret = RealSwapBuffers(hdc);

	gl_swap_end(hdc);

	return ret;
}

static BOOL WINAPI hook_wgl_swap_buffers(HDC hdc)
{
	 
	gl_swap_begin(hdc);

	const BOOL ret = RealWglSwapBuffers(hdc);

	gl_swap_end(hdc);

	return ret;
}

static BOOL WINAPI hook_wgl_swap_layer_buffers(HDC hdc, UINT planes)
{
	 
	gl_swap_begin(hdc);

	const BOOL ret = RealWglSwapLayerBuffers(hdc, planes);

	gl_swap_end(hdc);

	return ret;
}

static BOOL WINAPI hook_wgl_delete_context(HGLRC hrc)
{
	 
	if ( functions_initialized) {
		HDC last_hdc = jimglGetCurrentDC();
		HGLRC last_hrc = jimglGetCurrentContext();

		jimglMakeCurrent(data.hdc, hrc);
		gl_free();
		jimglMakeCurrent(last_hdc, last_hrc);
	}

	return RealWglDeleteContext(hrc);
}

static bool gl_register_window(void)
{
	 
	WNDCLASSW wc = {0};
	wc.style = CS_OWNDC;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpfnWndProc = DefWindowProc;
	wc.lpszClassName = DUMMY_WINDOW_CLASS_NAME;

	if (!RegisterClassW(&wc)) {
		printf("gl_register_window: failed to register window class: %d",
		     GetLastError());
		return false;
	}

	return true;
}
 
 bool hook_gl(void)
{
	 
	 
	void *wgl_dc_proc;
	void *wgl_slb_proc;
	void *wgl_sb_proc;

	gl = get_system_module("opengl32.dll");
	if (!gl) {
		return false;
	}

	/* "life is feudal: your own" somehow uses both opengl and directx at
	 * the same time, so blacklist it from capturing opengl */
	/*const char *process_name = get_process_name();
	if (_strcmpi(process_name, "yo_cm_client.exe") == 0 ||
	    _strcmpi(process_name, "cm_client.exe") == 0) {
		printf("Ignoring opengl for game: %s", process_name);
		return true;
	}*/

	if (!gl_register_window()) {
		return true;
	}

	wgl_dc_proc = base_get_proc("wglDeleteContext");
	wgl_slb_proc = base_get_proc("wglSwapLayerBuffers");
	wgl_sb_proc = base_get_proc("wglSwapBuffers");

	DetourTransactionBegin();

	RealSwapBuffers = SwapBuffers;
	DetourAttach((PVOID *)&RealSwapBuffers, hook_swap_buffers);
	if (wgl_dc_proc) {
		RealWglDeleteContext = (PFN_WglDeleteContext)wgl_dc_proc;
		DetourAttach((PVOID *)&RealWglDeleteContext,
			     hook_wgl_delete_context);
	}
	if (wgl_slb_proc) {
		RealWglSwapLayerBuffers = (PFN_WglSwapLayerBuffers)wgl_slb_proc;
		DetourAttach((PVOID *)&RealWglSwapLayerBuffers,
			     hook_wgl_swap_layer_buffers);
	}
	if (wgl_sb_proc) {
		RealWglSwapBuffers = (PFN_WglSwapBuffers)wgl_sb_proc;
		DetourAttach((PVOID *)&RealWglSwapBuffers,
			     hook_wgl_swap_buffers);
	}

	const LONG error = DetourTransactionCommit();
	const bool success = error == NO_ERROR;
	if (success) {
		printf("Hooked SwapBuffers");
		if (RealWglDeleteContext)
			printf("Hooked wglDeleteContext");
		if (RealWglSwapLayerBuffers)
			printf("Hooked wglSwapLayerBuffers");
		if (RealWglSwapBuffers)
			printf("Hooked wglSwapBuffers");
		printf("Hooked GL");
	} else {
		RealSwapBuffers = NULL;
		RealWglDeleteContext = NULL;
		RealWglSwapLayerBuffers = NULL;
		RealWglSwapBuffers = NULL;
		printf("Failed to attach Detours hook: %ld", error);
	}

	return success;
}
