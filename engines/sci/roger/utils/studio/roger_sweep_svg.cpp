/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "sci/roger/utils/studio/roger_sweep_svg.h"
#include "common/base64.h"
#include "common/memstream.h"
#include "sci/roger/png_loader.h"

namespace Sci {
namespace Roger {

namespace {

// Standalone drag script (PicSweep.tsx STANDALONE_SWEEP_SCRIPT, verbatim).
// It deliberately contains no '<', '>', or '&' so it can embed as a plain
// XML text node. Do not restyle or "fix" it.
const char *kStandaloneSweepScript = R"SVGJS(
(function () {
  var svg = null;
  var cs = document.currentScript;
  if (cs) svg = cs.ownerSVGElement || cs.parentNode;
  if (!svg) svg = document.querySelector('svg');
  if (!svg) return;
  var reveal = svg.querySelector('#picSweepRevealRect');
  var handle = svg.querySelector('#picSweepHandle');
  if (!reveal || !handle) return;
  var viewBox = svg.viewBox.baseVal;
  var minX = 0;
  var maxX = viewBox.width;
  var viewH = viewBox.height;
  var dragging = false;

  function toSvgX(evt) {
    var pt = svg.createSVGPoint();
    pt.x = evt.clientX; pt.y = evt.clientY;
    var ctm = svg.getScreenCTM();
    if (!ctm) return null;
    return pt.matrixTransform(ctm.inverse()).x;
  }
  function clampNum(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }
  function setSplit(x) {
    var nx = clampNum(x, minX, maxX);
    reveal.setAttribute('width', String(nx));
    handle.setAttribute('transform', 'translate(' + nx + ' 0)');
  }
  function stopAutoAnim() {
    var nodes = svg.querySelectorAll('animate, animateTransform');
    nodes.forEach(function (n) { n.remove(); });
  }

  handle.addEventListener('pointerdown', function (e) {
    stopAutoAnim();
    dragging = true;
    try { handle.setPointerCapture(e.pointerId); } catch (_) {}
    var x = toSvgX(e);
    if (x !== null) setSplit(x);
  });
  svg.addEventListener('pointermove', function (e) {
    if (!dragging) return;
    e.preventDefault();
    var x = toSvgX(e);
    if (x !== null) setSplit(x);
  });
  function stop() { dragging = false; }
  svg.addEventListener('pointerup', stop);
  svg.addEventListener('pointercancel', stop);

  // Re-affirm cursor + selection behavior in case host viewer doesn't inherit
  // the inline styles consistently.
  handle.style.cursor = 'ew-resize';
  svg.style.userSelect = 'none';
  svg.style.touchAction = 'none';
  // Suppress unused param warning when viewH is not consumed elsewhere.
  void viewH;
})();
)SVGJS";

// Reference geometry is authored at 1920 wide; scale proportionally, never 0.
int scaleDim(int v1920, int w) {
	const int v = v1920 * w / 1920;
	if (v < 1) {
		return 1;
	}
	return v;
}

// SMIL timing shared by the width animate and the handle animateTransform.
const char *kSweepTiming =
	"dur=\"6s\" repeatCount=\"indefinite\" calcMode=\"spline\" "
	"keyTimes=\"0;0.5;1\" keySplines=\"0.42 0 0.58 1;0.42 0 0.58 1\"";

} // namespace

Common::String buildSweepSvgFromPngData(const byte *pngLeft, uint32 lenLeft,
                                        const byte *pngRight, uint32 lenRight,
                                        int width, int height, bool animated) {
	if (!pngLeft || !lenLeft || !pngRight || !lenRight || width <= 0 || height <= 0) {
		return Common::String();
	}

	// b64EncodeData takes a non-const pointer but only reads.
	const Common::String b64Left =
		Common::b64EncodeData(const_cast<byte *>(pngLeft), lenLeft);
	const Common::String b64Right =
		Common::b64EncodeData(const_cast<byte *>(pngRight), lenRight);

	const int sweepMin = scaleDim(120, width);
	const int sweepMax = scaleDim(1800, width);
	const int split0 = animated ? sweepMin : width / 2;
	// Handle geometry (reference values at 1920: grab 80, line 6, r 36,
	// chevron 12/16/22 stroke 5).
	const int grabHalf = scaleDim(40, width);
	const int lineW = scaleDim(6, width);
	const int circleR = scaleDim(36, width);
	const int chA = scaleDim(12, width);
	const int chB = scaleDim(16, width);
	const int chC = scaleDim(22, width);
	const int chevW = scaleDim(5, width);
	// Label pills (reference: 220x40 rx 6, inset 16, bottom offset 56,
	// text baseline offset 28, font 22).
	const int pillW = scaleDim(220, width);
	const int pillH = scaleDim(40, width);
	const int pillRx = scaleDim(6, width);
	const int inset = scaleDim(16, width);
	const int bottomOff = scaleDim(56, width);
	const int textOff = scaleDim(28, width);
	const int fontPx = scaleDim(22, width);

	Common::String s;
	s += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	s += Common::String::format(
		"<svg xmlns=\"http://www.w3.org/2000/svg\" "
		"xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
		"viewBox=\"0 0 %d %d\" style=\"display:block\">\n", width, height);

	// Clip rect: the reveal window over the left (A) image.
	s += "<defs><clipPath id=\"picSweepReveal\">";
	s += Common::String::format(
		"<rect id=\"picSweepRevealRect\" x=\"0\" y=\"0\" width=\"%d\" height=\"%d\">",
		split0, height);
	if (animated) {
		s += Common::String::format(
			"<animate attributeName=\"width\" values=\"%d;%d;%d\" %s/>",
			sweepMin, sweepMax, sweepMin, kSweepTiming);
	}
	s += "</rect></clipPath></defs>\n";

	// Right (B) image full-frame first, then left (A) clipped above it.
	const char *imgFmt =
		"<image href=\"data:image/png;base64,%s\" x=\"0\" y=\"0\" "
		"width=\"%d\" height=\"%d\" preserveAspectRatio=\"none\" "
		"style=\"image-rendering:pixelated\"/>\n";
	s += Common::String::format(imgFmt, b64Right.c_str(), width, height);
	s += "<g clip-path=\"url(#picSweepReveal)\">\n";
	s += Common::String::format(imgFmt, b64Left.c_str(), width, height);
	s += "</g>\n";

	// Slider handle: grab rect, divider line, circle, chevrons.
	s += Common::String::format(
		"<g id=\"picSweepHandle\" transform=\"translate(%d 0)\" "
		"style=\"cursor:ew-resize\">\n", split0);
	if (animated) {
		// First child so it precedes the visible handle parts (reference rule).
		s += Common::String::format(
			"<animateTransform attributeName=\"transform\" type=\"translate\" "
			"values=\"%d 0;%d 0;%d 0\" %s/>\n",
			sweepMin, sweepMax, sweepMin, kSweepTiming);
	}
	s += Common::String::format(
		"<rect x=\"-%d\" y=\"0\" width=\"%d\" height=\"%d\" fill=\"transparent\"/>\n",
		grabHalf, grabHalf * 2, height);
	s += Common::String::format(
		"<line x1=\"0\" y1=\"0\" x2=\"0\" y2=\"%d\" stroke=\"#111827\" "
		"stroke-width=\"%d\"/>\n", height, lineW);
	s += Common::String::format(
		"<circle cx=\"0\" cy=\"%d\" r=\"%d\" fill=\"#111827\"/>\n",
		height / 2, circleR);
	s += Common::String::format(
		"<path d=\"M -%d -%d L -%d 0 L -%d %d M %d -%d L %d 0 L %d %d\" "
		"transform=\"translate(0 %d)\" fill=\"none\" stroke=\"#fff\" "
		"stroke-width=\"%d\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n",
		chA, chB, chC, chA, chB, chA, chB, chC, chA, chB, height / 2, chevW);
	s += "</g>\n";

	// Label pills. Kept AFTER the base64 blobs on purpose: editing the labels
	// in the exported file means scrolling to the bottom, not past megabytes.
	s += "<!-- ==================== EDIT LABELS HERE ==================== -->\n";
	s += "<!-- Change the text between the two text tags below.           -->\n";
	s += "<g pointer-events=\"none\" font-family=\"system-ui, sans-serif\" "
	     "font-weight=\"700\">\n";
	s += Common::String::format(
		"<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"%d\" "
		"fill=\"rgba(0,0,0,0.6)\"/>\n",
		inset, height - bottomOff, pillW, pillH, pillRx);
	s += Common::String::format(
		"<text x=\"%d\" y=\"%d\" text-anchor=\"middle\" fill=\"#fff\" "
		"font-size=\"%d\">A</text>\n",
		inset + pillW / 2, height - textOff, fontPx);
	s += Common::String::format(
		"<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"%d\" "
		"fill=\"rgba(0,0,0,0.6)\"/>\n",
		width - inset - pillW, height - bottomOff, pillW, pillH, pillRx);
	s += Common::String::format(
		"<text x=\"%d\" y=\"%d\" text-anchor=\"middle\" fill=\"#fff\" "
		"font-size=\"%d\">B</text>\n",
		width - inset - pillW / 2, height - textOff, fontPx);
	s += "</g>\n";

	// Standalone interaction script last (reference appends it last too).
	s += "<script>";
	s += kStandaloneSweepScript;
	s += "</script>\n</svg>\n";
	return s;
}

Graphics::Surface *downscaleNearest(const Graphics::Surface &src, int divisor) {
	if (divisor < 1 || src.w < divisor || src.h < divisor) {
		return nullptr;
	}
	Graphics::Surface *out = new Graphics::Surface();
	out->create(src.w / divisor, src.h / divisor, src.format);
	const int bpp = src.format.bytesPerPixel;
	for (int y = 0; y < out->h; y++) {
		for (int x = 0; x < out->w; x++) {
			memcpy(out->getBasePtr(x, y), src.getBasePtr(x * divisor, y * divisor), bpp);
		}
	}
	return out;
}

Common::String buildSweepSvg(const Graphics::Surface &left,
                             const Graphics::Surface &right,
                             int divisor, bool animated) {
	if (left.w != right.w || left.h != right.h) {
		return Common::String();
	}
	Graphics::Surface *ls = downscaleNearest(left, divisor);
	Graphics::Surface *rs = downscaleNearest(right, divisor);
	Common::String out;
	if (ls && rs) {
		Common::MemoryWriteStreamDynamic pl(DisposeAfterUse::YES);
		Common::MemoryWriteStreamDynamic pr(DisposeAfterUse::YES);
		if (encodeSurfacePng(*ls, pl) && encodeSurfacePng(*rs, pr)) {
			out = buildSweepSvgFromPngData(pl.getData(), static_cast<uint32>(pl.size()),
			                               pr.getData(), static_cast<uint32>(pr.size()),
			                               ls->w, ls->h, animated);
		}
	}
	if (ls) {
		ls->free();
		delete ls;
	}
	if (rs) {
		rs->free();
		delete rs;
	}
	return out;
}

} // namespace Roger
} // namespace Sci
