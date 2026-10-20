import fs from "node:fs/promises";
import path from "node:path";
import { Presentation } from "file:///C:/Users/k024g/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs";

const input = "C:/Users/k024g/Downloads/LE3B_12_ササナミ_ソウシ_基盤システム.pptx";
const output = "C:/Users/k024g/OneDrive/デスクトップ/2年/2年前期/CG2/CG2/.codex-ppt-build/previous-deck";
const deck = await Presentation.load(input);
await fs.mkdir(output, { recursive: true });
const montage = await deck.export({ format: "webp", montage: true, scale: 1 });
await fs.writeFile(path.join(output, "montage.webp"), Buffer.from(await montage.arrayBuffer()));
for (const [index, slide] of deck.slides.items.entries()) {
  const image = await slide.export({ format: "png", scale: 1 });
  await fs.writeFile(path.join(output, `slide-${String(index + 1).padStart(2, "0")}.png`), Buffer.from(await image.arrayBuffer()));
}
const inspect = await deck.inspect({
  select: "$.slides[*]",
  include: ["id", "index", "title", "elements.id", "elements.kind", "elements.text", "notes.text"],
  fileName: path.join(output, "content.ndjson"),
});
console.log(JSON.stringify({ slides: deck.slides.items.length, inspection: inspect.path }));
