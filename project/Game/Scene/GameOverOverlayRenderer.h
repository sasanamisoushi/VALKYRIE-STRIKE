#pragma once

class GameOverSequence;

// ゲームオーバー演出中の警告オーバーレイを描画する。
class GameOverOverlayRenderer {
public:
	void Draw(const GameOverSequence& sequence, float screenWidth, float screenHeight) const;
};
