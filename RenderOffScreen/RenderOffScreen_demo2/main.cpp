/***********************************************************************************************
					created: 		2022-07-21

					author:			chensong

					purpose:		RenderOffScreen demo two
************************************************************************************************/
#include <windows.h>
#include <iostream>
#include <commctrl.h>
#include <gl/gl.h>
#include <gl/glu.h>
//#include <gl/glew.h>
#include <GL\GL.h>
#include <GL\GLU.h>
//#include <GL/freeglut.h>
#pragma comment( lib, "OpenGL32.lib" )
#pragma comment( lib, "glu32.lib" )
//#pragma comment( lib, "freeglut.lib" )

using namespace std;

static HDC hdc;
static HBITMAP hbm;
static HGDIOBJ r;
static HGLRC hglrc;
static DWORD *pbits;// pointer to bitmap bits

static int WIDTH = 120;
static int HEIGHT = 90;

__declspec(dllexport) void StartBmpContext(int width, int height)
{
	WIDTH = width;
	HEIGHT = height;

	// Create a memory DC compatible with the screen
	hdc = CreateCompatibleDC(0);
	if (hdc == 0) cout << "Could not create memory device context";

	// Create a bitmap compatible with the DC
	// must use CreateDIBSection(), and this means all pixel ops must be synchronised
	// using calls to GdiFlush() (see CreateDIBSection() docs)
	BITMAPINFO bmi = {
			{ sizeof(BITMAPINFOHEADER), WIDTH, HEIGHT, 1, 32, BI_RGB, 0, 0, 0, 0, 0 },
			{ 0 }
	};

	hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void **)&pbits,
		0, 0);
	/*HBITMAP hbm = CreateCompatibleBitmap(hdc,WIDTH,HEIGHT);*/
	if (hbm == 0) cout << "Could not create bitmap";

	// Select the bitmap into the DC
	r = SelectObject(hdc, hbm);
	if (r == 0) cout << "Could not select bitmap into DC";

	// Choose the pixel format
	PIXELFORMATDESCRIPTOR pfd = {
			sizeof(PIXELFORMATDESCRIPTOR), // struct size
					1, // Version number
					PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL, // use OpenGL drawing to BM
					PFD_TYPE_RGBA, // RGBA pixel values
					32, // color bits
					0, 0, 0, // RGB bits shift sizes...
					0, 0, 0, // Don't care about them
					0, 0, // No alpha buffer info
					0, 0, 0, 0, 0, // No accumulation buffer
					32, // depth buffer bits
					0, // No stencil buffer
					0, // No auxiliary buffers
					PFD_MAIN_PLANE, // Layer type
					0, // Reserved (must be 0)
					0, // No layer mask
					0, // No visible mask
					0 // No damage mask
	};
	int pfid = ChoosePixelFormat(hdc, &pfd);
	cout << pfid << endl;
	if (pfid == 0) cout << "Pixel format selection failed";

	// Set the pixel format
	// - must be done *after* the bitmap is selected into DC
	BOOL b = SetPixelFormat(hdc, pfid, &pfd);
	if (!b) cout << "Pixel format set failed";

	// Create the OpenGL resource context (RC) and make it current to the thread
	hglrc = wglCreateContext(hdc);
	if (hglrc == 0) cout << "OpenGL resource context creation failed";
	wglMakeCurrent(hdc, hglrc);

}

int SaveBmp(HBITMAP hBitmap, char* FileName)
{
	HDC hDC;
	//��ǰ�ֱ�����ÿ������ռ�ֽ���
	int iBits;
	//λͼ��ÿ������ռ�ֽ���
	WORD wBitCount;
	//�����ɫ���С�� λͼ�������ֽڴ�С ��λͼ�ļ���С �� д���ļ��ֽ���
	DWORD dwPaletteSize = 0, dwBmBitsSize = 0, dwDIBSize = 0, dwWritten = 0;
	//λͼ���Խṹ
	BITMAP Bitmap;
	//λͼ�ļ�ͷ�ṹ
	BITMAPFILEHEADER bmfHdr;
	//λͼ��Ϣͷ�ṹ
	BITMAPINFOHEADER bi;
	//ָ��λͼ��Ϣͷ�ṹ
	LPBITMAPINFOHEADER lpbi;
	//�����ļ��������ڴ�������ɫ����
	HANDLE fh, hDib, hPal, hOldPal = NULL;

	//����λͼ�ļ�ÿ��������ռ�ֽ���
	hDC = CreateDC("DISPLAY", NULL, NULL, NULL);
	iBits = GetDeviceCaps(hDC, BITSPIXEL) * GetDeviceCaps(hDC, PLANES);
	DeleteDC(hDC);
	if (iBits <= 1) wBitCount = 1;
	else if (iBits <= 4) wBitCount = 4;
	else if (iBits <= 8) wBitCount = 8;
	else wBitCount = 24;

	GetObject(hBitmap, sizeof(Bitmap), (LPSTR)&Bitmap);
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = Bitmap.bmWidth;
	bi.biHeight = Bitmap.bmHeight;
	bi.biPlanes = 1;
	bi.biBitCount = wBitCount;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrImportant = 0;
	bi.biClrUsed = 0;

	dwBmBitsSize = ((Bitmap.bmWidth * wBitCount + 31) / 32) * 4 * Bitmap.bmHeight;

	//Ϊλͼ���ݷ����ڴ�
	hDib = GlobalAlloc(GHND, dwBmBitsSize + dwPaletteSize + sizeof(BITMAPINFOHEADER));
	lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDib);
	*lpbi = bi;

	// �����ɫ��
	hPal = GetStockObject(DEFAULT_PALETTE);
	if (hPal)
	{
		hDC = ::GetDC(NULL);
		hOldPal = ::SelectPalette(hDC, (HPALETTE)hPal, FALSE);
		RealizePalette(hDC);
	}

	// ��ȡ�õ�ɫ�����µ�����ֵ
	GetDIBits(hDC, hBitmap, 0, (UINT)Bitmap.bmHeight, (LPSTR)lpbi + sizeof(BITMAPINFOHEADER)
		+ dwPaletteSize, (BITMAPINFO *)lpbi, DIB_RGB_COLORS);

	//�ָ���ɫ��
	if (hOldPal)
	{
		::SelectPalette(hDC, (HPALETTE)hOldPal, TRUE);
		RealizePalette(hDC);
		::ReleaseDC(NULL, hDC);
	}

	//����λͼ�ļ�
	fh = CreateFile(FileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);


	if (fh == INVALID_HANDLE_VALUE) return 1;

	// ����λͼ�ļ�ͷ
	bmfHdr.bfType = 0x4D42; // "BM"
	dwDIBSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwPaletteSize + dwBmBitsSize;
	bmfHdr.bfSize = dwDIBSize;
	bmfHdr.bfReserved1 = 0;
	bmfHdr.bfReserved2 = 0;
	bmfHdr.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER) + dwPaletteSize;
	// д��λͼ�ļ�ͷ
	WriteFile(fh, (LPSTR)&bmfHdr, sizeof(BITMAPFILEHEADER), &dwWritten, NULL);
	// д��λͼ�ļ���������
	WriteFile(fh, (LPSTR)lpbi, dwDIBSize, &dwWritten, NULL);
	//���
	GlobalUnlock(hDib);
	GlobalFree(hDib);
	CloseHandle(fh);

	return 0;

}

__declspec(dllexport) int SaveBmp(char* FileName)
{
	return SaveBmp(hbm, FileName);
}

__declspec(dllexport) int GetWidth()
{
	return WIDTH;
}
__declspec(dllexport) int GetHeight()
{
	return HEIGHT;
}

__declspec(dllexport) void GetMemBmpData(char **s, int *slen)
{
	*s = (char*)pbits;
	*slen = WIDTH * HEIGHT * 4;
}

__declspec(dllexport) void EndBmpContext()
{
	// Clean up
	wglDeleteContext(hglrc); // Delete RC
	SelectObject(hdc, r); // Remove bitmap from DC
	DeleteObject(hbm); // Delete bitmap
	DeleteDC(hdc); // Delete DC
}



int main(int argc, char *argv[])
{

	return EXIT_SUCCESS;
}