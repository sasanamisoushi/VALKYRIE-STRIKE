import fs from "node:fs/promises";
import path from "node:path";
import { Presentation } from "file:///C:/Users/k024g/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs";

const root = process.cwd();
const output = path.join(root, ".codex-ppt-build", "inspect");
await fs.mkdir(output, { recursive: true });

for (const filename of [
  "artifacts/thruster_trail_technique_named_terms.pptx",
  "artifacts/thruster_trail_technique_slides.pptx",
  "artifacts/thruster_trail_visual_revision.pptx",
]) {
  const deck = await Presentation.load(path.join(root, filename));
  const stem = path.basename(filename, ".pptx");
  const text = await deck.inspect({
    select: "$.slides[*]",
    include: ["id", "index", "title", "layout", "elements.id", "elements.kind", "elements.text", "elements.alt", "elements.position"],
  });
  await fs.writeFile(path.join(output, `${stem}.ndjson`), text.ndjson, "utf8");
  const deckDir = path.join(output, stem);
  await fs.mkdir(deckDir, { recursive: true });
  for (const slide of deck.slides.items) {
    const png = await slide.export({ format: "png", scale: 1 });
    await fs.writeFile(path.join(deckDir, `slide-${slide.index + 1}.png`), new Uint8Array(await png.arrayBuffer()));
  }
  console.log(`${filename}: ${deck.slides.items.length} slides`);
}
