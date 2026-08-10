var t1;
var hue = 0;
var sign = 0;
var delay = 0;

export function beforeRender(delta) {
  t1 = time(0.02);

  if (sign = ~sign) {
    t1 = 1-t1;
    hue = 0.6
  }
  else {
    hue = 0
  }
  t1 = pixelCount * t1;
}

export function render(index) {
  var x = index - t1;
  x = (x * x) / (pixelCount * 5);
  hsv(hue, 1, 1-x)
}