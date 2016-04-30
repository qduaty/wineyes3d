// GPL v3
// part of: wineyes3d

// transparent OpenGL window based on https://stackoverflow.com/questions/4052940/how-to-make-an-opengl-rendering-context-with-transparent-background/12290229#12290229?newreg=e50c2f448eae4c3f8ee2ae32925d0fa0

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdint>
#include <tchar.h>
#include <assert.h>
#include <Dwmapi.h>

#include <gl/glew.h>
#include <gl/wglew.h>

#include "WGL_ARB_multisample.h" // http://www.dhpoware.com/demos/glMultiSampleAntiAliasing.html

#pragma comment (lib, "dwmapi.lib")
#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "glu32.lib")

HDC hDC;            
HGLRC m_hrc;        
POINT mousePos;

int w = 150;
int h = 100;
int winx = 0;
int winy = 0;
float opacity = 1.0f;

/// registry functions from: https://genesisdatabase.wordpress.com/2010/10/12/reading-and-writing-registry-in-windows-using-winapi/
/// eg. GetKeyData(HKEY_LOCAL_MACHINE, “Software\\Microsoft\\Windows\\CurrentVersion\\Run”, “ApplicationName”, storeHere, strlen(storeHere));
int GetKeyData(HKEY hRootKey, char *subKey, char *value, LPBYTE data, DWORD cbData)
{
    HKEY hKey;
    if(RegOpenKeyEx(hRootKey, subKey, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return 0;
 
    if(RegQueryValueEx(hKey, value, NULL, NULL, data, &cbData) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return 0;
    }
 
    RegCloseKey(hKey);
    return 1;
}

/// eg. SetKeyData(HKEY_LOCAL_MACHINE, “Software\\Microsoft\\Windows\\CurrentVersion\\Run”, REG_SZ, “ApplicationName”, “C:\\ApplicationPath\\ApplicationName.exe”, strlen(“C:\\ApplicationPath\\ApplicationName.exe”));
int SetKeyData(HKEY hRootKey, char *subKey, DWORD dwType, char *value, LPBYTE data, DWORD cbData)
{
    HKEY hKey;
    if(RegCreateKey(hRootKey, subKey, &hKey) != ERROR_SUCCESS)
        return 0;
 
    if(RegSetValueEx(hKey, value, 0, dwType, data, cbData) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return 0;
    }
 
    RegCloseKey(hKey);
    return 1;
}

/// make window always on top
/// based on http://www.codeguru.com/cpp/w-d/dislog/article.php/c1857/Making-a-Window-Always-On-Top.htm
void StayOnTop(HWND m_hWnd)
{	
    RECT rect;
    // get the current window size and position
    GetWindowRect(m_hWnd, &rect);
 
    // now change the size, position, and Z order 
    // of the window.
    ::SetWindowPos(m_hWnd ,       // handle to window
                HWND_TOPMOST,  // placement-order handle
                rect.left,     // horizontal position
                rect.top,      // vertical position
                rect.right - rect.left,  // width
                rect.bottom - rect.top, // height
                SWP_SHOWWINDOW // window-positioning options
                );
}

int npoints;
float DEG2RAD;
const float radiusX = 0.5f;
const float radiusY = 1.0f;

BOOL initSC() {
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0, 0, 0, 0);

    return 0;
}

void resizeSC(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW );
    glLoadIdentity();

    npoints = max(4, (w + h) / 3);
    DEG2RAD = 2 * M_PI / (npoints - 1);
}

BOOL renderSC(float x, float y) {
    float norm = x && y ? sqrt(x*x+y*y):1;
    if(norm < 1) norm = 1;
    x/=norm;
    y/=norm;
    float x1 = x - 0.25;
    float x2 = x + 0.25;
    x1 = max(-0.5, min(0.5, x1));
    x2 = max(-0.5, min(0.5, x2));
    y = max(-0.5, min(0.5, y));
    x1 *= -radiusX;
    x2 *= -radiusX;
    y *= -radiusY;

    auto renderEllipse = [](float size, float xoffset, float x, float y, int which, int npoints){
        float z = which / -2.0;
        float origAmount = which == 2;

        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(origAmount * x + xoffset, origAmount * y, z);

        for(int i = 0; i < npoints; i++)
        {
           float rad = i * DEG2RAD;
           glVertex3f(origAmount * x + cos(rad) * radiusX * size + xoffset, origAmount * y + sin(rad) * radiusY * size, z);
        }

        glEnd();
    };

    auto renderOneSide = [y, renderEllipse](float xoffset, float x){
        glColor4f(0, 0, 0, opacity);
        renderEllipse(1, xoffset, x, y, 0, npoints);
        glColor4f(1, 1, 1, opacity);
        renderEllipse(0.8f, xoffset, x, y, 1, npoints);
        glColor4f(0, 0, 0, opacity);
        renderEllipse(0.15f, xoffset, x, y, 2, npoints);
    };

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPushMatrix();

    renderOneSide(-0.5f, x1); //L
    renderOneSide(0.5f, x2); //R

    glPopMatrix();
    glFlush();

    return 0;
}


BOOL CreateHGLRC(HWND hWnd) {
    PIXELFORMATDESCRIPTOR pfd = {
      sizeof(PIXELFORMATDESCRIPTOR),
      1,                                // Version Number
      PFD_DRAW_TO_WINDOW      |         // Format Must Support Window
      PFD_SUPPORT_OPENGL      |         // Format Must Support OpenGL
      PFD_SUPPORT_COMPOSITION |         // Format Must Support Composition
      PFD_DOUBLEBUFFER,                 // Must Support Double Buffering
      PFD_TYPE_RGBA,                    // Request An RGBA Format
      32,                               // Select Our Color Depth
      0, 0, 0, 0, 0, 0,                 // Color Bits Ignored
      8,                                // An Alpha Buffer
      0,                                // Shift Bit Ignored
      0,                                // No Accumulation Buffer
      0, 0, 0, 0,                       // Accumulation Bits Ignored
      24,                               // 16Bit Z-Buffer (Depth Buffer)
      8,                                // Some Stencil Buffer
      0,                                // No Auxiliary Buffer
      PFD_MAIN_PLANE,                   // Main Drawing Layer
      0,                                // Reserved
      0, 0, 0                           // Layer Masks Ignored
   };     

   HDC hdc = GetDC(hWnd);
   int PixelFormat = 0;

   ChooseBestAntiAliasingPixelFormat(PixelFormat, true);

   if(!PixelFormat)
       PixelFormat = ChoosePixelFormat(hdc, &pfd); // Find A Compatible Pixel Format

   if (PixelFormat == 0) {
      assert(0);
      return FALSE ;
   }

   BOOL bResult = SetPixelFormat(hdc, PixelFormat, &pfd);
   if (bResult==FALSE) {
      assert(0);
      return FALSE ;
   }

   m_hrc = wglCreateContext(hdc);
   if (!m_hrc){
      assert(0);
      return FALSE;
   }

   ReleaseDC(hWnd, hdc);

   return TRUE;
}

int dragx = 0, dragy = 0;

/// @return success (no error)
bool drawIfMust(HWND hWnd){
    // modified from wineyes
    POINT newmousePos;
    RECT windowRect;
    GetCursorPos((LPPOINT)&newmousePos);

    if(mousePos.x == newmousePos.x && mousePos.y == newmousePos.y)
        return true;

    HDC hdc = GetDC(hWnd);
    mousePos.x = newmousePos.x, mousePos.y = newmousePos.y;
    GetWindowRect(hWnd, &windowRect);
    float x = (windowRect.left + w / 2.0 - mousePos.x /* different coordinate system */) / w;
    float y = (mousePos.y - (windowRect.top + h / 2.0)) / h;

    wglMakeCurrent(hdc, m_hrc);
    renderSC(x, y);
    bool result = SwapBuffers(hdc) == TRUE;
    ReleaseDC(hWnd, hdc);
    return result;
}

int timerAvailable = 0; // 0 - Sleep(), 1 - vsync, 2 - win32 msg
bool renderingError = false;
bool windowMoved = false, windowFrameVisible = false, buttonPressed = false;

LRESULT CALLBACK WindowFunc(HWND hWnd,UINT msg, WPARAM wParam, LPARAM lParam) {
    PAINTSTRUCT ps;

    switch(msg) {
        case WM_CREATE:
        break;

        case WM_DESTROY:
            if(m_hrc) {
                wglMakeCurrent(NULL, NULL);
                wglDeleteContext(m_hrc);
            }
            KillTimer(hWnd, NULL);
            PostQuitMessage(0);
        break;

        case WM_TIMER:
            if(!drawIfMust(hWnd))
                renderingError = true;
        break;

        // copied from wineyes
        case WM_LBUTTONDOWN:
            buttonPressed = true;
            windowMoved = false;
            SetCapture(hWnd);
            dragx = (short)LOWORD(lParam);
            dragy = (short)HIWORD(lParam);
        break;

        case WM_MOUSEMOVE:
            if (buttonPressed) {
                static POINT mouseloc;
                POINT newmouseloc;
                RECT r;
                GetWindowRect(hWnd,&r);
                newmouseloc.x = r.left + (short)LOWORD(lParam);
                newmouseloc.y = r.top + (short)HIWORD(lParam);
                if ((newmouseloc.x != mouseloc.x) || (newmouseloc.y != mouseloc.y)){
                    mouseloc.x = newmouseloc.x;
                    mouseloc.y = newmouseloc.y;
                    winx = r.left + (short)LOWORD(lParam) - dragx;
                    winy = r.top + (short)HIWORD(lParam) - dragy;
                    MoveWindow(hWnd, winx, winy, w, h, 1);
                    windowMoved = true;
                }
            }
        break;
        // end of "copied from wineyes"

        case WM_LBUTTONUP:
            if (buttonPressed) 
            {
                ReleaseCapture();
                SetKeyData(HKEY_CURRENT_USER, "Software\\wineyes3d", REG_DWORD, "x", (LPBYTE)&winx, 4);
                SetKeyData(HKEY_CURRENT_USER, "Software\\wineyes3d", REG_DWORD, "y", (LPBYTE)&winy, 4);
            }

            if(!windowMoved)
            {
                if(!windowFrameVisible)
                    SetWindowLongPtr(hWnd, GWL_STYLE, WS_VISIBLE | WS_SIZEBOX);
                else
                {
    		        SetWindowLongPtr(hWnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
                    SetKeyData(HKEY_CURRENT_USER, "Software\\wineyes3d", REG_DWORD, "w", (LPBYTE)&w, 4);
                    SetKeyData(HKEY_CURRENT_USER, "Software\\wineyes3d", REG_DWORD, "h", (LPBYTE)&h, 4);
                }
                    
                windowFrameVisible = !windowFrameVisible;
            }

            buttonPressed = false;
        break;
        
        case WM_SIZE:
            {
                RECT r;
                GetWindowRect(hWnd,&r);
                w = r.right - r.left;
                h = r.bottom - r.top;
                resizeSC(w, h);
            }
        break;

        case WM_QUIT:
        {
            exit(0);
        }
        break;

        default: 
            return DefWindowProc(hWnd,msg,wParam,lParam);
    }

    return 0;
}

int WINAPI _tWinMain(HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR str,int nWinMode) {
    GetKeyData(HKEY_CURRENT_USER, "Software\\wineyes3d", "x", (LPBYTE)&winx, 4);
    GetKeyData(HKEY_CURRENT_USER, "Software\\wineyes3d", "y", (LPBYTE)&winy, 4);
    GetKeyData(HKEY_CURRENT_USER, "Software\\wineyes3d", "w", (LPBYTE)&w, 4);
    GetKeyData(HKEY_CURRENT_USER, "Software\\wineyes3d", "h", (LPBYTE)&h, 4);
    GetKeyData(HKEY_CURRENT_USER, "Software\\wineyes3d", "opacity", (LPBYTE)&opacity, 4);
    int SetKeyData(HKEY hRootKey, char *subKey, DWORD dwType, char *value, LPBYTE data, DWORD cbData);

    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = (WNDPROC)WindowFunc;
    wc.cbClsExtra  = 0;
    wc.cbWndExtra  = 0;
    wc.hInstance = hThisInst;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)CreateSolidBrush(0x00000000);
    wc.lpszClassName = "wineyes3D";

    if(!RegisterClassEx(&wc)) 
    {
        MessageBox(NULL, _T("RegisterClassEx - failed"), _T("Error"), MB_OK | MB_ICONERROR);
        return FALSE;
    }

    HWND hWnd = CreateWindowEx(WS_EX_TOOLWINDOW /* no taskbar button */, wc.lpszClassName, wc.lpszClassName, 
        WS_VISIBLE | WS_POPUP, winx, winy, w, h, NULL, NULL, hThisInst, NULL);

    StayOnTop(hWnd);

    if(!hWnd) 
    {
        MessageBox(NULL, _T("CreateWindowEx - failed"), _T("Error"), MB_OK | MB_ICONERROR);
        return FALSE;
    }

#if 1 // compositing
    DWM_BLURBEHIND bb = {0};
    HRGN hRgn = CreateRectRgn(0, 0, -1, -1);
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.hRgnBlur = hRgn;
    bb.fEnable = TRUE;
    DwmEnableBlurBehindWindow(hWnd, &bb);
#endif

    CreateHGLRC(hWnd);
    HDC hdc = GetDC(hWnd);
    wglMakeCurrent(hdc, m_hrc);

    GLenum result = glewInit();

    if(result != GLEW_OK)
    {
        ::MessageBox(hWnd, (char*)glewGetErrorString(result), "glew won't work", MB_OK);
        exit(1);
    }

    initSC();
    resizeSC(w, h);
    ReleaseDC(hWnd, hdc);
    MSG msg;

#if 0
    if(wglewIsSupported("WGL_EXT_swap_control") && wglSwapIntervalEXT(1))
        timerAvailable = 1;
    else if(SetTimer(hWnd, (int)NULL, 50, NULL))
        timerAvailable = 2;
#endif

    while(1) 
    {
        if(renderingError)
            break;

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else if(timerAvailable != 2)
        {
            if(timerAvailable == 0)
                Sleep(14 + (rand() % 2));

            if(!drawIfMust(hWnd))
            {
                renderingError = true;
                break;
            }
        }
    }

    return (FALSE); 
}