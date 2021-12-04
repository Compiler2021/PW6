# PW6 实验报告

> PB19020583 李晗驰
>
> PB19000002 孙策 
>
> PB19030888 张舒恒

## 问题回答

##### 思考题1-1：

针对`while`语句，LLVM IR代码会有十分明显的特点，可以将布局其分为四块，按顺序列出

1. `while`语句上文，以`br`指令无条件跳转到`while`语句的`bool`表达式块为结尾
2. `while`语句`bool`表达式部分，结尾`br`指令指明表达式判断为`true`和`false`分别要跳转到的代码块
3. `while`语句内部代码块部分，当第2部分判断为`true`，通过`br`指令跳转至此，结尾`br`指令无条件跳转到第2部分，即`while`语句`bool`表达式部分，这里的`br`指令内容很有意思，在下面展开回答
4. `while`语句下文，当第2部分判断为`false`，通过`br`指令跳转至此，控制流离开`while`语句执行部分，执行下文的程序。

以`demo`生成的`.ll`文件相关语句为例

```llvm
br label %14, !llvm.loop !4

!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.mustprogress"}
```

`!llvm.loop.*`是LLVM IR中的元数据(metadata)，由于LLVM IR没有表示loop的基本架构，所以使用元数据来表示loop，并以此可以进行包括控制流转移，变量增加固定数值等在内的一系列基础操作，而如何寻找loop中的最具代表性的指令来赋予元数据呢，最终选择了分支指令的最末尾，而这个元数据所对应的“序号”也恰好将不同的loop分隔开来，方便对程序运行的控制。

比如，新增一个`while`语句，我们出了在两个`while`的结尾`br`指令发现一个元数据外，还会在`.ll`文件的末尾看到如下代码:

```llvm
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.mustprogress"}
!6 = distinct !{!6, !5}
```

显然`!4`和`!6`区分了两个`while`语句，又由于他们都是`while`语句，因此都要执行`!5`操作

同样的对`for`语句，可能看到这种:

```llvm
br i1 %exitcond, label %._crit_edge, label %.lr.ph, !llvm.loop !0

!0 = distinct !{ !0, !1 }
!1 = !{ !"llvm.loop.unroll.count", i32 4 }
```

`!1`也包含一种对应的操作。

##### 思考题1-2：

举例说明，`func_hand.ll`使用的函数调用语句为：

```llvm
%6 = call i32 @add(i32 %4, i32 %5)  ; funcall ret add(a, b)
```

其中使用指令为`call`，指令后指明函数返回参数的类型，再往后则指定对应的函数，以及要传入的实参。

传入实参部分要指明参数的类型和来源变量。

函数返回是什么类型，则等号左边的数据就会是对应类型，在这个例子中，类型为`i32`。

直接调用`%6`就可以进行计算和操作，不过针对`demo`中类似的部分，如果有`a = call(c,d)`之类的操作，会将`%6`存入到“分配”给`a`的变量中，将返回值绑定到`a`参数

## 实验设计

### InitVal

​	如果当前InitVal是Expr类型，则直接访问。如果当前InitVal是InitVal嵌套类型，则记录嵌套深度initval_depth++，遍历node.elementList。如果常值表达式const_expr是int类型，则将其值转换成float类型存入array_inital；如果是float类型，则直接存入array_inital。

### FuncDef

​	遍历参数表中的每个参数，如果参数是指针类型，则调用PointerType::get获取指针类型存入参数类型数组params；如果参数不是指针类型，则直接从TypeMap中读入类型信息存入参数类型数组params。

​	根据函数参数类型数组和函数返回类型建立函数类型，根据函数类型和函数名建立函数，将函数加入符号表，建立函数基本块。进入作用域，访问函数的参数表param_list，遍历函数体body，如果遇到ReturnStmt则不编译其后面语句，如果函数没有终止指令，则给函数增加一条返回语句，退出作用域。

### FuncFParamList

​	当前的函数是函数表中最后一个函数，由此获取函数参数值表，遍历函数形参表，判断参数是不是指针类型并相应地获取类型分配空间存入参数的值。

### FuncParam

​	遍历参数的数组维度列表，如果表达式Expr不空，则进行访问。

### EmptyStmt

​	不用处理

### ExprStmt

​	直接进行访问

### ReturnStmt

​	返回语句为空则生成空返回语句；否则先对表达式进行求值，然后根据求出值的类型以及当前函数的返回值类型判断是否需要进行类型转换，最后生成返回语句

### AssignStmt

​	先计算右式的值，接着获取左值的地址与类型，判断是否需要进行类型转换，最后生成 store 语句

### UnaryCondExpr

​	计算右式的值，接着根据值的类型与 0 或 0.0 进行比较，获取结果

### BinaryCondExpr

​	先判断是否为逻辑运算符，如果是则进行短路计算；否则分别计算出左式与右式的值，进行类型转换和比较

### UnaryExpr

​	计算右式的值，接着判断是否为 `-` ，是的话使用 0 或 0.0 减去右式的值；否则值不变

### LVal

​	根据名字查符号表获取左值的值和地址

### BinaryExpr

​	分别计算出左式与右式的值，进行类型转换和运算

### Literal

​	根据字面量类型生成 CONST_INT 或 CONST_FLOAT

### FuncCallStmt

​	通过函数名在符号表中找到函数，对每个形参类型进行分类为int，float，pointer，并相应求值，将结果放到实参集合 params 中，同时与形参进行类型比较，判断是否需要类型转换。调用create_call调用函数，并将实参集合传递给形参表。

### IfStmt

​	如果没有else_statement，则访问条件表达式cond_exp，根据其值设置条件跳转，真跳转到 trueBB基本块，假跳转到nextBB基本块。建立trueBB断点，访问if_statement，跳转到nextBB；建立nextBB断点。

​	如果有else_statement，则访问条件表达式cond_exp，根据其值设置条件跳转，真跳转到 trueBB基本块，假跳转到falseBB基本块。建立trueBB断点，访问if_statement，如果if_statement的最后语句不是终止指令则说明终止指令在nextBB中，跳转到nextBB；如果if_statement的最后语句是终止指令则去除nextBB。建立falseBB断点，访问else_statement，如果else_statement的最后语句不是终止指令则说明终止指令在nextBB中，跳转到nextBB。建立nextBB断点。

### WhileStmt

​	强制跳转到condBB，建立condBB断点，访问条件表达式cond_exp，根据其值设置条件跳转，真跳转到 trueBB基本块，假跳转到falseBB基本块。建立trueBB断点，将当前condBB和falseBB分别存入tmp_condbb和tmp_falsebb(方便BreakStmt和ContinueStmt)，访问循环体statement，跳转到condBB，建立falseBB断点。最后tmp_condbb和tmp_falsebb栈顶出栈以清除已退出循环的condBB或falseBB。

### BreakStmt

​	跳转到tmp_falsebb栈顶基本块。

### ContinueStmt

​	跳转到tmp_condbb栈顶基本块。

## 实验难点及解决方案

### 如何实现短路计算

​	以 `||` 为例，先分配一个临时变量 tmp，接着计算左式的值，再将左式的值与 0 或 0.0 进行比较，并把比较结果赋给 tmp，为真则跳转到 next ，为假则跳转到 false，在 false 块内，tmp会加上右式的值与 0 或 0.0 比较的结果，接着跳转到 next 块中，next 块判断 tmp 的值是否大于 0 ，只要大于 0 就代表整个表达式的值为真

### 如何实现编译期计算常量表达式

​	构建一个全局变量 `const_expr` ，其存有常量表达式计算的中间值以及类型，同时还有全局变量 `const_int_var` 和 `const_float_var` ，用来存放全局 const 变量的名字与值。然后在遍历 AST 遇到在全局作用域内的表达式时，就计算它们的值，并存到 `const_expr ` 中

### 如何实现数组的左值运算

​	在 LVal 的计算中，假设遇到了 `a[0]`，那么对 a 是指针（表明 a 是函数参数）和 a 是数组 （局部声明或者全局声明的数组）进行判断并分开处理；如果遇到了 `a` 并且发现 a 是个指针或者数组，那表明 a 是作为函数实参传入的，这时就要分开并正确生成指向 a 中第一个元素地址的指针类型

## 实验总结

## 实验反馈

## 组间交流
