#include "include/texture/texture_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <sstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <string>

#include <glad/gl.h>
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"
#include "linmath.h"

using namespace std;
namespace {
	static const struct
	{
		float x, y;
		float r, g, b;
	} vertices[3] =
	{
		{ -0.6f, -0.4f, 1.f, 0.f, 0.f },
		{  0.6f, -0.4f, 0.f, 1.f, 0.f },
		{   0.f,  0.6f, 0.f, 0.f, 1.f }
	};

	static const char* vertex_shader_text =
		"#version 110\n"
		"uniform mat4 MVP;\n"
		"attribute vec3 vCol;\n"
		"attribute vec2 vPos;\n"
		"varying vec3 color;\n"
		"void main()\n"
		"{\n"
		"    gl_Position = MVP * vec4(vPos, 0.0, 1.0);\n"
		"    color = vCol;\n"
		"}\n";

	static const char* fragment_shader_text =
		"#version 110\n"
		"varying vec3 color;\n"
		"void main()\n"
		"{\n"
		"    gl_FragColor = vec4(color, 0.2);\n"
		"}\n";

	static void error_callback(int error, const char* description)
	{
		fprintf(stderr, "Error: %s\n", description);
	}

	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	void printOpenGLErrorIfHave() {
		GLenum error = 1;
		while (error != 0)
		{
			error = glGetError();
			if (error != GL_NO_ERROR)
				std::cout << "opengl error: " << error << std::endl;
		}
	}

	double GetMonitorScaleFactor(int x=0, int y=0) {
		const POINT target_point = { (LONG)x, (LONG)y };
		HMONITOR monitor = MonitorFromPoint(target_point, MONITOR_DEFAULTTONEAREST);
		UINT dpi = FlutterDesktopGetDpiForMonitor(monitor);
		double scale_factor = dpi / 96.0;

		return scale_factor;

		//return 1.0;
	}

	string uchar2String(const unsigned char* c) {
		if (!c) {
			return "";
		}

		return string(reinterpret_cast<const char*>(c));
	}

	std::string GetOpengGLInfo() {
		string vendor = uchar2String(glGetString(GL_VENDOR));
		string renderer = uchar2String(glGetString(GL_RENDERER));
		string version = uchar2String(glGetString(GL_VERSION));
		//GLubyte* glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
		string extVersion = uchar2String(glGetString(GL_EXTENSIONS));

		string str = string("vendor: ") + vendor + string(" | renderer: ") + 
			renderer + string(" | version: ") + version + 
			string(" | ext version: ") + extVersion;

		return str;
	}

	LRESULT CALLBACK WndProc(HWND const window,
		UINT const message,
		WPARAM const wparam,
		LPARAM const lparam) {
		return DefWindowProc(window, message, wparam, lparam);
	}

	const wchar_t* registerWindowClass(const wchar_t* name) {
		WNDCLASS window_class{};
		window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
		window_class.lpszClassName = name;
		window_class.style = CS_HREDRAW | CS_VREDRAW;
		window_class.cbClsExtra = 0;
		window_class.cbWndExtra = 0;
		window_class.hInstance = GetModuleHandle(nullptr);
		window_class.hIcon =
			LoadIcon(window_class.hInstance, MAKEINTRESOURCE(101));
		window_class.hbrBackground = 0;
		window_class.lpszMenuName = nullptr;
		window_class.lpfnWndProc = WndProc;
		RegisterClass(&window_class);

		return name;
	}

	HWND createMainWindow(int width, int height) {
		const wchar_t* window_class = registerWindowClass(L"test_main_window");
		HWND window = CreateWindow(
			window_class, L"hello", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			0, 0,
			width, height,
			nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

		HDC device_context = GetDC(window);
		PIXELFORMATDESCRIPTOR pfd = {};
		pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cDepthBits = 24;
		pfd.cStencilBits = 8;

		int pixel_format = ChoosePixelFormat(device_context, &pfd);
		SetPixelFormat(device_context, pixel_format, &pfd);

		LONG Style = GetWindowLong(window, GWL_EXSTYLE);
		SetWindowLong(window, GWL_EXSTYLE, Style | WS_EX_LAYERED);
		SetLayeredWindowAttributes(window, 0, 255, LWA_ALPHA);

		return window;
	}

	std::mutex openg_mutex;

	GLFWwindow* setupOpenGL(int width, int height) {
		glfwSetErrorCallback(error_callback);

		if (!glfwInit())
			exit(EXIT_FAILURE);

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
		glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
		glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
		//glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

		GLFWwindow* window;
		window = glfwCreateWindow(width, height, "Simple example", NULL, NULL);
		if (!window)
		{
			glfwTerminate();
			exit(EXIT_FAILURE);
		}

		glfwMakeContextCurrent(window);
		gladLoadGL(glfwGetProcAddress);

		std::cout << GetOpengGLInfo() << std::endl;

		printOpenGLErrorIfHave();

		glfwSetKeyCallback(window, key_callback);

		glfwMakeContextCurrent(nullptr);

		return window;
	}

	void startRender(GLFWwindow* window, int64_t texture_id, flutter::TextureRegistrar* tex_registrar) {
		auto thread_id = std::this_thread::get_id();
		std::cout << "opengl renderer thread id: " << thread_id << std::endl;

		GLuint vertex_buffer, vertex_shader, fragment_shader, program;
		GLint mvp_location, vpos_location, vcol_location;

		openg_mutex.lock();
		glfwMakeContextCurrent(window);
		glfwSwapInterval(1);


		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glGenBuffers(1, &vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
		glCompileShader(vertex_shader);

		fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
		glCompileShader(fragment_shader);

		program = glCreateProgram();
		glAttachShader(program, vertex_shader);
		glAttachShader(program, fragment_shader);
		glLinkProgram(program);

		mvp_location = glGetUniformLocation(program, "MVP");
		vpos_location = glGetAttribLocation(program, "vPos");
		vcol_location = glGetAttribLocation(program, "vCol");

		glEnableVertexAttribArray(vpos_location);
		glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE,
			sizeof(vertices[0]), (void*)0);
		glEnableVertexAttribArray(vcol_location);
		glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE,
			sizeof(vertices[0]), (void*)(sizeof(float) * 2));

		glfwMakeContextCurrent(NULL);
		openg_mutex.unlock();

		while (!glfwWindowShouldClose(window))
		{
			float ratio;
			int width, height;
			mat4x4 m, p, mvp;

			auto start = std::chrono::high_resolution_clock::now();
			//openg_mutex.lock();
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double, std::milli> duration = end - start;
			std::cout << "renderer get opengl lock: " << duration.count() << " ms" << std::endl;

			glfwMakeContextCurrent(window);

			glfwGetFramebufferSize(window, &width, &height);
			ratio = width / (float)height;

			glViewport(0, 0, width, height);
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);

			mat4x4_identity(m);
			mat4x4_rotate_Z(m, m, (float)glfwGetTime());
			mat4x4_ortho(p, -ratio, ratio, -1.f, 1.f, 1.f, -1.f);
			mat4x4_mul(mvp, p, m);

			glUseProgram(program);
			glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const GLfloat*)mvp);
			glDrawArrays(GL_TRIANGLES, 0, 3);

			glfwSwapBuffers(window);

			glfwPollEvents();
			printOpenGLErrorIfHave();

			glfwMakeContextCurrent(NULL);

			//openg_mutex.unlock();

			duration = std::chrono::high_resolution_clock::now() - end;
			std::cout << "renderer cost: " << duration.count() << " ms" << std::endl;

			//tex_registrar->MarkTextureFrameAvailable(texture_id);

			//std::this_thread::sleep_for(std::chrono::milliseconds(20));

		}
	}

	class CopyTextureBufferCallback
	{
	public:
		CopyTextureBufferCallback(GLFWwindow* window);
		~CopyTextureBufferCallback();

		FlutterDesktopPixelBuffer* operator()(size_t width, size_t height);

	private:
		GLFWwindow* _window;
		FlutterDesktopPixelBuffer* _pixelBuffer;
		std::chrono::steady_clock::time_point _last_frame_start_time;
	};

	CopyTextureBufferCallback::CopyTextureBufferCallback(GLFWwindow* window): _pixelBuffer(nullptr), _window(window) {
		
	}

	CopyTextureBufferCallback::~CopyTextureBufferCallback()
	{
		if (!_pixelBuffer)
			return;

		if (_pixelBuffer->buffer)
			delete[] _pixelBuffer->buffer;

		delete _pixelBuffer;
	}

	FlutterDesktopPixelBuffer* CopyTextureBufferCallback::operator()(size_t width, size_t height) {
		auto thread_id = std::this_thread::get_id();
		std::cout << "copy buffer thread id: " << thread_id << std::endl;

		auto current_frame_start_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> milli_of_last_frame = current_frame_start_time - _last_frame_start_time;
		double fps = 1000.0 / milli_of_last_frame.count();
		std::cout << "FPS: " << fps << "	frame milli seconds: " << milli_of_last_frame.count() << std::endl;

		_last_frame_start_time = current_frame_start_time;

		if (!_pixelBuffer) {
			_pixelBuffer = new FlutterDesktopPixelBuffer();
		}
		
		double scale_factor = GetMonitorScaleFactor();
		size_t w = static_cast<size_t>(width * scale_factor);
		size_t h = static_cast<size_t>(height * scale_factor);

		if (_pixelBuffer->width != w || _pixelBuffer->height != h) {
			_pixelBuffer->width = w;
			_pixelBuffer->height = h;

			if (_pixelBuffer->buffer)
				delete[] _pixelBuffer->buffer;
	
			_pixelBuffer->buffer = new uint8_t[w * h * 4];
		}

		openg_mutex.lock();

		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - current_frame_start_time;
		std::cout << "copy callback get opengl lock cost: " << elapsed.count() << " ms\n";

		glfwMakeContextCurrent(_window);
		glReadBuffer(GL_BACK);
		glReadPixels(0, 0, (GLsizei)w, (GLsizei)h, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)_pixelBuffer->buffer);

		elapsed = std::chrono::high_resolution_clock::now() - end;
		std::cout << "copying buffer cost: " << elapsed.count() << " ms\n";

		printOpenGLErrorIfHave();

		glfwMakeContextCurrent(nullptr);

		openg_mutex.unlock();

		return _pixelBuffer;
	}

	class TexturePlugin : public flutter::Plugin {
	public:
		static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

		TexturePlugin();

		virtual ~TexturePlugin();

		flutter::TextureRegistrar* texture_registrar;

	private:
		// Called when a method is called on this plugin's channel from Dart.
		void HandleMethodCall(
			const flutter::MethodCall<flutter::EncodableValue>& method_call,
			std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
	};

	// static
	void TexturePlugin::RegisterWithRegistrar(
		flutter::PluginRegistrarWindows* registrar) {
		auto channel =
			std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
				registrar->messenger(), "textureFactory",
				&flutter::StandardMethodCodec::GetInstance());

		auto plugin = std::make_unique<TexturePlugin>();
		plugin->texture_registrar = registrar->texture_registrar();

		channel->SetMethodCallHandler(
			[plugin_pointer = plugin.get()](const auto& call, auto result) {
			plugin_pointer->HandleMethodCall(call, std::move(result));
		});

		registrar->AddPlugin(std::move(plugin));
	}

	TexturePlugin::TexturePlugin() {}

	TexturePlugin::~TexturePlugin() {}

	void TexturePlugin::HandleMethodCall(
		const flutter::MethodCall<flutter::EncodableValue>& method_call,
		std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
		if (method_call.method_name().compare("getPlatformVersion") == 0) {
			std::ostringstream version_stream;
			version_stream << "Windows ";
			if (IsWindows10OrGreater()) {
				version_stream << "10+";
			}
			else if (IsWindows8OrGreater()) {
				version_stream << "8";
			}
			else if (IsWindows7OrGreater()) {
				version_stream << "7";
			}
			result->Success(flutter::EncodableValue(version_stream.str()));
		}
		else if (method_call.method_name().compare("newTexture") == 0) {
			auto thread_id = std::this_thread::get_id();
			std::cout << "plugin new texture thread id: " << thread_id << std::endl;

			size_t w = 0, h = 0;

			const auto* arguments = std::get_if<flutter::EncodableMap>(method_call.arguments());
			if (arguments) {
				auto width = arguments->find(flutter::EncodableValue("width"));
				if (width != arguments->end()) {
					w = (size_t)std::get<int>(width->second);
				}
				auto height = arguments->find(flutter::EncodableValue("height"));
				if (height != arguments->end()) {
					h = (size_t)std::get<int>(height->second);
				}
			}

			double scale_factor = GetMonitorScaleFactor();
			auto window = setupOpenGL((int)(w * scale_factor), (int)(h * scale_factor));

			CopyTextureBufferCallback callback(window);
			flutter::TextureVariant* pixelBuffer = new flutter::TextureVariant(flutter::PixelBufferTexture(callback));
			auto tex_id = this->texture_registrar->RegisterTexture(pixelBuffer);

			auto render_thread = std::thread(startRender, window, tex_id, this->texture_registrar);
			render_thread.detach();
			
			return result->Success(flutter::EncodableValue(0));
		}
		else {
			result->NotImplemented();
		}
	}

}  // namespace

void TexturePluginRegisterWithRegistrar(
	FlutterDesktopPluginRegistrarRef registrar) {
	auto thread_id = std::this_thread::get_id();
	std::cout << "plugin registrar thread id: " << thread_id << std::endl;
	TexturePlugin::RegisterWithRegistrar(
		flutter::PluginRegistrarManager::GetInstance()
		->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
