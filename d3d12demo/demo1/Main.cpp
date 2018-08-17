#include <Windows.h>
#include <wrl/client.h>
#include <memory>
#include <stdexcept>

#include "Demo.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "D3DCompiler.lib")

using namespace std;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	const LONG width = 800;
	const LONG height = 600;
	const UINT bufferCount{ 3 };

	shared_ptr<Demo> demo;

	try {
		demo = make_shared<Demo>(bufferCount, "Hello World!", width, height);
	}
	catch (runtime_error& err) {
		MessageBox(nullptr, err.what(), "Error", MB_OK);
		return 0;
	}


	MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	while (msg.message != WM_QUIT) {
		BOOL r{ PeekMessage(&msg, NULL,0,0,PM_REMOVE) };

		if (r == 0) {
			try {

				demo->render();
			}
			catch (runtime_error& err) {
				MessageBox(nullptr, err.what(), "Error", MB_OK);
			}
		}
		else {
			DispatchMessage(&msg);
		}
	}

	return 0;
}