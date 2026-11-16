#include "app/ViewInput.h"

#include <imgui.h>

void ProcessCameraInput( OrbitCamera& camera, const ViewMouseState& mouse )
{
	const ImGuiIO& io = ImGui::GetIO();
	if( mouse.active && ImGui::IsMouseDown( ImGuiMouseButton_Left ) ) camera.Orbit( io.MouseDelta.x, io.MouseDelta.y );
	if( mouse.hovered && io.MouseWheel != 0.0f ) camera.Zoom( io.MouseWheel );
}

void ProcessShortcuts( AppModel& model, const BakeWorkflow& workflow )
{
	if( ImGui::GetIO().WantTextInput ) return;
	if( ImGui::IsKeyPressed( ImGuiKey_Tab, false ) ) model.view.showUi = !model.view.showUi;
	const bool running = workflow.IsBaking();
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_O ) && !workflow.IsLoading() ) model.requests.open = true;
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_I ) && !running ) model.requests.import = true;
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_S ) && model.results.hasBaked && !running ) model.requests.save = true;
}
