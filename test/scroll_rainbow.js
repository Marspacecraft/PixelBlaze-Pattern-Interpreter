export function render(index) {
  var t = time(0.02);
  var pos = index / pixelCount;
  hsv(pos + t, 1, 0.8);
}