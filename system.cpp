#include "system.h"

static bool running = true;
static HINSTANCE hInstance;
static HWND hwnd;
static Game* game = 0;

// timer stuff
double timer::inv_freq = 1;

// forward declarations
void ReshapeWindow( unsigned int w, unsigned int h );

// Math Stuff
// ----------------------------------------------------------------------------
vec3 normalize( const vec3& v ) { return v.normalized(); }
vec3 cross( const vec3& a, const vec3& b ) { return a.cross( b ); }
float dot( const vec3& a, const vec3& b ) { return a.dot( b ); }
vec3 operator * ( const float& s, const vec3& v ) { return vec3( v.x * s, v.y * s, v.z * s ); }
vec3 operator * ( const vec3& v, const float& s ) { return vec3( v.x * s, v.y * s, v.z * s ); }

// FatalError
// Generic error handling; called by FATALERROR macro.
// ----------------------------------------------------------------------------
void FatalError( const char* file, int line, const char* message )
{
	char t[8192];
	sprintf( t, "%s, line %i:\n%s", file, line, message );
	MessageBox( NULL, t, "Error", MB_OK | MB_ICONEXCLAMATION );
	exit( 0 );
}
void FatalError( const char* file, int line, const char* message, const char* context )
{
	char t[3000];
	sprintf( t, "%s, line %i:\n%s", file, line, message );
	MessageBox( NULL, t, context, MB_OK | MB_ICONEXCLAMATION );
	exit( 0 );
}

// GetTime
// Reasonably accurate time in a float
// ----------------------------------------------------------------------------
float GetTime()
{
	LARGE_INTEGER freq, value;
	QueryPerformanceCounter( &value );
	QueryPerformanceFrequency( &freq );
	return (float)((double)value.QuadPart / (double)freq.QuadPart);
}

// StartTimer
// Mark the start of a time period to measure
// ----------------------------------------------------------------------------
static LONGLONG startTime = 0;
void StartTimer()
{
	LARGE_INTEGER value;
	QueryPerformanceCounter( &value );
	startTime = value.QuadPart;
}

// GetDuration
// Get the duration since calling StartTimer
// ----------------------------------------------------------------------------
float GetDuration()
{
	LARGE_INTEGER freq, value;
	LONGLONG elapsed;
	QueryPerformanceCounter( &value );
	QueryPerformanceFrequency( &freq );
	elapsed = value.QuadPart - startTime;
	return (float)((double)elapsed / (double)freq.QuadPart);
}

// WndProc
// Handle windows messages.
// ----------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) 
{
	switch (message) 
	{
	case WM_SIZE:
		ReshapeWindow( LOWORD( lParam ), HIWORD( lParam ) );
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) running = false;
		if (game) game->KeyDown( wParam & 255 );
		break;
	case WM_MOUSEMOVE:
		if (game) game->MouseMove( LOWORD( lParam ), HIWORD( lParam ) );
		break;
	case WM_LBUTTONDOWN:
		if (game) game->MouseClick();
		break;
	case WM_KEYUP:
		if (game) game->KeyUp( wParam & 255 );
		break;
	}
	return DefWindowProc( hWnd, message, wParam, lParam );
}

// RedirectIOToConsole
// Add a debugging console window and divert stdout, stdin and sterr to it.
// Adapted from http://dslweb.nwnexus.com/~ast/dload/guicon.htm .
// ----------------------------------------------------------------------------
#pragma warning( disable:4311 ) 
#pragma warning( disable:4302 )
void RedirectIOToConsole()
{
	CONSOLE_SCREEN_BUFFER_INFO coninfo;
	AllocConsole();
	GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &coninfo );
	coninfo.dwSize.Y = 500;
	SetConsoleScreenBufferSize( GetStdHandle( STD_OUTPUT_HANDLE ), coninfo.dwSize );
	HANDLE h1 = GetStdHandle( STD_OUTPUT_HANDLE );
	int h2 = _open_osfhandle( (intptr_t)h1, _O_TEXT );
	FILE* fp = _fdopen( h2, "w" );
	*stdout = *fp;
	setvbuf( stdout, NULL, _IONBF, 0 );
	h1 = GetStdHandle( STD_INPUT_HANDLE ), h2 = _open_osfhandle( (intptr_t)h1, _O_TEXT );
	fp = _fdopen( h2, "r" ), *stdin = *fp;
	setvbuf( stdin, NULL, _IONBF, 0 );
	h1 = GetStdHandle( STD_ERROR_HANDLE ), h2 = _open_osfhandle( (intptr_t)h1, _O_TEXT );
	fp = _fdopen( h2, "w" ), *stderr = *fp;
	setvbuf( stderr, NULL, _IONBF, 0 );
	ios::sync_with_stdio();
	freopen( "CON", "w", stdout );
	freopen( "CON", "w", stderr );
}

// window_from_dc_replacement
// https://www.opengl.org/discussion_boards/showthread.php/173030 
// ----------------------------------------------------------------------------
HWND WINAPI window_from_dc_replacement( HDC dc )
{
	static HWND wnd = NULL;
	if (dc == NULL) return NULL;
	if (wnd == NULL) 
	{
		WNDCLASSA wc;
		memset( &wc, 0, sizeof( wc ) );
		wc.lpfnWndProc = DefWindowProc;
		wc.hInstance = GetModuleHandleA( NULL );
		wc.lpszClassName = "_dummy_window_class_";
		RegisterClassA( &wc );
		wnd = CreateWindowA( wc.lpszClassName, NULL, WS_POPUP, 0, 0, 32, 32, NULL, NULL, wc.hInstance, NULL );
	}
	return wnd;
}

// patch_window_from_dc
// https://www.opengl.org/discussion_boards/showthread.php/173030 
// ----------------------------------------------------------------------------
void patch_window_from_dc()
{
	DWORD old_prot;
	unsigned __int64 wfdc = (unsigned __int64)GetProcAddress( GetModuleHandleA( "user32.dll" ), "WindowFromDC" );
	VirtualProtect( (void*)wfdc, 14, PAGE_EXECUTE_WRITECOPY, &old_prot );
	// jmp [eip + 0]
	*(unsigned char*)(wfdc + 0) = 0xff;
	*(unsigned char*)(wfdc + 1) = 0x25;
	*(unsigned*)(wfdc + 2) = 0x00000000;
	*(unsigned __int64*)(wfdc + 6) = (unsigned __int64)&window_from_dc_replacement;
}

// OpenWindow
// Opens a window.
// ----------------------------------------------------------------------------
bool OpenWindow( const char* title, int width, int height ) 
{
	WNDCLASS windowClass;
	memset( &windowClass, 0, sizeof( windowClass ) );
	DWORD dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
	hInstance = GetModuleHandle( NULL );
	windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	windowClass.lpfnWndProc = (WNDPROC)WndProc;
	windowClass.hInstance = hInstance;
	windowClass.hIcon = LoadIcon( NULL, IDI_WINLOGO );
	windowClass.hCursor = LoadCursor( NULL, IDC_ARROW );
	windowClass.lpszClassName = title;
	if (!RegisterClass( &windowClass )) return false;
	hwnd = CreateWindowEx( dwExStyle, title, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, width, height, NULL, NULL, hInstance, NULL );
	if (!CreateGLContext()) FATALERROR( "Failed to create OpenGL context" );
	ShowWindow( hwnd, SW_SHOW );
	UpdateWindow( hwnd );
	return true;
}

// GetWindowHandle
// Provides global access to the application window handle.
// ----------------------------------------------------------------------------
HWND GetWindowHandle()
{
	return hwnd;
}

// ReshapeWindow
// Any operations required after a window resize go here.
// ----------------------------------------------------------------------------
void ReshapeWindow( unsigned int w, unsigned int h )
{
	// TODO
}

// WinMain
// Application entry point and message pump.
// ----------------------------------------------------------------------------
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) 
{
	MSG msg;
	RedirectIOToConsole();
	OpenWindow( "OpenCL Laboratory", SCRWIDTH, SCRHEIGHT );
	game = new Game();
	game->Init();
	while (running)
	{
		if (PeekMessage( &msg, NULL, 0, 0, PM_REMOVE )) 
		{
			if (msg.message == WM_QUIT) running = false; else 
			{
				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}
		}
		else 
		{
			game->Tick();
			Present();
		}
	}
	game->Shutdown();
	ShutdownGL();
	return (int)msg.wParam;
}

// EOF