#include "app/RenderSync.h"

#include "i18n/I18n.h"

#include <vector>

void SyncRenderer( AppModel& model, Renderer& renderer )
{
	// ---- アンチエイリアス (TAA): UI の設定をレンダラーへ反映
	renderer.SetTaaEnabled( model.view.taaEnabled );

	// ---- プレビュー (トーンマップ → テクスチャ)
	HdrDocument& hdr = model.hdr;
	if( hdr.previewDirty && hdr.preview.w > 0 )
	{
		hdr.previewDirty = false;
		std::vector<uint8_t> rgba;
		ibl::TonemapPreview( hdr.preview, hdr.previewEV, rgba );
		std::string err;
		if( !renderer.SetPreviewImage( rgba.data(), hdr.preview.w, hdr.preview.h, err ) ) model.status = i18n::Fmt( i18n::Str::PreviewFailed, err.c_str() );
	}
}
