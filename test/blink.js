export var speed = 0.01;

function beforeRender(delta) {
  t = time(speed);
  //log("t =", t, "speed =", speed);
}

function render(index) {
    
  if (t < 0.5) {
    //log("t =", t, "index =", index)
    hsv(0, 1, 1);   // 红
  } else {
    //log("t =", t, "index =", index)
    hsv(0.6, 1, 1); // 蓝
  }
}