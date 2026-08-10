// 简单数组测试
var a = array(3)          // 声明长度为3的数组
a[0] = 1.1
a[1] = 2.2
a[2] = 3.3

var b = a                // 引用传递（别名）
b[1] = 9.9               // 应修改 a[1]

var c = array(4.4, 5.5, 6.6)  // 数组字面量
c[0] += 1.0              // 复合赋值

// 输出关键值（如果支持 console_log）
// log("a[0]=", a[0], " a[1]=", a[1], " a[2]=", a[2])
// log("c[0]=", c[0])

// 渲染函数（仅用于触发执行，不影响测试）
export function render(index) {
    hsv(index / pixelCount, 1, 0.5)
}