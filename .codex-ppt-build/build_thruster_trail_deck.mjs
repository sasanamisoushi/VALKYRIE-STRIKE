import fs from "node:fs/promises";
import fsSync from "node:fs";
import path from "node:path";
import { pathToFileURL } from "node:url";
import { Presentation, PresentationFile } from "file:///C:/Users/k024g/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs";

const workspaceDir = "C:/Users/k024g/OneDrive/デスクトップ/2年/2年前期/CG2/CG2";
const skillDir = "C:/Users/k024g/.codex/plugins/cache/openai-primary-runtime/presentations/26.1015.12040/skills/presentations";
const buildDir = path.join(workspaceDir, ".codex-ppt-build");
// Keep the published output separate from private build/validation files.
const finalPath = path.join(workspaceDir, "validated-output", "thruster_trail_video_sections.validated.pptx");
const candidatePath = path.join(buildDir, "thruster_trail_candidate.pptx");
const nodePython = "C:/Users/k024g/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe";

const C = {
  ink: "#EAF6FF", muted: "#A8C1D1", cyan: "#39D7FF", magenta: "#F161D5",
  violet: "#8D72FF", navy: "#07131F", navy2: "#0B2132", surface: "#102E42",
  line: "#27536B", white: "#FFFFFF", green: "#6DE0B2", amber: "#FFC566",
};
// Artifact Tool emits Aptos for all native text boxes in this environment.
// Keeping the declared design font aligned with the emitted font makes the
// final PowerPoint portable and lets the layout validator verify the file.
const font = "Aptos";
const ASSET = {
  macrossCover: path.join(workspaceDir, ".codex-ppt-build", "previous-extract", "ppt", "media", "image2.png"),
  macrossStill: path.join(workspaceDir, ".codex-ppt-build", "previous-extract", "ppt", "media", "image1.jpg"),
  beforeStill: path.join(workspaceDir, ".codex-ppt-build", "previous-extract", "ppt", "media", "image4.jpg"),
  afterStill: "C:/Users/k024g/AppData/Local/Temp/codex-clipboard-bb33f54f-8793-43fb-80e5-f0cc77d1f4da.png",
};

function shape(slide, geometry, left, top, width, height, fill, line = "none") {
  return slide.shapes.add({
    geometry,
    position: { left, top, width, height },
    fill,
    line: line === "none" ? { style: "solid", fill: "none", width: 0 } : { style: "solid", fill: line, width: 1 },
  });
}

function text(slide, value, left, top, width, height, size, color = C.ink, bold = false, align = "left") {
  const s = shape(slide, "textbox", left, top, width, height, "none");
  s.text = value;
  s.text.style = { fontFamily: font, fontSize: size, color, bold, alignment: align, verticalAlignment: "middle" };
  return s;
}

function title(slide, value, number, size = 33) {
  text(slide, value, 68, 42, 1020, 54, size, C.white, true);
  text(slide, String(number).padStart(2, "0"), 1142, 52, 70, 32, 14, C.cyan, true, "right");
  shape(slide, "rect", 68, 105, 1144, 2, C.cyan);
}

function body(slide, lines, left, top, width, size = 20, color = C.ink) {
  const h = Math.max(50, lines.length * (size * 1.7));
  return text(slide, lines.map((v) => `• ${v}`).join("\n"), left, top, width, h, size, color, false);
}

function mediaSlot(slide, label, fileName, left, top, width, height, accent = C.cyan) {
  shape(slide, "roundRect", left, top, width, height, C.navy2, accent);
  text(slide, "VIDEO", left + 20, top + 20, 90, 24, 13, accent, true);
  text(slide, label, left + 20, top + 58, width - 40, 44, 22, C.white, true);
  text(slide, fileName, left + 20, top + height - 62, width - 40, 40, 13, C.muted, false);
  text(slide, "再生映像をここへ配置", left + 20, top + height / 2 - 6, width - 40, 30, 16, C.muted, false, "center");
}

function addLocalImage(slide, imagePath, alt, position, fit = "cover") {
  const extension = path.extname(imagePath).toLowerCase();
  const contentType = extension === ".jpg" || extension === ".jpeg" ? "image/jpeg" : "image/png";
  slide.images.add({ blob: fsSync.readFileSync(imagePath), contentType, alt, fit, position, geometry: "roundRect", borderRadius: "rounded-xl" });
}

function mediaStill(slide, label, fileName, imagePath, left, top, width, height, accent = C.cyan) {
  shape(slide, "roundRect", left, top, width, height, C.navy2, accent);
  addLocalImage(slide, imagePath, label, { left: left + 3, top: top + 3, width: width - 6, height: height - 6 });
  shape(slide, "rect", left + 3, top + 3, width - 6, 42, C.navy);
  text(slide, "VIDEO", left + 18, top + 11, 70, 18, 12, accent, true);
  text(slide, label, left + 88, top + 8, width - 112, 26, 16, C.white, true);
  shape(slide, "ellipse", left + width / 2 - 29, top + height / 2 - 29, 58, 58, C.navy, accent);
  shape(slide, "triangle", left + width / 2 - 8, top + height / 2 - 13, 26, 26, accent);
  shape(slide, "rect", left + 3, top + height - 43, width - 6, 40, C.navy);
  text(slide, fileName, left + 16, top + height - 35, width - 32, 23, 12, C.ink, false, "center");
}

function implementationMovie(slide, number, technique, focus, videoFile, accent) {
  title(slide, `実装動画：${technique}`, number);
  text(slide, "実装後の動きで、技術が画面上でどう見えるかを確認する", 70, 134, 1040, 34, 22, C.white, true);
  mediaStill(slide, "Valkyrie Strike　実装映像", videoFile, ASSET.afterStill, 170, 200, 940, 350, accent);
  text(slide, "見る点", 160, 600, 100, 26, 18, accent, true);
  text(slide, focus, 250, 597, 850, 32, 19, C.muted, true);
  note(slide, [
    `実装動画: C:/Users/k024g/Videos/Captures/${videoFile}`,
    "発表時には、枠内のサムネイルを実際の録画ファイルへ置き換える。",
  ]);
}

function dot(slide, x, y, color = C.white, size = 18) {
  shape(slide, "ellipse", x - size / 2, y - size / 2, size, size, color);
}

function label(slide, value, left, top, width, color = C.cyan) {
  shape(slide, "roundRect", left, top, width, 30, color);
  text(slide, value, left + 10, top + 1, width - 20, 26, 13, C.navy, true, "center");
}

function note(slide, lines) {
  slide.speakerNotes.text = lines;
  slide.speakerNotes.setVisible(true);
}

async function writeBlob(filePath, blob) {
  await fs.writeFile(filePath, new Uint8Array(await blob.arrayBuffer()));
}

async function main() {
  await fs.mkdir(buildDir, { recursive: true });
  await fs.mkdir(path.dirname(finalPath), { recursive: true });
  const deck = Presentation.create({ slideSize: { width: 1280, height: 720 } });

  // 1. Cover
  {
    const s = deck.slides.add();
    s.background.fill = "linear(135deg, #06111B 0%, #0B2940 58%, #251135 100%)";
    shape(s, "rect", 68, 94, 10, 450, C.cyan);
    text(s, "スラスター光跡の設計と改善", 108, 120, 900, 110, 48, C.white, true);
    text(s, "『マクロスΔスクランブル』を参考にした\nValkyrie Strike のスラスター表現", 112, 255, 740, 90, 25, C.muted, false);
    text(s, "空間履歴／リボントレイル／曲線補間／曲がる立体炎", 112, 390, 760, 36, 19, C.cyan, true);
    shape(s, "roundRect", 900, 170, 245, 250, C.navy2, C.magenta);
    text(s, "THRUSTER\nTRAIL", 930, 220, 185, 92, 29, C.magenta, true, "center");
    text(s, "技術説明スライド", 930, 338, 185, 28, 16, C.muted, false, "center");
    note(s, ["本資料では、参考ゲームの光跡を観察し、Valkyrie Strike へ実装した技術と改善の過程を説明する。"]);
  }

  // 2. Reference game
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    title(s, "題材にしたゲーム", 2);
    text(s, "マクロスΔスクランブル", 70, 146, 510, 48, 34, C.cyan, true);
    text(s, "高速移動と急旋回で、ノズルから出た光が\n空間に弧を残す表現を参考にする。", 70, 222, 560, 76, 21, C.ink, false);
    label(s, "観察する点", 70, 338, 140, C.magenta);
    text(s, "根元の強い発光\n曲がる立体炎\n時間で細く消える尾", 70, 388, 430, 122, 23, C.white, true);
    shape(s, "roundRect", 716, 145, 385, 415, C.navy2, C.magenta);
    addLocalImage(s, ASSET.macrossCover, "マクロスΔスクランブルのキービジュアル", { left: 730, top: 159, width: 357, height: 350 }, "contain");
    text(s, "参考作品のキービジュアル", 736, 516, 345, 28, 15, C.muted, false, "center");
    note(s, ["参考映像ファイル: C:/Users/k024g/Videos/Captures/(3) PS Vita「マクロスΔスクランブル」プレイ動画 永遠のワルキューレ(前編) - YouTube - Google Chrome 2026-10-15 10-54-19.mp4"]);
  }

  // 3. Comparison movie slots
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    title(s, "比較する映像", 3);
    text(s, "観察したい違いは、ノズルからの出方と急旋回時の軌跡", 70, 132, 920, 36, 21, C.muted, false);
    mediaStill(s, "参考：マクロスΔスクランブル", "参考動画（PS Vita）", ASSET.macrossStill, 70, 200, 510, 350, C.magenta);
    mediaStill(s, "実装前：Valkyrie Strike", "2026-10-17 20-03-09.mp4", ASSET.beforeStill, 700, 200, 510, 350, C.cyan);
    text(s, "比較の観点：根元のつながり／曲線の滑らかさ／幅の減衰／旋回時の折れ", 120, 605, 1040, 34, 18, C.ink, true, "center");
    note(s, ["実装前比較用: C:/Users/k024g/Videos/Captures/VALKYRIE STRIKE 2026-10-17 20-03-09.mp4", "参考用: C:/Users/k024g/Videos/Captures/(3) PS Vita「マクロスΔスクランブル」プレイ動画 永遠のワルキューレ(前編) - YouTube - Google Chrome 2026-10-15 10-54-19.mp4"]);
  }

  // 4. Target form
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    title(s, "目標とするスラスターの形", 4);
    text(s, "ノズルから出る短い高輝度の炎と、空間に残る細い光跡を連続して見せる", 70, 133, 1040, 36, 22, C.white, true);
    shape(s, "roundRect", 84, 230, 205, 120, C.surface, C.cyan);
    text(s, "ノズル根元\nフレア", 100, 260, 170, 54, 20, C.white, true, "center");
    shape(s, "rightArrow", 308, 255, 220, 70, C.magenta);
    text(s, "立体的な炎", 337, 274, 165, 30, 20, C.navy, true, "center");
    shape(s, "rightArrow", 520, 280, 210, 48, C.violet);
    text(s, "曲がる炎", 540, 287, 165, 28, 18, C.white, true, "center");
    shape(s, "rightArrow", 710, 316, 400, 32, C.cyan);
    text(s, "時間で細く消える光跡", 792, 354, 260, 30, 18, C.cyan, true, "center");
    body(s, ["炎と光跡が別の方向へ伸びない", "加速中は太く明るく、減速すると短く弱くなる", "旋回の履歴を残し、機体が通った弧を読める"], 110, 470, 1000, 20);
    note(s, ["目標は、固定の炎と光跡を別エフェクトとして離すのではなく、同じ経路を共有する噴射として描くこと。"]);
  }

  // 5. Overall pipeline
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    title(s, "実装の全体構成", 5);
    const steps = [
      ["1", "ノズル位置を記録", "ワールド空間で履歴を保存"],
      ["2", "曲線を生成", "Centripetal Catmull-Rom スプライン"],
      ["3", "リボントレイル", "履歴を帯状メッシュとして接続"],
      ["4", "スイープメッシュ", "同じ曲線へ円形断面を並べる"],
      ["5", "発光を重ねる", "フレア、UVスクロール、マッハリング"],
    ];
    steps.forEach((step, i) => {
      const x = 72 + i * 228;
      shape(s, "roundRect", x, 210, 196, 250, C.surface, i % 2 ? C.magenta : C.cyan);
      text(s, step[0], x + 16, 228, 40, 32, 19, i % 2 ? C.magenta : C.cyan, true);
      text(s, step[1], x + 16, 286, 164, 48, 21, C.white, true);
      text(s, step[2], x + 16, 360, 164, 60, 16, C.muted, false);
    });
    text(s, "重要点：リボンと立体炎が同じ曲線データを共有する", 186, 540, 900, 42, 25, C.green, true, "center");
    note(s, ["実装場所: project/3D/Trail.cpp, project/Game/Player/BoosterEffect.cpp, project/resources/shaders/SpeedTrail.PS.hlsl"]);
  }

  // 6. History sampling
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    title(s, "技術 1：リボントレイル", 6);
    text(s, "過去のノズル位置を、帯状メッシュとして空間に残す", 70, 136, 1060, 36, 25, C.white, true);
    label(s, "入力", 80, 226, 110, C.magenta);
    text(s, "ノズル位置の履歴", 80, 272, 350, 38, 24, C.ink, true);
    label(s, "生成", 80, 356, 110, C.cyan);
    text(s, "左右を結んだ帯", 80, 402, 350, 38, 24, C.ink, true);
    label(s, "消去", 80, 486, 110, C.green);
    text(s, "寿命で幅と透明度を下げる", 80, 532, 430, 38, 21, C.ink, true);
    shape(s, "roundRect", 560, 205, 590, 360, C.navy2, C.cyan);
    text(s, "ノズル口", 610, 242, 120, 26, 17, C.magenta, true);
    shape(s, "ellipse", 647, 298, 72, 72, C.magenta, C.white);
    text(s, "現在", 650, 386, 66, 25, 15, C.magenta, true, "center");
    shape(s, "roundRect", 708, 316, 122, 38, C.violet);
    shape(s, "roundRect", 816, 338, 126, 30, C.cyan);
    shape(s, "roundRect", 931, 357, 104, 22, C.cyan);
    shape(s, "roundRect", 1026, 373, 70, 14, C.green);
    dot(s, 769, 335, C.white, 15); dot(s, 880, 353, C.white, 13); dot(s, 984, 368, C.white, 11); dot(s, 1060, 380, C.white, 8);
    text(s, "新しい点ほど太く明るい", 676, 457, 250, 28, 18, C.white, true, "center");
    text(s, "過去ほど細く透明になる", 890, 457, 240, 28, 18, C.muted, false, "center");
    text(s, "距離ベースで点を追加するため、速度やFPSが変わっても帯の密度が安定する", 110, 620, 1060, 32, 20, C.green, true, "center");
    note(s, ["リボントレイルは、ノズルの過去位置の左右へ頂点を広げ、隣り合う点を結んだ帯状メッシュとして描く。今回はフレームごとではなく一定距離ごとに履歴点を追加する。高速移動で1フレームに距離を大きく進んだ場合は中間点を補い、頂点の間隔を保つ。各点には年齢を持たせ、古い点ほど幅と透明度を下げて尾を自然に消す。"]);
  }

  // 7. Ribbon trail implementation movie
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    implementationMovie(s, 7, "リボントレイル", "ノズル根元から帯が切れずにつながり、尾ほど細く消えるか", "VALKYRIE STRIKE 2026-10-17 20-03-09.mp4", C.magenta);
  }

  // 8. Curve interpolation
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    title(s, "技術 2：Centripetal Catmull-Rom スプライン", 8, 28);
    text(s, "4つの制御点の間を、滑らかな曲線に補間する", 70, 136, 1080, 36, 25, C.white, true);
    shape(s, "roundRect", 72, 208, 500, 330, C.surface, C.magenta);
    text(s, "通常の補間で起きやすい形", 98, 232, 380, 28, 18, C.magenta, true);
    dot(s, 140, 420, C.white); dot(s, 245, 320, C.white); dot(s, 360, 445, C.white); dot(s, 485, 350, C.white);
    dot(s, 205, 360, C.magenta, 13); dot(s, 286, 390, C.magenta, 13); dot(s, 312, 472, C.magenta, 13); dot(s, 405, 404, C.magenta, 13);
    text(s, "点間隔が偏ると\nふくらみ・ループが出やすい", 126, 474, 394, 42, 18, C.ink, true, "center");
    shape(s, "roundRect", 650, 208, 500, 330, C.navy2, C.cyan);
    text(s, "Centripetal Catmull-Rom", 676, 232, 390, 28, 18, C.cyan, true);
    dot(s, 718, 420, C.white); dot(s, 821, 320, C.white); dot(s, 940, 445, C.white); dot(s, 1060, 350, C.white);
    dot(s, 773, 374, C.cyan, 13); dot(s, 864, 366, C.cyan, 13); dot(s, 942, 404, C.cyan, 13); dot(s, 1009, 392, C.cyan, 13);
    text(s, "点の間を自然につなぐ\n過度なふくらみを抑える", 702, 474, 394, 42, 18, C.ink, true, "center");
    text(s, "ノズル口と仮想制御点を先頭に置くことで、根元から曲線として接続できる", 110, 607, 1060, 32, 20, C.green, true, "center");
    note(s, ["Catmull-Romスプラインは、P0からP3までの4点を参照し、P1とP2の間を通る曲線を求める補間法。通常の均一パラメータでは点の間隔が偏ると曲線が大きくふくらんだり、ループしたりすることがある。Centripetal方式は点間距離に応じてパラメータを決めるため、この不自然なふくらみを抑えやすい。実装では現在のノズル口と排気方向に置いた仮想制御点を先頭へ加え、根元にも同じ補間を適用する。"]);
  }

  // 9. Curve interpolation implementation movie
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    implementationMovie(s, 9, "Centripetal Catmull-Rom スプライン", "急旋回しても軌跡が折れず、根元から自然な弧になるか", "VALKYRIE STRIKE 2026-10-17 19-39-46.mp4", C.cyan);
  }

  // 10. Curved 3D flame
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    title(s, "技術 3：スプライン・スイープメッシュ", 10);
    text(s, "曲線に沿って断面を並べ、曲がる立体炎を作る", 70, 136, 1080, 36, 25, C.white, true);
    const groups = [
      ["形状", "円形断面を連結", C.violet],
      ["質感", "外炎・中間層・白い芯", C.magenta],
      ["破綻防止", "平行移動フレーム", C.cyan],
    ];
    groups.forEach((group, i) => {
      const x = 78 + i * 280;
      shape(s, "roundRect", x, 220, 246, 134, C.surface, group[2]);
      text(s, group[0], x + 18, 244, 210, 28, 20, group[2], true, "center");
      text(s, group[1], x + 18, 294, 210, 28, 17, C.ink, true, "center");
    });
    shape(s, "roundRect", 935, 202, 230, 330, C.navy2, C.violet);
    text(s, "断面を曲線へ配置", 955, 224, 190, 24, 16, C.violet, true, "center");
    [288, 336, 388, 444].forEach((y, i) => {
      const x = 973 + i * 38;
      shape(s, "ellipse", x, y, 66 - i * 7, 66 - i * 7, C.violet, C.white);
      shape(s, "ellipse", x + 15 - i * 2, y + 15 - i * 2, 36 - i * 5, 36 - i * 5, C.white);
    });
    text(s, "スプラインに沿う\n立体チューブ", 960, 470, 182, 40, 17, C.muted, false, "center");
    text(s, "Parallel Transport Frame が断面の向きを滑らかに引き継ぎ、急旋回時の反転とねじれを抑える", 104, 600, 1000, 34, 19, C.green, true, "center");
    note(s, ["スプライン・スイープメッシュは、中心となる曲線に断面形状を沿わせ、断面同士をつないで立体メッシュを作る方法。今回は10角形の断面を連続配置し、外炎・中間層・白い芯を重ねる。曲がった経路では断面の向きを毎回固定軸から決めると反転するため、Parallel Transport Frameで前の断面の向きを次の接線へ投影して引き継ぐ。"]);
  }

  // 11. Sweep mesh implementation movie
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    implementationMovie(s, 11, "スプライン・スイープメッシュ", "炎の立体感を保ったまま、曲線に沿って向きが変わるか", "VALKYRIE STRIKE 2026-10-17 15-20-31.mp4", C.violet);
  }

  // 12. Shader layers
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    title(s, "技術 4：発光、流れ、消え方", 12);
    text(s, "メッシュに重ねる4つの見え方", 70, 136, 850, 36, 25, C.white, true);
    const parts = [["根元", "加算ビルボード", "強い発光", C.magenta], ["内部", "UVスクロール", "流れる模様", C.cyan], ["コア", "マッハリング", "周期的な明部", C.violet], ["末端", "ライフタイム・テーパー", "細く透明になる", C.green]];
    parts.forEach((p, i) => {
      const y = 208 + i * 93;
      label(s, p[0], 76, y + 14, 100, p[3]);
      text(s, p[1], 214, y, 300, 34, 22, C.white, true);
      text(s, p[2], 214, y + 42, 270, 24, 16, C.muted, false);
      shape(s, "roundRect", 582, y + 16, 420 - i * 58, 38 - i * 5, p[3]);
      dot(s, 650 + i * 40, y + 35, C.white, 14 - i * 2);
    });
    text(s, "発光の層を重ねることで、ノズル根元から尾までを一続きの噴射として見せる", 110, 607, 1000, 32, 20, C.green, true, "center");
    note(s, ["加算ビルボードはカメラに正対する発光面で、ノズル根元を強く見せ、3Dモデルとの境目をなじませる。UVスクロールは光跡内部のテクスチャ座標を時間で動かし、静止した空間履歴にも流れを与える。マッハリングはコア層の周期的な明部で推力の強さを示す。ライフタイム・テーパーは各履歴点の経過時間から幅と透明度を下げ、尾を自然に消す。"]);
  }

  // 13. Shader implementation movie
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    implementationMovie(s, 13, "発光、流れ、消え方", "根元の強い発光と、尾へ流れて消える見え方が出ているか", "LE3B_13_ササナミ_ソウシ_VALKYRIESTRIKE.mp4", C.green);
  }

  // 14. Right angle issue
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    title(s, "直角に見える問題と改善", 14);
    shape(s, "roundRect", 72, 164, 500, 390, C.surface, C.magenta);
    text(s, "Before　NG", 102, 190, 220, 32, 25, C.magenta, true);
    text(s, "固定の直線炎 ＋ 履歴", 102, 232, 250, 24, 16, C.muted, false);
    shape(s, "roundRect", 156, 300, 54, 142, C.magenta);
    shape(s, "roundRect", 200, 420, 236, 48, C.magenta);
    dot(s, 182, 292, C.white, 15); dot(s, 264, 444, C.white, 15); dot(s, 340, 444, C.white, 12); dot(s, 407, 444, C.white, 9);
    text(s, "方向が急に切り替わる", 122, 500, 400, 26, 19, C.white, true, "center");
    shape(s, "roundRect", 710, 164, 500, 390, C.navy2, C.cyan);
    text(s, "After　OK", 740, 190, 220, 32, 25, C.cyan, true);
    text(s, "共通の曲線データ", 740, 232, 250, 24, 16, C.muted, false);
    const afterCurve = [[798, 301, 82, 62], [844, 337, 106, 54], [930, 378, 124, 43], [1027, 408, 96, 31]];
    afterCurve.forEach((part, i) => {
      shape(s, "ellipse", part[0], part[1], part[2], part[3], i === 0 ? C.violet : C.cyan);
      dot(s, part[0] + part[2] / 2, part[1] + part[3] / 2, C.white, Math.max(8, 15 - i * 2));
    });
    text(s, "根元から連続した弧になる", 760, 500, 400, 26, 19, C.white, true, "center");
    text(s, "リボントレイルと立体炎が同じ曲線を共有することで、境目と直角感を同時に抑える", 90, 607, 1100, 32, 20, C.green, true, "center");
    note(s, ["改善前は固定の直線炎と、空間履歴から作る光跡を別々に扱っていたため、両者の方向が切り替わる位置で直角のような折れ目が見えた。改善後はノズル口を共通の起点にし、仮想制御点を含むCentripetal Catmull-Rom曲線を生成する。リボントレイルとスプライン・スイープメッシュの両方へ同じ曲線を渡すことで、根元から一続きの弧として描画する。"]);
  }

  // 15. Result movie slot
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    title(s, "完成後の検証映像", 15);
    text(s, "急旋回時に、根元から尾までが一続きに曲がるかを確認する", 70, 134, 1050, 36, 22, C.white, true);
    mediaStill(s, "Valkyrie Strike　検証用プレイ映像", "動画を挿入して再生する", ASSET.afterStill, 170, 202, 940, 350, C.green);
    text(s, "確認項目　根元の接続　／　曲線の自然さ　／　幅の減衰　／　急旋回時のねじれ", 130, 615, 1020, 32, 19, C.muted, true, "center");
    note(s, ["このページの画像は、更新後のゲーム画面を使った動画サムネイル。発表時にはPowerPointの［挿入］→［ビデオ］→［このデバイス］から、更新後の録画ファイルをこの枠へ追加する。比較映像と同じ操作条件で急旋回、上昇、ロールを行うと、曲線の連続性を確認しやすい。"]);
  }

  // 16. Outlook
  {
    const s = deck.slides.add({ background: { fill: C.navy } });
    title(s, "今後の展望", 16);
    text(s, "スラスター表現をゲーム全体へ自然に広げる", 70, 145, 1050, 42, 28, C.cyan, true);
    const outlook = [
      ["01", "変形後のスラスター位置調整", "各形態のノズル位置を調整し、噴射の起点をモデルへ正確に合わせる", C.magenta],
      ["02", "ボス攻撃を正式版へ", "攻撃の予兆とエフェクトを整え、戦闘中の見やすさを上げる", C.cyan],
      ["03", "被弾アラートを追加", "被弾の方向と危険度が分かる表示と音を追加する", C.green],
    ];
    outlook.forEach((item, index) => {
      const y = 244 + index * 108;
      text(s, item[0], 98, y, 70, 38, 19, item[3], true);
      shape(s, "rect", 184, y + 3, 5, 74, item[3]);
      text(s, item[1], 220, y - 2, 720, 34, 24, C.white, true);
      text(s, item[2], 220, y + 38, 820, 28, 17, C.muted, false);
    });
    text(s, "プレイ中に気持ちよく読み取れる演出へ仕上げる", 100, 614, 1080, 32, 21, C.green, true, "center");
    note(s, ["今後は、変形後のノズル位置を調整して噴射の起点を正確に合わせる。ボス攻撃は予兆、弾道、着弾の表現を整えて正式版へ更新する。さらに被弾時のアラートを追加し、プレイヤーが危険を把握しやすい戦闘演出を作る。"]);
  }

  const montage = await deck.export({ format: "webp", montage: true });
  await writeBlob(path.join(buildDir, "thruster_trail_montage.webp"), montage);
  for (const [index, slide] of deck.slides.items.entries()) {
    await writeBlob(path.join(buildDir, `slide-${String(index + 1).padStart(2, "0")}.png`), await slide.export({ format: "png", scale: 1 }));
  }

  await (await PresentationFile.exportPptx(deck)).save(candidatePath);
  const { finalizePresentation } = await import(pathToFileURL(path.join(skillDir, "container_tools", "artifact_tool_utils.mjs")).href);
  await finalizePresentation({
    workspaceDir,
    candidatePath,
    finalPath,
    pythonExecutable: nodePython,
    integrityValidatorPath: path.join(skillDir, "container_tools", "inspect_presentation_package_integrity.py"),
    layoutValidatorPath: path.join(skillDir, "container_tools", "inspect_presentation_layout_geometry.py"),
    layoutArgs: ["--expected-slide-size-emu", "12192000,6858000", "--validate-bullet-geometry", "--validate-heading-fit"],
    fontPolicy: { basis: "design", families: [font] },
    verifyArtifactToolImport: true,
    receiptPath: path.join(buildDir, "thruster_trail.validation.json"),
  });
}

main().catch((error) => { console.error(error); process.exitCode = 1; });
