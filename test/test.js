// 最小数组测试
buf = array(pixelCount)

export function beforeRender(delta) {
  for(var i=0;i<pixelCount;i++){
    buf[i] = i / pixelCount + time(0.2)
  }
}

export function render(index) {
  var v = buf[index]
  hsv(v,1,v)
}