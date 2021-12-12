# PW6 实验报告

> PB19020583 李晗驰
>
> PB19000002 孙策 
>
> PB19030888 张舒恒

## 实验要求

第一关要求我们先熟悉 LLVM IR，然后根据给出的 sy 文件写出对应的 IR 代码；第二关要求我们熟悉框架的各种 API 然后写出可以生成第一个写的 IR 代码的生成器；第三关要求我们完整地实现一个 IR 中间代码生成器

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

##### 思考题2-1：

两条指令参与计算的索引个数不同。

因为指针的类型不同，第一个 `getelementptr` 的指针类型是 `int (*)[10]` ，第一个索引 `i32 0` 得到 第一个 `int [10]` 的地址，第二个索引 `i32 0` 得到第一个 `int [10]` 中第一个 `int` 的地址；第二个 `getelementptr` 的指针类型是 `int *`，第一个索引得到第一个 `int` 的地址

##### 思考题3-1：

​	"%2 = getelementptr [10 x i32], [10 x i32]* %1, i32 0, i32 %0"以int类型4字节寻址，即数组为%1[10]，获取%1[%0]的地址

​	"%2 = getelementptr i32, i32* %1 i32 %0"以单字节寻址，获取指针%1偏移%0个字节的地址

## 实验设计

### Assembly 

​	初始化类型转换TypeMap

### VarDef

​	变量定义有几个关键分支点：

- 变量是否为全局变量，这决定了我们调用`GlobalVariable`还是调用`get_alloc`

- 变量元素的类型，`int`与`float`
- 变量的类型，是普通变量或是数组，数组是一维或是多维
- 是否赋初值，赋初值则要进行相应的处理

针对非数组变量，其处理相对简单，局部变量只需要调用`get_alloc`和`create_store`，或者全局变量调用`GlobalVariable`的API来建立对应的全局变量。

针对数组变量，不赋初值时的处理均较为简洁，下面讨论赋初值的情况。

- 对于一维数组，其全局变量赋初值较为简单，因为API设计的较为方便。而局部变量需要使用一个一个`create_store`的方法赋值，才比较稳健。通过来自`InitVal`的`Vector`存储的信息，可以很好的进行赋值

- 对于多维数组，这是件麻烦事，先来讨论局部变量：

  - 局部变量要先分配空间，然而目前没有合适的`Type`来表示多维数组，于是参照一维数组的类型组织方式，修改源文件以增加多维数组类型：

  ```c++
  class MultiDimensionArrayType : public Type {
  public:
      MultiDimensionArrayType(Type *contained, std::vector<int> elements_array, unsigned dimension);
  
      static bool is_valid_element_type(Type *ty);
  
      static MultiDimensionArrayType *get(Type *contained, std::vector<int> elements_array, unsigned dimension);
  
      Type *get_element_type() const { return contained_; }
      
      unsigned get_num_of_dimension() const { return dimension; }
      
      std::vector<int> get_dim_vec() const { return elements_array;}
      
      unsigned get_num_of_elements_by_dimension(int dimension) const { return elements_array[dimension]; } 
  private:
      Type *contained_;
      std::vector<int> elements_array; 
      unsigned dimension;
  };
  ```

  这里的要点是整个类必须存储足够的信息，并可以提供相应的`API`来获取这些信息

  具体的实现不在此处赘述。

  要使的`get_alloc`函数支持多维数组，其主要在于修改`print()`函数，增加多维数组类型的case，来正确输出。

  - 其次是多维数组的赋值，我们在`InitVal`结点用一个`vector<vector<float>>`来存储相应的赋值数据。并另外有一个`vector<int>`存储每一维的长度

    这样整个赋值过程就变成了进制计算，其中每位的进制与维度长度有关，将正确的指针传给`getptr`，再`create_store`最终实现每一个元素的正确赋值。

- 再来讨论多维数组全局变量

  全局变量用了一套不同的API，为了调用API，我们要在`Constant.h`中添加支持多维数组的类：

  ```c++
  class ConstantMultiArray : public Constant
  {
  private:
      std::vector<std::vector<Constant*>> const_multi_array; // This is to store initial value
      std::vector<int> dimension_vec;
      int size;
      ConstantMultiArray(MultiDimensionArrayType *ty, std::vector<int> dimension_vec, const std::vector<std::vector<Constant*>> &val, int size);
  public:
      
      ~ConstantMultiArray()=default;
  
      Constant* get_element_value(std::vector<int> gep_vec); // hand me the corrdinate for each dimension
  
      unsigned get_size_of_array() { 
          return size;
       } 
  
      static ConstantMultiArray *get(MultiDimensionArrayType *ty, std::vector<int> dimension_vec, const std::vector<std::vector<Constant*>> &val, int size);
  
      virtual std::string print() override;
  };
  ```

  并实现各个函数。

  其中最为重要的是`print()`函数，它是有初值的多维数组全局变量声明的最大难点。

  在这里，我们要引入层级作用域的观点，这中方式在`InitVal`也有用到。

  这种观点能帮助我们了解什么时候打印几层方括号，以及该层的指针类型应该是什么

  形象的组织全局变量赋初值的形式。

  就是一颗树，其根节点对应最上层，树的每层结点都有`x`个孩子，这个`x`与该层对应的维度长有关

  例如`int a[2][2][3] = ...`

  这里根节点有两个孩子，这两个孩子分别又有两个孩子，第三层的结点又分别有三个孩子

  括号最终被组织成这种形式：

  `[[[ , , ],[ , , ]],[[ , , ],[ , , ]]]`

  我们来看clang的一个例子，将其拆分：

  ```llvm
  @a = dso_local global [2 x [2 x [3 x i32]]] //全局变量类型（alloc）
  [
  ·[2 x [3 x i32]] [
  ··[3 x i32] [i32 1, i32 2, i32 9], 
  ··[3 x i32] [i32 3, i32 4, i32 10]
  ], 
  ·[2 x [3 x i32]] [
  ·[3 x i32] [i32 5, i32 6, i32 11], 
  ·[3 x i32] [i32 7, i32 8, i32 12]
  ]
  ]
  , align 16
  ```

  这正是我们所说的括号组织形式。

### InitVal

针对普通和一维数组变量赋值：

 如果当前`InitVal`是`Expr`类型，则直接访问。如果当前`InitVal`是`InitVal`嵌套类型，则记录嵌套深度initval_depth++，遍历`node.elementList`。如果常值表达式`const_expr`是`int`类型，则将其值转换成`float`类型存入`array_inital`；如果是`float`类型，则直接存入`array_inital`。

针对多维数组：

嵌套深度的设计，主要是为了应对多维数组赋初值，举一个简单的例子：

`int a[2][2] = {{0，1}，{2，3}}；`

这个赋值的嵌套形式，我们的处理方式就是去记录程序当前的嵌套位置。

当发现位于最底层的时候，比如`0，1`

0和1都被规约为`Exp`，并合起来规约成`Exp`的`PtrList`，在程序计算`Exp`值的过程中，使用一个临时`vector<float>`存储计算结果，防止丢失信息，每进入一次最底层，将`vector`存入一个`vector<vector<float>>`中，最终在`VarDef`结点使用该信息。

### FuncDef

	遍历参数表中的每个参数，如果参数是指针类型，则调用PointerType::get获取指针类型存入参数类型数组params；如果参数不是指针类型，则直接从TypeMap中读入类型信息存入参数类型数组params。
	
	根据函数参数类型数组和函数返回类型建立函数类型，根据函数类型和函数名建立函数，将函数加入符号表，建立函数基本块。进入作用域，访问函数的参数表param_list，遍历函数体body，如果遇到ReturnStmt则不编译其后面语句，如果函数没有终止指令，则给函数增加一条返回语句，退出作用域。

### FuncFParamList

	当前的函数是函数表中最后一个函数，由此获取函数参数值表，遍历函数形参表，判断参数是不是指针类型并相应地获取类型分配空间存入参数的值。

### FuncParam

	遍历参数的数组维度列表，如果表达式Expr不空，则进行访问。

### EmptyStmt

不用处理

### ExprStmt

直接进行访问

### ReturnStmt

返回语句为空则生成空返回语句；否则先对表达式进行求值，然后根据求出值的类型以及当前函数的返回值类型判断是否需要进行类型转换，最后生成返回语句

### AssignStmt

先计算右式的值，接着获取左值的地址与类型，判断是否需要进行类型转换，最后生成 store 语句

### UnaryCondExpr

计算右式的值，接着根据值的类型与 0 或 0.0 进行比较，获取结果

### BinaryCondExpr

先判断是否为逻辑运算符，如果是则进行短路计算；否则分别计算出左式与右式的值，进行类型转换和比较

### UnaryExpr

计算右式的值，接着判断是否为 `-` ，是的话使用 0 或 0.0 减去右式的值；否则值不变

### LVal

根据名字查符号表获取左值的值和地址

### BinaryExpr

分别计算出左式与右式的值，进行类型转换和运算

### Literal

根据字面量类型生成 CONST_INT 或 CONST_FLOAT

### FuncCallStmt

通过函数名在符号表中找到函数，对每个形参类型进行分类为int，float，pointer，并相应求值，将结果放到实参集合 params 中，同时与形参进行类型比较，判断是否需要类型转换。调用create_call调用函数，并将实参集合传递给形参表。

### IfStmt

	如果没有else_statement，则访问条件表达式cond_exp，根据其值设置条件跳转，真跳转到 trueBB基本块，假跳转到nextBB基本块。建立trueBB断点，访问if_statement，跳转到nextBB；建立nextBB断点。如果有else_statement，则访问条件表达式cond_exp，根据其值设置条件跳转，真跳转到 trueBB基本块，假跳转到falseBB基本块。建立trueBB断点，访问if_statement，如果if_statement的最后语句不是终止指令则说明终止指令在nextBB中，跳转到nextBB；如果if_statement的最后语句是终止指令则去除nextBB。建立falseBB断点，访问else_statement，如果else_statement的最后语句不是终止指令则说明终止指令在nextBB中，跳转到nextBB。建立nextBB断点。

### WhileStmt

	强制跳转到condBB，建立condBB断点，访问条件表达式cond_exp，根据其值设置条件跳转，真跳转到 trueBB基本块，假跳转到falseBB基本块。建立trueBB断点，将当前condBB和falseBB分别存入tmp_condbb和tmp_falsebb(方便BreakStmt和ContinueStmt)，访问循环体statement，跳转到condBB，建立falseBB断点。最后tmp_condbb和tmp_falsebb栈顶出栈以清除已退出循环的condBB或falseBB。

### BreakStmt

	跳转到tmp_falsebb栈顶基本块。

### ContinueStmt

	跳转到tmp_condbb栈顶基本块。

## 实验难点及解决方案

### 如何实现短路计算

以 `||` 为例，先分配一个临时变量 tmp，接着计算左式的值，再将左式的值与 0 或 0.0 进行比较，并把比较结果赋给 tmp，为真则跳转到 next ，为假则跳转到 false，在 false 块内，tmp会加上右式的值与 0 或 0.0 比较的结果，接着跳转到 next 块中，next 块判断 tmp 的值是否大于 0 ，只要大于 0 就代表整个表达式的值为真

### 如何实现编译期计算常量表达式

构建一个全局变量 `const_expr` ，其存有常量表达式计算的中间值以及类型，同时还有全局变量 `const_int_var` 和 `const_float_var` ，用来存放全局 const 变量的名字与值。然后在遍历 AST 遇到在全局作用域内的表达式时，就计算它们的值，并存到 `const_expr ` 中

### 如何实现数组的左值运算

在 LVal 的计算中，假设遇到了 `a[0]`，那么对 a 是指针（表明 a 是函数参数）和 a 是数组 （局部声明或者全局声明的数组）进行判断并分开处理；如果遇到了 `a` 并且发现 a 是个指针或者数组，那表明 a 是作为函数实参传入的，这时就要分开并正确生成指向 a 中第一个元素地址的指针类型

### 如何实现break和continue语句的正确跳转

​	建立全局栈tmp_condbb和tmp_falsebb，当访问循环体之前把condBB和falseBB存入栈中，循环体中有break或continue则可以跳转到相应栈顶基本块，循环结束后栈顶基本块出栈。

### 如何解决void类型函数以if-else语句结束导致无法正常终止的问题

​	我们给每个没有终止指令的函数增加一条返回语句，则if语句如果以终止指令结尾则不需要建立next基本块，否则next基本块一定含终止指令。

### 如何实现一维数组的赋值

 采用一个全局`vector`存储赋值，离开`Initval`结点意味着已经获取所有赋值信息，在`VarDef`中直接调用`vector`来获取初值。

### 如何实现多维数组及其赋值

大致已在`VarDef`结点中进行了讲解，主要是自己做了新的`API`来保证相关部分的正确运行与输出。

这里多次用到了进制计算、作用域等等知识点，信息量庞杂且巨大。

保存初值的方式是建立一个`vector<vector<float>>`，每一个内部`vector`都对应一次对最低维度结点的赋值。

在全局变量方面，括号的组成类似于数组赋初值的代码组成，这里采用作用域分级的思想，假设程序向前走，每输出一个' [ '就是进入更深一层，每输出一个' ] '就是上升到更浅一层，在最底层获取赋值信息，输出`value`

对齐问题在`Initval`中无法实现，但是在`VarDef`中可以实现，针对每一个内部`vector`，如果长度小于最低维长度，则用0信息补齐即可。

## 实验总结

在本次实验中，本小组在助教给的代码框架基础上实现了 SysYF 语言的 IR 生成器。在编写代码的过程中，小组成员对于 代码生成的基本逻辑以及 LLVM IR 的基本指令有了更深的了解，同时也了解了在代码生成的过程中有哪些以前不知道的难点。在解决这些难点的过程中，小组成员对代码生成的过程更为明晰，同时对于访问者模式的使用与优点也有了更深的体会。

实验对多维数组的实现，使我们更加深入的了解了整个生成器的代码架构，以及其运行机制。包括，如何得到操作对象在整个`module`中对应的命名（%opx），怎样通过类保存的信息，打印出各种llvm对应的指令等等。整个代码工程的组建方式，让人体会到了C++的方便与美妙之处。

## 实验反馈

助教给的框架设计合理，文档详细明晰，实验时间给的也比较宽裕，可以按时完成实验。小组实验的模式也让我们体验到了多人协同开发的流程以及一些注意事项。

文档的API提示方面有待修改，其问题主要体现在描述不够详细，部分令人无法很好的理解调用方式。除此之外，在全局变量API部分竟然提出语言定义全局变量初值一定为0。这与所给的测试样例，以及`task2`所给的demo中的builder文件对全局变量API的调用自相矛盾。

在许多API的使用中，使用者不得不查看源码来知晓一些调用方式，以及其影响。

## 组间交流

本组没有与其他小组进行交流
