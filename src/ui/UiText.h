// UiText.h : 翻訳で長くなってもパネルからはみ出さないためのテキスト補助 (折り返し / ラベル列の幅)
#pragma once
#include <imgui.h>

#include <algorithm>
#include <initializer_list>

namespace ui
{
	// 無効色 / 指定色の文字を、パネル幅で折り返して表示する (TextDisabled / TextColored は折り返さない)
	inline void TextDisabledWrapped( const char* text )
	{
		ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_TextDisabled ) );
		ImGui::TextWrapped( "%s", text );
		ImGui::PopStyleColor();
	}
	inline void TextColoredWrapped( const ImVec4& color, const char* text )
	{
		ImGui::PushStyleColor( ImGuiCol_Text, color );
		ImGui::TextWrapped( "%s", text );
		ImGui::PopStyleColor();
	}

	// スライダー / コンボなどの「右側のラベル」に必要な幅 (px)。PushItemWidth( -LabelColumnWidth( ... ) ) で使う。
	// 最も長いラベルに合わせ、入力欄が狭くなりすぎないよう、利用可能幅の 55% までに制限する。
	inline float LabelColumnWidth( std::initializer_list<const char*> labels, float dpiScale )
	{
		float w = 0.0f;
		for( const char* l : labels ) w = std::max( w, ImGui::CalcTextSize( l ).x );
		w += ImGui::GetStyle().ItemInnerSpacing.x;
		w = std::max( w, 90.0f * dpiScale );
		return std::min( w, ImGui::GetContentRegionAvail().x * 0.55f );
	}
} // namespace ui
