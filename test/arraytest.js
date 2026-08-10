export var testResult = "Running..."

// 1. 一维数组声明与初始化
var arr1 = array(5)          // 长度为5的空数组
for (var i = 0; i < 5; i++) {
    arr1[i] = i * 0.2        // 初始化为 0, 0.2, 0.4, 0.6, 0.8
}

// 2. 数组字面量初始化
var arr2 = [1.0, 2.0, 3.0, 4.0, 5.0]

// 3. 数组引用（别名）
var alias = arr1
alias[2] = 9.9               // 应修改 arr1[2]

// 4. 复合赋值
arr1[0] += 0.1               // 变为 0.1
arr2[3] *= 2                 // 变为 8.0

// 5. 嵌套数组
var nested = array(3)
nested[0] = array(2)
nested[1] = array(2)
nested[2] = array(2)
nested[0][0] = 1.1
nested[0][1] = 2.2
nested[1][0] = 3.3
nested[1][1] = 4.4
nested[2][0] = 5.5
nested[2][1] = 6.6

// 6. 数组越界（自动扩容）
arr1[10] = 7.7               // 应扩容至11个元素

// 7. 使用数组值参与运算
var sum = 0.0
for (var i = 0; i < 5; i++) {
    sum += arr1[i]
}
// sum 应为 0.1 + 0.2 + 9.9 + 0.6 + 0.8 = 11.6

// 8. 数组元素作为函数返回值（内置函数）
var arr3 = array(3)
arr3[0] = sin(0.5)
arr3[1] = cos(0.5)
arr3[2] = floor(2.9)

// 9. 导出变量供外部检查（若支持）
export var checkArr1 = arr1
export var checkArr2 = arr2
export var checkAlias = alias
export var checkNested = nested[0][0]  // 取第一个元素
export var checkSum = sum
export var checkArr3 = arr3

// 10. 渲染函数（仅用于触发执行，不影响测试）
export function render(index) {
    // 将测试结果以颜色显示（可选）
    var t = index / pixelCount
    hsv(t, 1, 0.5)
}
