#include <cstdio>
#include <Windows.h>
#include <cstdint>
#include <random>
#include <chrono>

#define _USE_MATH_DEFINES
#include <math.h>

#include "QuadTree.h"

void UpdateMainWindow(WndState* state, HDC hdc);
void Render(WndState* state);
void Update(QuadTree *state, double delta_time);
void ResizeDIBSection(WndState* state, long width, long height, long bits_per_pixel);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void Init(QuadTree *&quad_tree);

int constexpr DEFAULT_WIN_WIDTH = 1500, DEFAULT_WIN_HEIGHT = 1500;
int constexpr BITS_PER_PIXEL = 4 * sizeof(uint8_t);
wchar_t const *kAppName = L"Barnes Hut";

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev_inst, LPSTR cmd_line, int show)
{
	WNDCLASS wind_class;
	wind_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wind_class.lpfnWndProc = WindowProc;
	wind_class.cbClsExtra = 0;
	wind_class.cbWndExtra = 0;
	wind_class.hInstance = inst;
	wind_class.hIcon = NULL;
	wind_class.hCursor = NULL;
	wind_class.hbrBackground = 0;
	wind_class.lpszMenuName = kAppName;
	wind_class.lpszClassName = kAppName;

	RegisterClass(&wind_class);

	WndState app_state;
	ResizeDIBSection(&app_state, DEFAULT_WIN_WIDTH, DEFAULT_WIN_HEIGHT, BITS_PER_PIXEL);

	HWND hwnd = CreateWindow(wind_class.lpszClassName,
		kAppName,
		WS_VISIBLE | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		DEFAULT_WIN_WIDTH,
		DEFAULT_WIN_HEIGHT,
		NULL,
		NULL,
		wind_class.hInstance,
		&app_state);

	if (!hwnd)
	{
		printf("Error: Could not create window!");
		return -1;
	}

	ShowWindow(hwnd, show);
	app_state.running = true;

	Init(app_state.state);

	auto previous = std::chrono::high_resolution_clock::now();

	while (app_state.running)
	{

		MSG msg;
		while (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) app_state.running = false;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		auto now = std::chrono::high_resolution_clock::now();
		auto delta_time = std::chrono::duration<double>(now - previous).count() / 1000;
		previous = now;

		app_state.state->UpdateVelocities(app_state.state, delta_time, 1);
		app_state.state->UpdatePositions(delta_time);
		Render(&app_state);

		HDC hdc = GetDC(hwnd);

		UpdateMainWindow(&app_state, hdc);

		ReleaseDC(hwnd, hdc);
	}

	return 0;
}

void UpdateMainWindow(WndState* state, HDC hdc)
{
	StretchDIBits(hdc,
		0,
		0,
		state->bitmap_info.bmiHeader.biWidth,
		state->bitmap_info.bmiHeader.biHeight,
		0,
		0,
		state->bitmap_info.bmiHeader.biWidth,
		state->bitmap_info.bmiHeader.biHeight,
		state->debug_bitmap_memory,
		&state->bitmap_info,
		DIB_RGB_COLORS,
		SRCCOPY);
}

void Render(WndState* state)
{
	uint32_t* pixel = (uint32_t*)state->bitmap_memory;
	//memset(pixel, 0, 4 * DEFAULT_WIN_WIDTH * DEFAULT_WIN_HEIGHT);

	for (int i = 0; i < DEFAULT_WIN_WIDTH * DEFAULT_WIN_HEIGHT; ++i)
	{
		uint8_t *rgb = reinterpret_cast<uint8_t*>(&pixel[i]);
		rgb[2] = static_cast<uint8_t>(rgb[2] * 0.99);
		rgb[0] = static_cast<uint8_t>(min(0.1 * rgb[2] + 0.98 * rgb[0], 255));
		rgb[1] = static_cast<uint8_t>(min(0.1 * rgb[0] + 0.98 * rgb[1], 255));
	}

	auto particles = state->state->ToVector();

	memcpy(state->debug_bitmap_memory, state->bitmap_memory, 4 * DEFAULT_WIN_WIDTH * DEFAULT_WIN_HEIGHT);

	// Uncomment to display QuadTree
	//state->state->Draw(*state, { -1, 1, 1, -1 });

	for (auto *particle : particles)
	{
		int x = static_cast<int>((particle->pos.x + 1.0) / 2.0 * DEFAULT_WIN_WIDTH);
		if (x < 0) continue;
		if (x >= DEFAULT_WIN_WIDTH) continue;
		int y = static_cast<int>((particle->pos.y + 1.0) / 2.0 * DEFAULT_WIN_HEIGHT);
		if (y < 0) continue;
		if (y >= DEFAULT_WIN_HEIGHT) continue;
		pixel[x + DEFAULT_WIN_WIDTH * y] = 0x00FF0000;
	}

	/*for (auto* particle : particles)
	{
		int x = static_cast<int>((particle->pos.x + 1.0) / 2.0 * DEFAULT_WIN_WIDTH);
		if (x < 0) continue;
		if (x >= DEFAULT_WIN_WIDTH) continue;
		int y = static_cast<int>((particle->pos.y + 1.0) / 2.0 * DEFAULT_WIN_HEIGHT);
		if (y < 0) continue;
		if (y >= DEFAULT_WIN_HEIGHT) continue;
		pixel[x + DEFAULT_WIN_WIDTH * y] += (pixel[x + DEFAULT_WIN_WIDTH * y] != 0x00FF0000) * 0x00010000;
	}*/

	//for (int y = 0; y < state->bitmap_info.bmiHeader.biHeight; ++y)
	//{
	//	for (int x = 0; x < state->bitmap_info.bmiHeader.biWidth; ++x)
	//	{
	//		if (x >= 0 && x < 984 && y >= 40 && y < 1000) *pixel = 0x00FFFFFF;
	//		pixel++;
	//	}
	//}
}

void ResizeDIBSection(WndState* state, long width, long height, long bits_per_pixel)
{
	if (state->bitmap_memory) VirtualFree(state->bitmap_memory, 0, MEM_RELEASE);
	if (state->debug_bitmap_memory) VirtualFree(state->debug_bitmap_memory, 0, MEM_RELEASE);

	state->bitmap_info.bmiHeader.biSize = sizeof(state->bitmap_info.bmiHeader);
	state->bitmap_info.bmiHeader.biWidth = width;
	state->bitmap_info.bmiHeader.biHeight = height;
	state->bitmap_info.bmiHeader.biPlanes = 1;
	state->bitmap_info.bmiHeader.biBitCount = 32;
	state->bitmap_info.bmiHeader.biCompression = BI_RGB;
	state->bitmap_info.bmiHeader.biSizeImage = width * height & 32;

	state->bitmap_memory = VirtualAlloc(NULL, width * height * bits_per_pixel, MEM_COMMIT, PAGE_READWRITE);
	state->debug_bitmap_memory = VirtualAlloc(NULL, width * height * bits_per_pixel, MEM_COMMIT, PAGE_READWRITE);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WndState* state;
	if (uMsg == WM_CREATE)
	{
		CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
		state = (WndState*)pCreate->lpCreateParams;
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
	}
	else
	{
		state = (WndState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	}

	switch (uMsg)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		state->running = false;
		return 0;
	}
	case WM_CLOSE:
	{
		DestroyWindow(hwnd);
		return 0;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		UpdateMainWindow(state, hdc);

		EndPaint(hwnd, &ps);
		return 0;
	}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void Init(QuadTree *&quad_tree)
{
	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_real_distribution<double> dist(0, 1);
	quad_tree = new QuadTree({-1.0, 1.0, 1.0, -1.0});

	size_t num_particles = 1000;
	Particle *particles = new Particle[num_particles];

	for (int i = 0; i < num_particles - 2; ++i)
	{
		double r = dist(rng) * 0.5 + 0.5;
		double t = 2 * M_PI * dist(rng);
		double v = 6 * (dist(rng) + 9.0);
		double m = dist(rng) * 5 + 5;
		particles[i] = { {r * cos(t), r * sin(t)}, {v * cos(t + M_PI / 2), v * sin(t + M_PI / 2)}, m };
		quad_tree->insert(&particles[i]);
	}
	particles[num_particles - 2] = { {-0.1, 0}, {0,  80}, 5000 };
	particles[num_particles - 1] = { { 0.1, 0}, {0, -80}, 5000 };
	quad_tree->insert(&particles[num_particles - 2]);
	quad_tree->insert(&particles[num_particles - 1]);
}