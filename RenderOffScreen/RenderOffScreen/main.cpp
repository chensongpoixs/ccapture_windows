/***********************************************************************************************
					created: 		2022-07-21

					author:			chensong

					purpose:		RenderOffScreen demo one 
************************************************************************************************/

//#include "stdafx.h"
#include <windows.h>
#include <iostream>
#include <commctrl.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <string>
#pragma comment( lib, "OpenGL32.lib" )
#pragma comment( lib, "glu32.lib" )
//using namespace std;

BOOL SaveBmp(HBITMAP hBitmap, std::string FileName)
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
	if (iBits <= 1)
	{
		wBitCount = 1;
	}
	else if (iBits <= 4)
	{
		wBitCount = 4;
	}
	else if (iBits <= 8)
	{
		wBitCount = 8;
	}
	else
	{
		wBitCount = 24;
	}

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
	fh = CreateFile(FileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);


	if (fh == INVALID_HANDLE_VALUE) return FALSE;

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

	return TRUE;
}

void mGLRender()
{
	//1. ��ʾ�����ɫ��Ϊ��ɫ
	glClearColor(0.9f, 0.9f, 0.3f, 1.0f); 
	//2. ��ʾʵ������˰������������Ϊ��ɫ������glClear������Ψһ������ʾ��Ҫ������Ļ�����
	/*
	����ѡ��
	GL_COLOR_BUFFER_BIT:    ��ǰ��д����ɫ����
      GL_DEPTH_BUFFER_BIT:    ��Ȼ���
      GL_ACCUM_BUFFER_BIT:   �ۻ�����
����GL_STENCIL_BUFFER_BIT: ģ�建��
	*/
	glClear(GL_COLOR_BUFFER_BIT);
	
	//  3. ָ����һ�������ջ����һ�����������Ŀ��,��ѡֵ:
	//��GL_MODELVIEW,��ģ����ͼ�����ջӦ�����ľ��������������ִ�д����������Լ�������ͼ���ˡ�
	//  GL_PROJECTION, ��ͶӰ�����ջӦ�����ľ��������������ִ�д������Ϊ���ǵĳ�������͸�ӡ�
	//	GL_TEXTURE, ����������ջӦ�����ľ��������������ִ�д������Ϊ���ǵ�ͼ������������ͼ��
	glMatrixMode(GL_PROJECTION);
	// 4. �����ӽǵĺ���
	gluPerspective(30.0, 1.0, 1.0, 10.0);
	glMatrixMode(GL_MODELVIEW);
	// 5. �����ӽǵķ����� 
	gluLookAt(0, 0, -5, 0, 0, 0, 0, 1, 0);

	// 6. 
	/*
		��glBegin()��glEnd()֮��ɵ��õĺ���
		���� ��������
		glVertex*() ���ö�������
		glColor*() ���õ�ǰ��ɫ
		glIndex*() ���õ�ǰ��ɫ��
		glNormal*() ���÷�������
		glEvalCoord*() ��������
		glCallList(),glCallLists() ִ����ʾ�б�
		glTexCoord*() ������������
		glEdgeFlag*() ���Ʊ߽����
		glMaterial*() ���ò���
	*/
	glBegin(GL_TRIANGLES);
	// 6.1 ���õ�ǰ��ɫ
	glColor3d(1, 0, 0);
	// 6.2 ���ö�������
	glVertex3d(0, 1, 0);
	////////
	glColor3d(0, 1, 0);
	glVertex3d(-1, -1, 0);
	//
	glColor3d(0, 0, 1);
	glVertex3d(1, -1, 0);
	//
	glEnd();
	glFlush(); // remember to flush GL output!
}

int main(int argc, char* argv[])
{
	const int WIDTH = 500;
	const int HEIGHT = 500;

	// Create a memory DC compatible with the screen
	HDC hdc = CreateCompatibleDC(0);
	if (hdc == 0)
	{
		std::cout << "Could not create memory device context" << std::endl;;
	}

	// Create a bitmap compatible with the DC
	// must use CreateDIBSection(), and this means all pixel ops must be synchronised
	// using calls to GdiFlush() (see CreateDIBSection() docs)
	BITMAPINFO bmi = {
			{ sizeof(BITMAPINFOHEADER), WIDTH, HEIGHT, 1, 32, BI_RGB, 0, 0, 0, 0, 0 },
			{ 0 }
	};
	DWORD *pbits; // pointer to bitmap bits
	HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void **)&pbits,
		0, 0);
	if (hbm == 0)
	{
		std::cout << "Could not create bitmap" << std::endl;
	}

	//HDC hdcScreen = GetDC(0);
	//HBITMAP hbm = CreateCompatibleBitmap(hdcScreen,WIDTH,HEIGHT);

	// Select the bitmap into the DC
	HGDIOBJ r = SelectObject(hdc, hbm);
	if (r == 0)
	{
		std::cout << "Could not select bitmap into DC" << std::endl;;
	}

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
	if (pfid == 0)
	{
		std::cout << "Pixel format selection failed" << std::endl;;
	}

	// Set the pixel format
	// - must be done *after* the bitmap is selected into DC
	BOOL b = SetPixelFormat(hdc, pfid, &pfd);
	if (!b)
	{
		std::cout << "Pixel format set failed" << std::endl;;
	}

	// Create the OpenGL resource context (RC) and make it current to the thread
	HGLRC hglrc = wglCreateContext(hdc);
	if (hglrc == 0)
	{
		std::cout << "OpenGL resource context creation failed" << std::endl;;
	}
	wglMakeCurrent(hdc, hglrc);

	// Draw using GL - remember to sync with GdiFlush()
	GdiFlush();
	mGLRender();

	SaveBmp(hbm, "output.bmp");
	/*
	Examining the bitmap bits (pbits) at this point with a debugger will reveal
	that the colored triangle has been drawn.
	*/

	// Clean up
	wglDeleteContext(hglrc); // Delete RC
	SelectObject(hdc, r); // Remove bitmap from DC
	DeleteObject(hbm); // Delete bitmap
	DeleteDC(hdc); // Delete DC

	return 0;
}


/*

���ˣ�����ɹ������У�ȷʵ�ǿ��԰�������������ʲô���ģ�

CreateCompatibleDC

����dc

CreateDIBSection

����ͼ��

SelectObject

ͼ��ѡ��DC

SetPixelFormat

������Ԫ��ʽ

wglCreateContext

����RC

wglMakeCurrent

ѡ��RC

mGLRender

��ʼ��Ⱦ

SaveBmp

����ͼ��������Ҵ��������ժ�����ģ�

...

����
*/