#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define ChebPolySum_IMPLEMENTATION
#include "chebPolySum.h"
#define MAX_POINTS 32

#include <GLFW/glfw3.h>

static void glfw_error_callback(int error, const char* description) {
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// --- Examples ---
typedef enum {
	FUNC_COS = 0,
	FUNC_SIN,
} FuncKind;

typedef struct {
	const char* label;
	int size;
	double coeffs[MAX_POINTS];
	FuncKind kind;
} WindowExample;

static const WindowExample examples[] = {
	// --- Cosine windows ---
	// https://www.mathworks.com/matlabcentral/mlc-downloads/downloads/submissions/46092/versions/3/previews/coswin.m/index.html
	{"Hanning",							2 , {0.5, -0.5}, FUNC_COS},
	{"Hamming (exact)",					2 , {25./46., -21./46.}, FUNC_COS},
	{"Hamming (optimized)",				2 , {5.383553946707251e-001, -4.616446053292749e-001}, FUNC_COS},
	{"\"Good enough\" Blackman",		3 , {0.42, -0.5, 0.08}, FUNC_COS},
	{"Blackman (exact)",				3 , {7938./18608., -9240./18608., 1430./18608.}, FUNC_COS},
	{"3 Term Cosine (Blackman-Nuttall)",3 , {4.243800934609435e-001, -4.973406350967378e-001,  7.827927144231873e-002}, FUNC_COS},
	{"Blackman-Harris",					4 , { 3.58750287312166e-001, -4.882901074726000e-001,  1.412797129705190e-001,
											 -1.16798922447150e-002}, FUNC_COS},
	{"4 Term Cosine (Blackman-Nuttall)",4 , {3.635819267707608e-001, -4.891774371450171e-001,  1.365995139786921e-001,
											-1.064112210553003e-002}, FUNC_COS},
	{"5 Term Cosine",					5 , {3.232153788877343e-001, -4.714921439576260e-001,  1.755341299601972e-001,
											-2.849699010614994e-002,  1.261357088292677e-003}, FUNC_COS},
	{"6 Term Cosine",					6 , {2.935578950102797e-001, -4.519357723474506e-001,  2.014164714263962e-001,
											-4.792610922105837e-002,  5.026196426859393e-003, -1.375555679558877e-004}, FUNC_COS},
	{"7 Term Cosine",					7 , {2.712203605850388e-001, -4.334446123274422e-001,  2.180041228929303e-001,
											-6.578534329560609e-002,  1.076186730534183e-002, -7.700127105808265e-004,
											 1.368088305992921e-005}, FUNC_COS},
	{"8 Term Cosine",					8 , {2.533176817029088e-001, -4.163269305810218e-001,  2.288396213719708e-001,
											-8.157508425925879e-002,  1.773592450349622e-002, -2.096702749032688e-003,
											 1.067741302205525e-004, -1.280702090361482e-006}, FUNC_COS},
	{"9 Term Cosine",					9 , {2.384331152777942e-001, -4.005545348643820e-001,  2.358242530472107e-001,
											-9.527918858383112e-002,  2.537395516617152e-002, -4.152432907505835e-003,
											 3.685604163298180e-004, -1.384355593917030e-005,  1.161808358932861e-007}, FUNC_COS},
	{"10 Term Cosine",					10, {2.257345387130214e-001, -3.860122949150963e-001,  2.401294214106057e-001,
											-1.070542338664613e-001,  3.325916184016952e-002, -6.873374952321475e-003,
											 8.751673238035159e-004, -6.008598932721187e-005,  1.710716472110202e-006,
											-1.027272130265191e-008}, FUNC_COS},
	{"11 Term Cosine",					11, {2.151527506679809e-001, -3.731348357785249e-001,  2.424243358446660e-001,
											-1.166907592689211e-001,  4.077422105878731e-002, -1.000904500852923e-002,
											 1.639806917362033e-003, -1.651660820997142e-004,  8.884663168541479e-006,
											-1.938617116029048e-007,  8.482485599330470e-010}, FUNC_COS},
	// --- Sine examples---
	{"sin(x)",							2 , {0.0, 1.0}, FUNC_SIN},
	{"3 sin(x) - 4 sin(3x)",			4 , {0.0, 3.0, 0.0, -4.0}, FUNC_SIN},
};
static const int examples_count = sizeof(examples) / sizeof(examples[0]);

// --- String builders ---
static void build_input_label(char* buf, size_t buf_size, int i, FuncKind kind) {
	if		(i == 0) {			snprintf(buf, buf_size, "scalar##in");}
	else if (i == 1) {
		if (kind == FUNC_COS)	snprintf(buf, buf_size, "cos(x)##in");
		else					snprintf(buf, buf_size, "sin(x)##in");
	}
	else {
		if (kind == FUNC_COS)	snprintf(buf, buf_size, "cos(%dx)##in", i);
		else					snprintf(buf, buf_size, "sin(%dx)##in", i);
	}
}

static void build_output_label(char* buf, size_t buf_size, int i) {
	if		(i == 0) snprintf(buf, buf_size, "scalar##out");
	else if (i == 1) snprintf(buf, buf_size, "cos(x)##out");
	else 			 snprintf(buf, buf_size, "cos(x)^%d##out", i);
}

static void generate_example_code(char* buf, size_t buf_size, int size, const double* outputs, FuncKind kind) {
	if (size < 1) {
		buf[0] = '\0';
		return;
	}

	int pos = 0;

	if (kind == FUNC_COS) {
		pos += snprintf(buf + pos, buf_size - pos,
						"double window_function (const double x) {\n"
						"\tconst double c = cos(2.0 * pi * x);\n"
						"\treturn ");
		for (int i = 1; i < size; i++) {
			pos += snprintf(buf + pos, buf_size - pos, "(");
		}
		pos += snprintf(buf + pos, buf_size - pos, " %.20f", outputs[size - 1]);
		for (int i = size - 2; i >= 0; i--) {
			if (outputs[i] >= 0) {
				pos += snprintf(buf + pos, buf_size - pos, "\n\t\t\t\t* c + %.20f )", +outputs[i]);
			}
			else {
				pos += snprintf(buf + pos, buf_size - pos, "\n\t\t\t\t* c - %.20f )", -outputs[i]);
			}
		}
		pos += snprintf(buf + pos, buf_size - pos, ";\n}");
	}
	else {
		pos += snprintf(buf + pos, buf_size - pos,
						"double window_function (const double x) {\n"
						"\tconst double c = cos(x);\n"
						"\tconst double s = sin(x);\n"
						"\treturn s * (");
		for (int i = 1; i < size - 2; i++) {
			pos += snprintf(buf + pos, buf_size - pos, "(");
		}
		if (size > 0) {
			pos += snprintf(buf + pos, buf_size - pos, " %.20f", outputs[size - 2]);
		}
		for (int i = size - 3; i >= 0; i--) {
			if (outputs[i] >= 0) {
				pos += snprintf(buf + pos, buf_size - pos, "\n\t\t\t\t* c + %.20f )", +outputs[i]);
			}
			else {
				pos += snprintf(buf + pos, buf_size - pos, "\n\t\t\t\t* c - %.20f )", -outputs[i]);
			}
		}
		if (size < 3) pos += snprintf(buf + pos, buf_size - pos, " )");
		pos += snprintf(buf + pos, buf_size - pos, ";\n}");
	}
}

// Main code
int WinMain(void*, void*, int, char**) {
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return -1;

	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	GLFWwindow* window = glfwCreateWindow(800, 600, "Cosine/Sine sum resolver", nullptr, nullptr);
	if (!window) {
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// State
	static int size = 2;
	static double inputs[MAX_POINTS] = {};
	static double outputs[MAX_POINTS] = {};
	static int current_example = -1;
	static int current_tab = 0; // 0 = cos, 1 = sin
	static int pending_tab = -1;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		int width, height;
		glfwGetWindowSize(window, &width, &height);
		ImGui::SetNextWindowSize(ImVec2(width, height));
		ImGui::SetNextWindowPos(ImVec2(0, 0));

		static ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
										ImGuiWindowFlags_NoResize |
										ImGuiWindowFlags_NoCollapse |
										ImGuiWindowFlags_NoDecoration;

		ImGui::Begin("Main area", 0, flags);

		// --- Top bar ---
		if (ImGui::Button("+")) {
			size = (size < MAX_POINTS) ? size + 1 : size;
			if (current_tab == 0)
				chebCosSum(size, inputs, outputs);
			else
				chebSinSum(size - 1, inputs + 1, outputs);
		}
		ImGui::SameLine();
		if (ImGui::Button("-")) {
			size = (size > 2) ? size - 1 : size;
			if (current_tab == 0)
				chebCosSum(size, inputs, outputs);
			else
				chebSinSum(size - 1, inputs + 1, outputs);
		}
		ImGui::SameLine();
		ImGui::Text("size = %-6d", size);

		ImGui::SameLine();
		ImGui::Text("Example:");
		ImGui::SameLine();

		// --- Examples drop-down list ---
		const char* preview = (current_example >= 0) ? examples[current_example].label : "Select...";
		if (ImGui::BeginCombo("##examples", preview)) {
			for (int i = 0; i < examples_count; i++) {
				bool is_selected = (current_example == i);
				if (ImGui::Selectable(examples[i].label, is_selected)) {
					current_example = i;
					size = examples[i].size;
					memcpy(inputs, examples[i].coeffs, sizeof(double) * MAX_POINTS);
					if (examples[i].kind == FUNC_COS) {
						pending_tab = 0;
						chebCosSum(size, inputs, outputs);
					}
					else {
						pending_tab = 1;
						chebSinSum(size - 1, inputs + 1, outputs);
					}
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			for (int i = 0; i < MAX_POINTS; i++) {
				inputs[i] = 0;
				outputs[i] = 0;
			}
			current_example = -1;
		}

		// End of top bar

		if (ImGui::BeginTabBar("FuncKind", ImGuiTabBarFlags_None)) {
			ImGuiTabItemFlags cos_tab_flags = 0;
			ImGuiTabItemFlags sin_tab_flags = 0;

			if (pending_tab == 0) { cos_tab_flags |= ImGuiTabItemFlags_SetSelected; pending_tab = -1; }
			if (pending_tab == 1) { sin_tab_flags |= ImGuiTabItemFlags_SetSelected; pending_tab = -1; }

			if (ImGui::BeginTabItem("cos sum", nullptr, cos_tab_flags)) {
				if (current_tab != 0) {
					current_tab = 0;
					chebCosSum(size, inputs, outputs);
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("sin sum", nullptr, sin_tab_flags)) {
				if (current_tab != 1) {
					current_tab = 1;
					chebSinSum(size - 1, inputs + 1, outputs);
				}
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		// --- Input fields ---
		ImGui::PushID(current_tab);

		ImGui::PushID("inputs");
		ImGui::Text("Enter coefficients:");
		char label_buf[64];
		FuncKind kind = (current_tab == 0) ? FUNC_COS : FUNC_SIN;
		for (int i = current_tab; i < size; i++) {
			build_input_label(label_buf, sizeof(label_buf), i, kind);
			if (ImGui::InputDouble(label_buf, &inputs[i], 0.0, 0.0, "%.20lf")) {
				if (current_tab == 0)
					chebCosSum(size, inputs, outputs);
				else
					chebSinSum(size - 1, inputs + 1, outputs);
			}
		}
		ImGui::PopID(); // "inputs"

		ImGui::Separator();

		ImGui::PushID("outputs");
		ImGui::Text("Calculated coefficients:");
		for (int i = 0; i < size - current_tab; i++) {
			build_output_label(label_buf, sizeof(label_buf), i);
			ImGui::InputDouble(label_buf, &outputs[i], 0.0, 0.0, "%.20lf", ImGuiInputTextFlags_ReadOnly);
		}
		ImGui::PopID(); // "outputs"

		ImGui::Separator();

		// --- Generated code ---
		ImGui::Text("Example code:");
		static char code_buf[8192];
		generate_example_code(code_buf, sizeof(code_buf), size, outputs, kind);
		ImGui::InputTextMultiline("##source", code_buf, strlen(code_buf) + 1,
			ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16), ImGuiInputTextFlags_ReadOnly);

		ImGui::PopID(); // current_tab

		ImGui::End();

		// Rendering
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}