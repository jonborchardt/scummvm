# Roger generation pipeline (`gen/`)

The in-engine **omyac** art-generation pipeline: it turns SCI resources into the
hires plates, VIEW cels, and priority maps that Roger presents through the OSystem
overlay. Nothing here reads or writes the display overlay — these units produce
surfaces; `roger_compositor` and `file_roger_art_provider` consume them.

| File | Role |
|------|------|
| `roger_asset_gen.{h,cpp}` | Generation orchestrator: `generatePlate()`, `generateViewCel()`, `generatePriorityMap()`, `generateTextSurface()`; content-hash disk cache (`kTransformVersion`-keyed) |
| `roger_omyac.{h,cpp}` | Enhance passes (fill / line / all) → RGBA plate |
| `roger_pic_parser.{h,cpp}` | Parse an SCI pic resource into draw commands |
| `roger_pic_native.{h,cpp}` | Native pre-render (exposes `NativeRef::priority`) |
| `roger_scale.{h,cpp}` | scale2x/3x/6x nearest scalers |
| `roger_view_scaler.{h,cpp}` | Registry of VIEW-cel upscaler modules (entry 0 = shipping 6x) |
| `roger_ega_blend.{h,cpp}` | Precomputed okLab-mixed EGA color table |
| `roger_passes.{h,cpp}` | Pass-string parse/stamp + pass-edit ops; `kDefaultPassString` |
| `slice_set.{h,cpp}` | SCI0 EGA priority-band helpers |
| `roger_byte_reader.h` | Big-endian byte cursor over the pic bitstream |
| `roger_draw_command.h` | Decoded pic/cel command structs |

These units are deliberately **SCI-engine-type-free** so they unit-test in the
minimal CxxTest runner (`build_tests.ps1`).

**Cache-invalidation contract (READ BEFORE EDITING):** cache files are validated by
NAME only. Any change to the generated output of these files must bump
`kTransformVersion` in the same commit (history line appended at the constant), or
the commit message must state "output bit-identical; kTransformVersion unchanged"
with a `roger_gen_mode=always` byte-compare as proof. See the full discipline in
`CLAUDE.md` → "Cache invalidation discipline".
