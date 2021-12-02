#include "IRBuilder.h"
#include "BasicBlock.h"
#include "SyntaxTree.h"

#define CONST_INT(num) ConstantInt::get(num, module.get())
#define CONST_FLOAT(num) ConstantFloat::get(num, module.get())

// You can define global variables and functions here
int is_global = 0; // at first it is global
// to store state

struct ConstExpr {
    bool is_valid; // 为真代表表达式确实是个可计算的常值
    bool is_int; // 为真代表表达式计算结果是个 int 
    int int_value; // 当 is_int 为真时此成员有效
    float float_value; // 当 is_int 为假时此成员有效
} const_expr{false, false, 0, 0.0};

// store temporary value
Value *tmp_val = nullptr;
Value *tmp_addr = nullptr; // 地址
int label = 0;
std::vector<BasicBlock*> tmp_condbb;
std::vector<BasicBlock*> tmp_falsebb;
std::vector<float> array_inital;            // for each dimension store the num 
// 这里面保存了所有的全局的 const int 以及 const int 数组变量，std::string 是 它的名字，std::vector 里放它的值
std::map<std::string, std::vector<int>> const_int_var;
// 这里面保存了所有的全局的 const float 以及 const float 数组变量，std::string 是 它的名字，std::vector 里放它的值
std::map<std::string, std::vector<float>> const_float_var;

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *FLOAT_T;
Type *INT32PTR_T;
Type *FLOATPTR_T;
std::map<SyntaxTree::Type,Type *> TypeMap;

void IRBuilder::visit(SyntaxTree::Assembly &node) {
    VOID_T = Type::get_void_type(module.get());
    INT1_T = Type::get_int1_type(module.get());
    INT32_T = Type::get_int32_type(module.get());
    FLOAT_T = Type::get_float_type(module.get());
    INT32PTR_T = Type::get_int32_ptr_type(module.get());
    FLOATPTR_T = Type::get_float_ptr_type(module.get());

    TypeMap[SyntaxTree::Type::VOID]=VOID_T;
    TypeMap[SyntaxTree::Type::BOOL]=INT1_T;
    TypeMap[SyntaxTree::Type::INT]=INT32_T;
    TypeMap[SyntaxTree::Type::FLOAT]=FLOAT_T;

    for (const auto &def : node.global_defs) {
        def->accept(*this);
    }
}

// You need to fill them

void IRBuilder::visit(SyntaxTree::InitVal &node)
{
    if (node.isExp)
    {
        node.expr->accept(*this); // inside expr, this would help get value
                                  // VarDef hold the place for alloca
    }
    else
    {
        int i = 0;
        for (auto item : node.elementList)
        {
            item->accept(*this);    // tell this part if next level is Exp or not
            if(const_expr.is_int == true)
            {
                array_inital.push_back((float)const_expr.int_value);
            }
            else
            {
                array_inital.push_back(const_expr.float_value);
            }
            i++;
        } // we will wait to see if there needs more data pass
    }
    return;
}

void IRBuilder::visit(SyntaxTree::FuncDef &node) {
    std::vector<Type *> params;
    is_global++;
    for(auto param:node.param_list->params){
        params.push_back(TypeMap[param->param_type]);
    }
    auto FuncType = FunctionType::get(TypeMap[node.ret_type], params);
    auto Func = Function::create(FuncType, node.name, builder->get_module());
    scope.push(node.name,Func);
    auto bb = BasicBlock::create(builder->get_module(), "entry" + std::to_string(label++), Func);
    builder->set_insert_point(bb);
    scope.enter();
    node.param_list->accept(*this);
    for (auto statement : node.body->body) {
        statement->accept(*this);
        if(dynamic_cast<SyntaxTree::ReturnStmt*>(statement.get())){
            break;
        }
    }
    scope.exit();
    is_global--;
}

void IRBuilder::visit(SyntaxTree::FuncFParamList &node) {
    auto Argument = builder->get_module()->get_functions().back()->arg_begin();//当前的函数应该是函数表中最后一个函数
    for (const auto &param : node.params){
        param->accept(*this);                                               //访问参数
        if(!param->array_index.empty()) {
            auto paramAlloc = 
                param->param_type == SyntaxTree::Type::FLOAT
                ? builder->create_alloca(FLOATPTR_T)
                : builder->create_alloca(INT32PTR_T);
            builder->create_store(*Argument, paramAlloc);                            //存参数的值
            scope.push(param->name, paramAlloc);                              //加入符号表
        } else {
            auto paramAlloc = builder->create_alloca(TypeMap[param->param_type]);     //分配空间
            builder->create_store(*Argument, paramAlloc);                            //存参数的值
            scope.push(param->name, paramAlloc);                              //加入符号表
        }
        Argument++;                                                             //下一个参数值
    }
}

void IRBuilder::visit(SyntaxTree::FuncParam &node) {
    for(auto Exp:node.array_index){                                             //遍历每个Expr
        if (Exp != nullptr){
            Exp->accept(*this);
        }
    }
}

void IRBuilder::visit(SyntaxTree::VarDef &node)
{ // we will need to know glob/local here // seems const is not considered
    /* first judge is int or float */
    Type *Vartype;
    if (node.btype == SyntaxTree::Type::INT)
    {
        Vartype = INT32_T;
    }
    else
    {
        Vartype = FLOAT_T;
    }
    auto zero_initializer = ConstantZero::get(Vartype, module.get());
    int count = 0;
    int DimensionLength = 0;
    for (auto length : node.array_length)
    {
        length->accept(*this);
        count++; // this is about dimension
        DimensionLength = const_expr.int_value; // no checking here a[float] was not permitted
    }
    if (node.is_inited) 
    {
        node.initializers->accept(*this);
        auto initializer = tmp_val; // is inited
        if (scope.in_global())
        {
            if (count == 0) // this is not an array
            {
                if(tmp_val->get_type()->is_float_type() && node.btype == SyntaxTree::Type::INT) // float init
                {    
                    auto Adjusted_Inital = builder->create_fptosi(tmp_val, INT32_T);
                    auto global_init = GlobalVariable::create(node.name, module.get(), Vartype, false, dynamic_cast<Constant*>(Adjusted_Inital));
                    int val = (int)const_expr.float_value;
                    std::vector<int> init_vec;
                    init_vec.push_back(val);
                    if(node.is_constant == true)
                        const_int_var[node.name] = init_vec;
                }
                else if(tmp_val->get_type()->is_integer_type() && node.btype == SyntaxTree::Type::FLOAT)
                {
                    auto Adjusted_Inital = builder->create_sitofp(tmp_val, FLOAT_T);
                    auto global_init = GlobalVariable::create(node.name, module.get(), Vartype, false, dynamic_cast<Constant*>(Adjusted_Inital));
                    float val = (float)const_expr.int_value;
                    std::vector<float> init_vec;
                    init_vec.push_back(val);
                    if(node.is_constant == true)
                        const_float_var[node.name] = init_vec;
                }
                else
                {
                    auto global_init = GlobalVariable::create(node.name, module.get(), Vartype, false, dynamic_cast<Constant*>(tmp_val));
                    scope.push(node.name, global_init); // ok
                    if(node.is_constant == true)
                    {
                        if(const_expr.is_int)
                        {
                            int val = const_expr.int_value;
                            std::vector<int> init_vec;
                            init_vec.push_back(val);
                            const_int_var[node.name] = init_vec;
                        }
                        else
                        {
                            float val = const_expr.float_value;
                            std::vector<float> init_vec;
                            init_vec.push_back(val);
                            const_float_var[node.name] = init_vec;
                        }
                    }
                }
            }
            else    // array
            {
                auto *arrayType_global = ArrayType::get(Vartype, DimensionLength);
                if(node.btype == SyntaxTree::Type::FLOAT)
                {
                    std::vector<Constant *> init_val;
                    for(int i = 0; i < DimensionLength; i++)
                    {
                        if(i < array_inital.size()) // still have value
                            init_val.push_back(CONST_FLOAT(array_inital[i])); // float array
                        else
                            init_val.push_back(CONST_FLOAT(0.0));
                    }
                    auto arrayInitializer = ConstantArray::get(arrayType_global, init_val);
                    auto arrayGlobal = GlobalVariable::create(node.name, module.get(), arrayType_global, false, arrayInitializer);
                    scope.push(node.name, arrayGlobal);
                }
                else    // INT
                {
                    std::vector<Constant *> init_val;
                    for(int i = 0; i < DimensionLength; i++)
                    {
                        if(i < array_inital.size()) // still have value
                            init_val.push_back(CONST_INT((int)array_inital[i])); // float array
                        else
                            init_val.push_back(CONST_INT(0));
                    }
                    auto arrayInitializer = ConstantArray::get(arrayType_global, init_val);
                    auto arrayGlobal = GlobalVariable::create(node.name, module.get(), arrayType_global, false, arrayInitializer);
                    scope.push(node.name, arrayGlobal);
                }
                if(node.is_constant == true)
                {
                    if(node.btype == SyntaxTree::Type::FLOAT)
                    {
                        std::vector<float> init_vec;
                        for(int i = 0; i < DimensionLength; i++)
                        {
                            if(i < array_inital.size())
                                init_vec.push_back(array_inital[i]);
                            else
                                init_vec.push_back(0.0);
                            const_float_var[node.name] = init_vec;
                        }
                    }
                    else
                    {
                        std::vector<int> init_vec;
                        for(int i = 0; i < DimensionLength; i++)
                        {
                            if(i < array_inital.size())
                                init_vec.push_back((int)array_inital[i]);
                            else
                                init_vec.push_back(0);
                            const_int_var[node.name] = init_vec;
                        }
                    }
                }
                array_inital.clear(); // for next def
            }
        }
        else // initialed not global
        {
            if (count == 0) // this is not an array
            {
                auto local_init = builder->create_alloca(Vartype);
                scope.push(node.name, local_init);
                if(tmp_val->get_type()->is_float_type() && node.btype == SyntaxTree::Type::INT) // float init
                {
                    auto Adjusted_Inital = builder->create_fptosi(tmp_val, INT32_T);
                    builder->create_store(Adjusted_Inital, local_init);
                }
                else if(tmp_val->get_type()->is_integer_type() && node.btype == SyntaxTree::Type::FLOAT)
                {
                    auto Adjusted_Inital = builder->create_sitofp(tmp_val, FLOAT_T);
                    builder->create_store(Adjusted_Inital, local_init);
                }
                else
                {
                    builder->create_store(tmp_val, local_init);
                }
            }
            else // array
            {
                auto *arrayType_local = ArrayType::get(Vartype, DimensionLength);
                auto arrayLocal = builder->create_alloca(arrayType_local);
                if(node.btype == SyntaxTree::Type::FLOAT)
                {
                    for(int i = 0; i < DimensionLength; i++)
                    {
                        if(i < array_inital.size()) // still have value
                        {
                            auto Gep = builder->create_gep(arrayLocal, {CONST_INT(0), CONST_INT(i)});
                            builder->create_store(CONST_FLOAT(array_inital[i]), Gep);
                        }
                    }
                    scope.push(node.name, arrayLocal);
                }
                else
                {
                    for(int i = 0; i < DimensionLength; i++)
                    {
                        if(i < array_inital.size()) // still have value
                        {
                            auto Gep = builder->create_gep(arrayLocal, {CONST_INT(0), CONST_INT(i)});
                            builder->create_store(CONST_INT((int)array_inital[i]), Gep);
                        }
                    }
                    scope.push(node.name, arrayLocal);
                }
                if(node.is_constant == true)    // const procedure
                {
                    if(node.btype == SyntaxTree::Type::FLOAT)
                    {
                        std::vector<float> init_vec;
                        for(int i = 0; i < DimensionLength; i++)
                        {
                            if(i < array_inital.size())
                                init_vec.push_back(array_inital[i]);
                            else
                                init_vec.push_back(0.0);
                            const_float_var[node.name] = init_vec;
                        }
                    }
                    else
                    {
                        std::vector<int> init_vec;
                        for(int i = 0; i < DimensionLength; i++)
                        {
                            if(i < array_inital.size())
                                init_vec.push_back((int)array_inital[i]);
                            else
                                init_vec.push_back(0);
                            const_int_var[node.name] = init_vec;
                        }
                    }
                }
                array_inital.clear(); // for next def
            }
        }
    }
    else // no need to check type
    {
        if (scope.in_global())
        {
            if (count == 0) // this is not an array
            {
                auto global_init = GlobalVariable::create(node.name, module.get(), Vartype, false, zero_initializer);
                scope.push(node.name, global_init);
            }
            else
            {
                auto *arrayType_global = ArrayType::get(Vartype, DimensionLength);
                auto arrayGlobal = GlobalVariable::create(node.name, module.get(), arrayType_global, false, zero_initializer);
                scope.push(node.name, arrayGlobal);
            }
        }
        else
        {
            if (count == 0) // this is not an array
            {
                auto local_init = builder->create_alloca(Vartype);
                scope.push(node.name, local_init);
            }
            else
            {
                auto *arrayType_local = ArrayType::get(Vartype, DimensionLength);
                auto arrayLocal = builder->create_alloca(arrayType_local);
                scope.push(node.name, arrayLocal);
            }
        }
    }
}

void IRBuilder::visit(SyntaxTree::LVal &node) {
    auto ret = this->scope.find(node.name, false); // 根据名字获取值
    if (!node.array_index.empty()) {               // 如果是个数组
        node.array_index[0]->accept(*this);        // 计算下标表达式的值
        ret = this->builder->create_gep(ret, {CONST_INT(0), tmp_val}); // 获取数组元素
        tmp_addr = ret;
        tmp_val = this->builder->create_load(tmp_addr);
        return;
    }
    tmp_addr = ret;
    tmp_val = this->builder->create_load(ret);
}

void IRBuilder::visit(SyntaxTree::AssignStmt &node) {
    node.value->accept(*this);
    auto src = tmp_val; // 获取等号右边表达式的值
    node.target->accept(*this);
    auto dest = tmp_addr; // 获取左值
    if (dest->get_type()->get_pointer_element_type() == FLOAT_T) {
        if (src->get_type() == INT32_T)
            src = this->builder->create_sitofp(src, FLOAT_T);
        if (src->get_type() == INT1_T) {
            src = this->builder->create_zext(src, INT32_T);
            src = this->builder->create_sitofp(src, FLOAT_T);
        }
    } 
    if (dest->get_type()->get_pointer_element_type() == INT32_T) {
        if (src->get_type() == FLOAT_T)
            src = this->builder->create_fptosi(src, INT32_T);
        if (src->get_type() == INT1_T) 
            src = this->builder->create_zext(src, INT32_T);
    }
    this->builder->create_store(src, dest); // 存储值
    tmp_val = src; // 整个表达式的值就是等号右边的表达式的值
}

void IRBuilder::visit(SyntaxTree::Literal &node) {
    if (node.literal_type == SyntaxTree::Type::INT) { // 字面量是个整形
        tmp_val = CONST_INT(node.int_const);
        const_expr = {true, true, node.int_const, 0.0};
    } else {// 字面量是个浮点数
        tmp_val = CONST_FLOAT(node.float_const);
        const_expr = {true, false, 0, (float)node.float_const};
    }
}

void IRBuilder::visit(SyntaxTree::ReturnStmt &node) {
    if (node.ret.get() == nullptr) { // 返回值为 void 
        this->builder->create_void_ret();
        tmp_val = nullptr;
        return;
    }
    node.ret->accept(*this);
    auto retType = this->module->get_functions().back()->get_function_type();
    if (retType == FLOAT_T) {
        if (tmp_val->get_type() == INT32_T)
            tmp_val = this->builder->create_sitofp(tmp_val, FLOAT_T);
        if (tmp_val->get_type() == INT1_T) {
            tmp_val = this->builder->create_zext(tmp_val, INT32_T);
            tmp_val = this->builder->create_sitofp(tmp_val, FLOAT_T);
        }
    }
    if (retType == INT32_T) {
        if (tmp_val->get_type() == FLOAT_T)
            tmp_val = this->builder->create_fptosi(tmp_val, INT32_T);
        if (tmp_val->get_type() == INT1_T)
            tmp_val = this->builder->create_zext(tmp_val, INT32_T);
    }
    this->builder->create_ret(tmp_val);
    tmp_val = nullptr; // return 语句没有值
}

void IRBuilder::visit(SyntaxTree::BlockStmt &node) {
    this->scope.enter(); // 进入一个新的块作用域
    for (const auto &stmt : node.body) {
        stmt->accept(*this); // 对块里每条语句都生成代码
        if (dynamic_cast<SyntaxTree::ReturnStmt *>(stmt.get())) // 遇到返回语句，以后的语句不用生成代码了
            break;
    }
    tmp_val = nullptr;
    this->scope.exit(); // 退出块作用域
}

void IRBuilder::visit(SyntaxTree::EmptyStmt &node) {}

void IRBuilder::visit(SyntaxTree::ExprStmt &node) {
    node.exp->accept(*this);
    // 表达式的值就是 exp 的值，无需改变 tmp_val
}

void IRBuilder::visit(SyntaxTree::UnaryCondExpr &node) {
    node.rhs->accept(*this);
    auto rhs = tmp_val;
    if (tmp_val->get_type()->is_float_type()) 
        tmp_val = this->builder->create_fcmp_eq(rhs, CONST_FLOAT(0));
    else 
        tmp_val = this->builder->create_fcmp_eq(rhs, CONST_INT(0));
    // 如果 rhs 为 0，那 tmp_val = 1 否则 tmp_val = 0
    if (const_expr.is_valid) {
        if (const_expr.is_int)
            const_expr.int_value = const_expr.int_value == 0;
        else {
            const_expr.is_int = true;
            const_expr.int_value = const_expr.float_value == 0.0;
        }
    }
}

void IRBuilder::visit(SyntaxTree::BinaryCondExpr &node) {
    if (node.op == SyntaxTree::BinaryCondOp::LOR) {
        node.lhs->accept(*this);
        auto lhs_const = const_expr;
        const_expr.is_valid = false;

        auto if_true = BasicBlock::create( // 短路计算
                this->module.get(),
                "if_true" + std::to_string(label++),
                this->builder->get_insert_block()->get_parent());
        auto if_false = BasicBlock::create(
                this->module.get(),
                "if_false" + std::to_string(label++),
                this->builder->get_insert_block()->get_parent());

        if (tmp_val->get_type()->is_float_type()) // 为真则不用算右边
            tmp_val = this->builder->create_fcmp_ne(tmp_val, CONST_FLOAT(0.0));
        else 
            tmp_val = this->builder->create_icmp_ne(tmp_val, CONST_INT(0));

        Value *ret = this->builder->create_zext(tmp_val, INT32_T);
        auto retAlloca = this->builder->create_alloca(INT32_T); // 保存表达式比较结果
        this->builder->create_store(ret, retAlloca);

        this->builder->create_cond_br(tmp_val, if_true, if_false);
        this->builder->set_insert_point(if_false);

        node.rhs->accept(*this);
        auto rhs_const = const_expr;
        if (tmp_val->get_type()->is_float_type())
            tmp_val = this->builder->create_fcmp_ne(tmp_val, CONST_FLOAT(0.0));
        else 
            tmp_val = this->builder->create_icmp_ne(tmp_val, CONST_INT(0));

        ret = this->builder->create_load(retAlloca);
        tmp_val = this->builder->create_zext(tmp_val, INT32_T);
        tmp_val = this->builder->create_iadd(ret, tmp_val); // 把两个逻辑表达式的结果加在一起
        this->builder->create_store(tmp_val, retAlloca);    // 结果存到 retAlloca
        this->builder->create_br(if_true);
        
        this->builder->set_insert_point(if_true); // 左式为真，启动短路计算
        ret = this->builder->create_load(retAlloca);
        tmp_val = this->builder->create_icmp_gt(ret, CONST_INT(0)); // 只要结果不是 0 就代表有一个为 1

        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            const_expr.is_int = true;
            lhs_const.int_value = lhs_const.is_int
                ? lhs_const.int_value != 0
                : lhs_const.float_value != 0.0;
            rhs_const.int_value = rhs_const.is_int
                ? rhs_const.int_value != 0
                : rhs_const.float_value != 0.0;
            const_expr.int_value = lhs_const.int_value || rhs_const.int_value;
        } else {
            const_expr.is_valid = false;
        }
        return;
    }
    if (node.op == SyntaxTree::BinaryCondOp::LAND) {
        node.lhs->accept(*this);
        auto lhs_const = const_expr;
        const_expr.is_valid = false;

        auto if_true = BasicBlock::create( // 短路计算
                this->module.get(),
                "if_true" + std::to_string(label++),
                this->builder->get_insert_block()->get_parent());
        auto if_false = BasicBlock::create(
                this->module.get(),
                "if_false" + std::to_string(label++),
                this->builder->get_insert_block()->get_parent());

        if (tmp_val->get_type()->is_float_type()) // 为假则不用算右边
            tmp_val = this->builder->create_fcmp_ne(tmp_val, CONST_FLOAT(0.0));
        else 
            tmp_val = this->builder->create_icmp_ne(tmp_val, CONST_INT(0));

        Value *ret = this->builder->create_zext(tmp_val, INT32_T);
        auto retAlloca = this->builder->create_alloca(INT32_T);
        this->builder->create_store(ret, retAlloca);

        this->builder->create_cond_br(tmp_val, if_true, if_false);
        this->builder->set_insert_point(if_true);

        node.rhs->accept(*this);
        auto rhs_const = const_expr;
        if (tmp_val->get_type()->is_float_type())
            tmp_val = this->builder->create_fcmp_ne(tmp_val, CONST_FLOAT(0.0));
        else
            tmp_val = this->builder->create_icmp_ne(tmp_val, CONST_INT(0));
        ret = this->builder->create_load(retAlloca);
        tmp_val = this->builder->create_zext(tmp_val, INT32_T);
        tmp_val = this->builder->create_iadd(ret, tmp_val); // 把两个表达式结果加一起
        this->builder->create_store(tmp_val, retAlloca);
        this->builder->create_br(if_false);

        this->builder->set_insert_point(if_false); // 左式为假，启动短路计算
        ret = this->builder->create_load(retAlloca);
        tmp_val = this->builder->create_icmp_eq(ret, CONST_INT(2)); // 只要结果等于 2 那就是真
        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            const_expr.is_int = true;
            lhs_const.int_value = lhs_const.is_int
                ? lhs_const.int_value != 0
                : lhs_const.float_value != 0.0;
            rhs_const.int_value = rhs_const.is_int
                ? rhs_const.int_value != 0
                : rhs_const.float_value != 0.0;
            const_expr.int_value = lhs_const.int_value && rhs_const.int_value;
        } else {
            const_expr.is_valid = false;
        }
        return;
    }
    
    node.lhs->accept(*this);
    auto lhs = tmp_val;
    auto lhs_const = const_expr;
    const_expr.is_valid = false;
    node.rhs->accept(*this);
    auto rhs = tmp_val;
    auto rhs_const = const_expr;

    if (node.op == SyntaxTree::BinaryCondOp::EQ) {
        if (lhs->get_type()->is_float_type()) {
            if (rhs->get_type()->is_integer_type())
                rhs = this->builder->create_sitofp(rhs, FLOAT_T); // 类型提升
            tmp_val = this->builder->create_fcmp_eq(lhs, rhs);
        } else if (rhs->get_type()->is_float_type()) {
            lhs = this->builder->create_sitofp(lhs, FLOAT_T); // 类型提升
            tmp_val = this->builder->create_fcmp_eq(lhs, rhs);
        } else {
            tmp_val = this->builder->create_icmp_eq(lhs, rhs);
        }
        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            const_expr.is_int = true;
            if (lhs_const.is_int) {
                const_expr.int_value = rhs_const.is_int
                    ? lhs_const.int_value == rhs_const.int_value
                    : lhs_const.int_value == rhs_const.float_value;
            } else {
                const_expr.int_value = rhs_const.is_int
                    ? lhs_const.float_value == rhs_const.int_value
                    : lhs_const.float_value == rhs_const.float_value;
            }
        } else {
            const_expr.is_valid = false;
        }
    } else if (node.op == SyntaxTree::BinaryCondOp::GT) { // 以下基本重复，应该可以使用函数指针优化
        if (lhs->get_type()->is_float_type()) {
            if (rhs->get_type()->is_integer_type())
                rhs = this->builder->create_sitofp(rhs, FLOAT_T);
            tmp_val = this->builder->create_fcmp_gt(lhs, rhs);
        } else if (rhs->get_type()->is_float_type()) {
            lhs = this->builder->create_sitofp(lhs, FLOAT_T);
            tmp_val = this->builder->create_fcmp_gt(lhs, rhs);
        } else {
            tmp_val = this->builder->create_icmp_gt(lhs, rhs);
        }
        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            const_expr.is_int = true;
            if (lhs_const.is_int) {
                const_expr.int_value = rhs_const.is_int
                    ? lhs_const.int_value > rhs_const.int_value
                    : lhs_const.int_value > rhs_const.float_value;
            } else {
                const_expr.int_value = rhs_const.is_int
                    ? lhs_const.float_value > rhs_const.int_value
                    : lhs_const.float_value > rhs_const.float_value;
            }
        } else {
            const_expr.is_valid = false;
        }
    } else if (node.op == SyntaxTree::BinaryCondOp::GTE) {
        if (lhs->get_type()->is_float_type()) {
            if (rhs->get_type()->is_integer_type())
                rhs = this->builder->create_sitofp(rhs, FLOAT_T);
            tmp_val = this->builder->create_fcmp_ge(lhs, rhs);
        } else if (rhs->get_type()->is_float_type()) {
            lhs = this->builder->create_sitofp(lhs, FLOAT_T);
            tmp_val = this->builder->create_fcmp_ge(lhs, rhs);
        } else {
            tmp_val = this->builder->create_icmp_ge(lhs, rhs);
        }
        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            const_expr.is_int = true;
            if (lhs_const.is_int) {
                const_expr.int_value = rhs_const.is_int
                    ? lhs_const.int_value >= rhs_const.int_value
                    : lhs_const.int_value >= rhs_const.float_value;
            } else {
                const_expr.int_value = rhs_const.is_int
                    ? lhs_const.float_value >= rhs_const.int_value
                    : lhs_const.float_value >= rhs_const.float_value;
            }
        } else {
            const_expr.is_valid = false;
        }
    } else if (node.op == SyntaxTree::BinaryCondOp::NEQ) {
        if (lhs->get_type()->is_float_type()) {
            if (rhs->get_type()->is_integer_type())
                rhs = this->builder->create_sitofp(rhs, FLOAT_T);
            tmp_val = this->builder->create_fcmp_ne(lhs, rhs);
        } else if (rhs->get_type()->is_float_type()) {
            lhs = this->builder->create_sitofp(lhs, FLOAT_T);
            tmp_val = this->builder->create_fcmp_ne(lhs, rhs);
        } else {
            tmp_val = this->builder->create_icmp_ne(lhs, rhs);
        }
        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            const_expr.is_int = true;
            if (lhs_const.is_int) {
                const_expr.int_value = rhs_const.is_int
                    ? lhs_const.int_value != rhs_const.int_value
                    : lhs_const.int_value != rhs_const.float_value;
            } else {
                const_expr.int_value = rhs_const.is_int
                    ? lhs_const.float_value != rhs_const.int_value
                    : lhs_const.float_value != rhs_const.float_value;
            }
        } else {
            const_expr.is_valid = false;
        }
    } else if (node.op == SyntaxTree::BinaryCondOp::LT) {
        if (lhs->get_type()->is_float_type()) {
            if (rhs->get_type()->is_integer_type())
                rhs = this->builder->create_sitofp(rhs, FLOAT_T);
            tmp_val = this->builder->create_fcmp_lt(lhs, rhs);
        } else if (rhs->get_type()->is_float_type()) {
            lhs = this->builder->create_sitofp(lhs, FLOAT_T);
            tmp_val = this->builder->create_fcmp_lt(lhs, rhs);
        } else {
            tmp_val = this->builder->create_icmp_lt(lhs, rhs);
        }
        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            const_expr.is_int = true;
            if (lhs_const.is_int) {
                const_expr.int_value = rhs_const.is_int
                    ? lhs_const.int_value < rhs_const.int_value
                    : lhs_const.int_value < rhs_const.float_value;
            } else {
                const_expr.int_value = rhs_const.is_int
                    ? lhs_const.float_value < rhs_const.int_value
                    : lhs_const.float_value < rhs_const.float_value;
            }
        } else {
            const_expr.is_valid = false;
        }
    } else if (node.op == SyntaxTree::BinaryCondOp::LTE) {
        if (lhs->get_type()->is_float_type()) {
            if (rhs->get_type()->is_integer_type())
                rhs = this->builder->create_sitofp(rhs, FLOAT_T);
            tmp_val = this->builder->create_fcmp_le(lhs, rhs);
        } else if (rhs->get_type()->is_float_type()) {
            lhs = this->builder->create_sitofp(lhs, FLOAT_T);
            tmp_val = this->builder->create_fcmp_le(lhs, rhs);
        } else {
            tmp_val = this->builder->create_icmp_le(lhs, rhs);
        }
        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            const_expr.is_int = true;
            if (lhs_const.is_int) {
                const_expr.int_value = rhs_const.is_int
                    ? lhs_const.int_value <= rhs_const.int_value
                    : lhs_const.int_value <= rhs_const.float_value;
            } else {
                const_expr.int_value = rhs_const.is_int
                    ? lhs_const.float_value <= rhs_const.int_value
                    : lhs_const.float_value <= rhs_const.float_value;
            }
        } else {
            const_expr.is_valid = false;
        }
    }
}

void IRBuilder::visit(SyntaxTree::BinaryExpr &node) {
    node.lhs->accept(*this);
    auto lhs = tmp_val;
    auto lhs_const = const_expr;
    const_expr.is_valid = false;
    node.rhs->accept(*this);
    auto rhs = tmp_val;
    auto rhs_const = const_expr;

    if (node.op == SyntaxTree::BinOp::PLUS) {
        if (lhs->get_type()->is_float_type()) {
            if (rhs->get_type()->is_integer_type())
                rhs = this->builder->create_sitofp(rhs, FLOAT_T);
            tmp_val = this->builder->create_fadd(lhs, rhs);
        } else if (rhs->get_type()->is_float_type()) {
            lhs = this->builder->create_sitofp(lhs, FLOAT_T);
            tmp_val = this->builder->create_fadd(lhs, rhs);
        } else {
            tmp_val = this->builder->create_iadd(lhs, rhs);
        }
        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            if (lhs_const.is_int) {
                if (rhs_const.is_int) {
                    const_expr.is_int = true;
                    const_expr.int_value = lhs_const.int_value + rhs_const.int_value;
                } else {
                    const_expr.is_int = false;
                    const_expr.float_value = lhs_const.int_value + rhs_const.float_value;
                }
            } else {
                const_expr.is_int = false;
                const_expr.float_value = rhs_const.is_int
                    ? lhs_const.float_value + rhs_const.int_value
                    : lhs_const.float_value + rhs_const.float_value;
            }
        } else {
            const_expr.is_valid = false;
        }
    } else if (node.op == SyntaxTree::BinOp::MINUS) {
        if (lhs->get_type()->is_float_type()) {
            if (rhs->get_type()->is_integer_type())
                rhs = this->builder->create_sitofp(rhs, FLOAT_T);
            tmp_val = this->builder->create_fsub(lhs, rhs);
        } else if (rhs->get_type()->is_float_type()) {
            lhs = this->builder->create_sitofp(lhs, FLOAT_T);
            tmp_val = this->builder->create_fsub(lhs, rhs);
        } else {
            tmp_val = this->builder->create_isub(lhs, rhs);
        }
        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            if (lhs_const.is_int) {
                if (rhs_const.is_int) {
                    const_expr.is_int = true;
                    const_expr.int_value = lhs_const.int_value - rhs_const.int_value;
                } else {
                    const_expr.is_int = false;
                    const_expr.float_value = lhs_const.int_value - rhs_const.float_value;
                }
            } else {
                const_expr.is_int = false;
                const_expr.float_value = rhs_const.is_int
                    ? lhs_const.float_value - rhs_const.int_value
                    : lhs_const.float_value - rhs_const.float_value;
            }
        } else {
            const_expr.is_valid = false;
        }
    } else if (node.op == SyntaxTree::BinOp::MULTIPLY) {
        if (lhs->get_type()->is_float_type()) {
            if (rhs->get_type()->is_integer_type())
                rhs = this->builder->create_sitofp(rhs, FLOAT_T);
            tmp_val = this->builder->create_fmul(lhs, rhs);
        } else if (rhs->get_type()->is_float_type()) {
            lhs = this->builder->create_sitofp(lhs, FLOAT_T);
            tmp_val = this->builder->create_fmul(lhs, rhs);
        } else {
            tmp_val = builder->create_imul(lhs, rhs);
            /*tmp_val = dynamic_cast<Value *>(temp);*/  // Critical Problem! it will create mul i32*, which should not exist; 
        }
        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            if (lhs_const.is_int) {
                if (rhs_const.is_int) {
                    const_expr.is_int = true;
                    const_expr.int_value = lhs_const.int_value * rhs_const.int_value;
                } else {
                    const_expr.is_int = false;
                    const_expr.float_value = lhs_const.int_value * rhs_const.float_value;
                }
            } else {
                const_expr.is_int = false;
                const_expr.float_value = rhs_const.is_int
                    ? lhs_const.float_value * rhs_const.int_value
                    : lhs_const.float_value * rhs_const.float_value;
            }
        } else {
            const_expr.is_valid = false;
        }
    } else if (node.op == SyntaxTree::BinOp::DIVIDE) {
        if (lhs->get_type()->is_float_type()) {
            if (rhs->get_type()->is_integer_type())
                rhs = this->builder->create_sitofp(rhs, FLOAT_T);
            tmp_val = this->builder->create_fdiv(lhs, rhs);
        } else if (rhs->get_type()->is_float_type()) {
            lhs = this->builder->create_sitofp(lhs, FLOAT_T);
            tmp_val = this->builder->create_fdiv(lhs, rhs);
        } else {
            tmp_val = this->builder->create_isdiv(lhs, rhs);
        }
        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            if (lhs_const.is_int) {
                if (rhs_const.is_int) {
                    const_expr.is_int = true;
                    const_expr.int_value = lhs_const.int_value / rhs_const.int_value;
                } else {
                    const_expr.is_int = false;
                    const_expr.float_value = lhs_const.int_value / rhs_const.float_value;
                }
            } else {
                const_expr.is_int = false;
                const_expr.float_value = rhs_const.is_int
                    ? lhs_const.float_value / rhs_const.int_value
                    : lhs_const.float_value / rhs_const.float_value;
            }
        } else {
            const_expr.is_valid = false;
        }
    } else if (node.op == SyntaxTree::BinOp::MODULO) {
        tmp_val = this->builder->create_isrem(lhs, rhs);
        if (lhs_const.is_valid && rhs_const.is_valid) {
            const_expr.is_valid = true;
            const_expr.is_int = true;
            const_expr.int_value = lhs_const.int_value % rhs_const.int_value;
        } else {
            const_expr.is_valid = false;
        }
    }
}

void IRBuilder::visit(SyntaxTree::UnaryExpr &node) {
    node.rhs->accept(*this);
    if (node.op == SyntaxTree::UnaryOp::MINUS) {
        if (tmp_val->get_type()->is_float_type())
            tmp_val = this->builder->create_fsub(CONST_FLOAT(0.0), tmp_val);
        else 
            tmp_val = this->builder->create_isub(CONST_INT(0), tmp_val);
        if (const_expr.is_valid) {
            if (const_expr.is_int)
                const_expr.int_value = -const_expr.int_value;
            else 
                const_expr.float_value = -const_expr.float_value; 
        }
    }
}

void IRBuilder::visit(SyntaxTree::FuncCallStmt &node) {
    Function *ret = dynamic_cast<Function *>(this->scope.find(node.name, true));
    std::vector<Value *> params{}; // 实参集合
    auto arg_begin = ret->arg_begin();
    for (const auto &expr : node.params) { // 对每个实参进行求值，并放到 params 中
        expr->accept(*this);
        if ((*arg_begin)->get_type() == FLOAT_T) {
            if (tmp_val->get_type() == INT32_T)
                tmp_val = this->builder->create_sitofp(tmp_val, FLOAT_T);
            if (tmp_val->get_type() == INT1_T) {
                tmp_val = this->builder->create_zext(tmp_val, INT32_T);
                tmp_val = this->builder->create_sitofp(tmp_val, FLOAT_T);
            }
        }
        if ((*arg_begin)->get_type() == INT32_T) {
            if (tmp_val->get_type() == FLOAT_T)
                tmp_val = this->builder->create_fptosi(tmp_val, INT32_T);
            if (tmp_val->get_type() == INT1_T) {
                tmp_val = this->builder->create_zext(tmp_val, INT32_T);
            }
        }
        params.push_back(tmp_val);
        arg_begin++;
    }
    tmp_val = this->builder->create_call(ret, std::move(params));
}

void IRBuilder::visit(SyntaxTree::IfStmt &node) {
    auto trueBB = BasicBlock::create(this->builder->get_module(), "IfTrue" + std::to_string(label++), this->builder->get_module()->get_functions().back());
    auto nextBB = BasicBlock::create(this->builder->get_module(), "IfNext" + std::to_string(label++), this->builder->get_module()->get_functions().back());
    if(node.else_statement==nullptr){
        node.cond_exp->accept(*this);
        /*
        if(tmp_val->get_type()==INT32_T){
            tmp_val = builder->create_icmp_ne(tmp_val,CONST_INT(0));
        }
        else if(tmp_val->get_type()==FLOAT_T){
            tmp_val = builder->create_fcmp_ne(tmp_val,CONST_FLOAT(0));
        }
        */
        this->builder->create_cond_br(tmp_val, trueBB, nextBB);
        this->builder->set_insert_point(trueBB);
        node.if_statement->accept(*this);
        this->builder->create_br(nextBB);
        this->builder->set_insert_point(nextBB);
    }
    else{
        auto falseBB = BasicBlock::create(this->builder->get_module(), "IfFalse" + std::to_string(label++), this->builder->get_module()->get_functions().back());
        node.cond_exp->accept(*this);
        /*
        if(tmp_val->get_type()==INT32_T){
            tmp_val = builder->create_icmp_ne(tmp_val,CONST_INT(0));
        }
        else if(tmp_val->get_type()==FLOAT_T){
            tmp_val = builder->create_fcmp_ne(tmp_val,CONST_FLOAT(0));
        }
        */
        this->builder->create_cond_br(tmp_val, trueBB, falseBB);
        this->builder->set_insert_point(trueBB);
        node.if_statement->accept(*this);
        if (builder->get_insert_block()->get_terminator() == nullptr)
            this->builder->create_br(nextBB);
        else
            nextBB->erase_from_parent();

        this->builder->set_insert_point(falseBB);
        node.else_statement->accept(*this);
        if (builder->get_insert_block()->get_terminator() == nullptr)
            this->builder->create_br(nextBB);

        this->builder->set_insert_point(nextBB);
    }
}

void IRBuilder::visit(SyntaxTree::WhileStmt &node) {
    auto condBB=BasicBlock::create(this->builder->get_module(),"WhileCond" + std::to_string(label++),this->builder->get_module()->get_functions().back());
    auto trueBB=BasicBlock::create(this->builder->get_module(),"WhileTrue" + std::to_string(label++),this->builder->get_module()->get_functions().back());
    auto falseBB=BasicBlock::create(this->builder->get_module(),"WhileFalse" + std::to_string(label++),this->builder->get_module()->get_functions().back());
    this->builder->create_br(condBB);
    this->builder->set_insert_point(condBB);

    node.cond_exp->accept(*this);
    /*
    if (tmp_val->get_type() == INT32_T){
        tmp_val = builder->create_icmp_ne(tmp_val,CONST_INT(0));
    }
    else if(tmp_val->get_type()==FLOAT_T){
        tmp_val = builder->create_fcmp_ne(tmp_val,CONST_FLOAT(0));
    }
    */
    this->builder->create_cond_br(tmp_val, trueBB, falseBB);
    this->builder->set_insert_point(trueBB);
    tmp_condbb.push_back(condBB);
    tmp_falsebb.push_back(falseBB);

    node.statement->accept(*this);
    this->builder->create_br(condBB);
    this->builder->set_insert_point(falseBB);
    tmp_condbb.pop_back();
    tmp_falsebb.pop_back();
}

void IRBuilder::visit(SyntaxTree::BreakStmt &node) {
    this->builder->create_br(tmp_falsebb.back());
}

void IRBuilder::visit(SyntaxTree::ContinueStmt &node) {
    this->builder->create_br(tmp_condbb.back());
}

