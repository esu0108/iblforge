// FrameScheduler.h : イベント駆動描画の「いつ描くか / どれだけ眠るか」の判断だけを持つ (Win32 非依存の純 C++)
#pragma once

class FrameScheduler
{
public:
	static constexpr int      kTrailingFrames = 15;          // 入力後に続けて描画するフレーム数 (ホバー等のアニメーションとレイアウト確定のため)
	static constexpr int      kTaaTrailingFrames = 48;       // TAA 有効時: 履歴が収束する (16 サンプルのジッタが混ざり切る) まで描き続ける
	static constexpr unsigned kInfinite = 0xFFFFFFFFu;       // 無期限に待つ (Win32 の INFINITE と同値)

	// メッセージ (入力・リサイズ・再描画要求・ドロップなど) を受け取った
	void NoteMessage() { m_framesLeft = Trailing(); }

	// 1 フレーム描画した。ボタン押しっぱなし中は追加メッセージが来ないが、ImGui のキーリピートのため毎フレーム必要
	void NoteFrameRendered( bool anyMouseDown )
	{
		if( m_framesLeft > 0 ) --m_framesLeft;
		if( anyMouseDown ) m_framesLeft = Trailing();
	}

	// TAA の有効 / 無効 (入力後に続けるフレーム数が変わる)
	void SetTaaActive( bool active ) { m_taaActive = active; }

	// 描画せずに眠ってよいか
	bool ShouldSleep( bool minimized, bool continuous ) const { return minimized || ( !continuous && m_framesLeft == 0 ); }

	// 眠る長さ (ms)。ベイク / 読み込み中は進捗表示のため 50ms、テキスト入力中はカーソル点滅のため 250ms、それ以外は入力が来るまで
	static unsigned SleepTimeoutMs( bool minimized, bool busy, bool textInput )
	{
		if( minimized ) return kInfinite;
		return busy ? 50u : ( textInput ? 250u : kInfinite );
	}

	int FramesLeft() const { return m_framesLeft; }

private:
	int Trailing() const { return m_taaActive ? kTaaTrailingFrames : kTrailingFrames; }

	int m_framesLeft = 30; // 起動直後も描画する
	bool m_taaActive = true;
};
