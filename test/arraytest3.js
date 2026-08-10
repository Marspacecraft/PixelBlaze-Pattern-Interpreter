export var arrayLen = 16
export var speed = 0.3

var numArray = array(arrayLen)
var hueBuffer = array(pixelCount)
var nestedArr = array(3)
nestedArr[0] = array(4)
nestedArr[1] = array(4)
nestedArr[2] = array(4)

function initArrays() {
    var i
    for (i = 0; i < arrayLen; i++) {
        numArray[i] = i / arrayLen
    }

    var y, x
    for (y = 0; y < 3; y++) {
        for (x = 0; x < 4; x++) {
            nestedArr[y][x] = (y + x) * 0.25
        }
    }
}

initArrays()

// 每帧更新调色板（在 render 之前执行）
function beforeRender() {
    var t = time(speed)   // 用于流动，此处也可以不用，但保留原意
    var i

    // 调色板整体缓慢旋转
    for (i = 0; i < arrayLen; i++) {
        numArray[i] = frac(numArray[i] + 0.002)
    }

    // 把调色板映射到灯带，整体流动
    for (i = 0; i < pixelCount; i++) {
        var pos = (i / pixelCount) + t
        var arrIndex = floor(frac(pos) * arrayLen)
        hueBuffer[i] = numArray[arrIndex]
    }

    // 测试越界扩容（不影响主效果）
    numArray[99] = wave(t)
    // 修改二维嵌套数组（不影响主效果）
    nestedArr[1][2] = frac(t)
}

// 逐像素渲染（标准入口）
function render(index) {
    hsv(hueBuffer[index], 1, 1)
}