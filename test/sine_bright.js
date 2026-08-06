export function render(index) {
  var t = time(0.01);
  var pos = index / pixelCount;
  var bri = wave(pos * 6 + t); // wave = sine 0~1
  hsv(0.6, 1, bri);
}