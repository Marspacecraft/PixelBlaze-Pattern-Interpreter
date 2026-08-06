var speed = 0.2
export var hueShift = 0

function beforeRender(delta) {
  t = time(0.1 * speed)
}

function render(index) {
  h = t + index/pixelCount + hueShift
  s = 1
  v = 1
  hsv(h, s, v)
}