#pragma once

class Sprite;
class WarpExitPresentationState;

// ワープ出口からの出撃を知らせる MISSION START スプライトを描画する。
class MissionStartOverlayRenderer {
public:
	void Draw(Sprite* sprite, const WarpExitPresentationState& presentation) const;
};
