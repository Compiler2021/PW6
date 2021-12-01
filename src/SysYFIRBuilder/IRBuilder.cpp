#include "IRBuilder.h"
#include "BasicBlock.h"
#include "SyntaxTree.h"

#define CONST_INT(num) ConstantInt::get(num, module.get())
#define CONST_FLOAT(num) ConstantFloat::get(num, module.get())

// You can define global variables and functions here
int is_global = 0; // at first it is global
// to store state

// store temporary value
Value *tmp_val = nullptr;
int label = 0;
std::vector<BasicBlock*> tmp_condbb;
std::vector<BasicBlock*> tmp_falsebb;

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
        int num = node.elementList.size();
        for (auto item : node.elementList)
        {
            item->accept(*this);
            i++; // store the number 
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
    auto bb = BasicBlock::create(builder->get_module(), "entry", Func);
    builder->set_insert_point(bb);
    scope.enter();
    node.param_list->accept(*this);
    if(node.name=="main"){
        builder->create_call(Func, std::vector<Value *>{});
    }
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
    for(auto param:node.params){
        param->accept(*this);                                               //访问参数
        auto paramAlloc=builder->create_alloca(TypeMap[param->param_type]);     //分配空间
        builder->create_store(*Argument,paramAlloc);                            //存参数的值
        scope.push(param->name,paramAlloc);                              //加入符号表
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
    for (auto length : node.array_length)
    {
        length->accept(*this);
        count++;
    }
    if (node.is_inited) 
    {
        node.initializers->accept(*this);
        auto initializer = tmp_val; // is inited
        if (scope.in_global())
        {
            if (count == 0) // this is not an array
            {
                auto global_init = GlobalVariable::create(node.name, module.get(), Vartype, false, dynamic_cast<Constant*>(tmp_val));
                scope.push(node.name, global_init);
            }
            else
            {
                
                auto *arrayType_global = ArrayType::get(Vartype, count);
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
                builder->create_load(tmp_val);
            }
            else
            {
                auto *arrayType_local = ArrayType::get(Vartype, count);
                auto arrayLocal = builder->create_alloca(arrayType_local);
                scope.push(node.name, arrayLocal);
            }
        }
    }
    else
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
                
                auto *arrayType_global = ArrayType::get(Vartype, count);
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
                auto *arrayType_local = ArrayType::get(Vartype, count);
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
        ret = this->builder->create_gep(ret, {tmp_val}); // 获取数组元素
    }
    tmp_val = ret;
}

void IRBuilder::visit(SyntaxTree::AssignStmt &node) {
    node.value->accept(*this);
    auto src = tmp_val; // 获取等号右边表达式的值
    node.target->accept(*this);
    auto dest = tmp_val; // 获取左值
    this->builder->create_store(src, dest); // 存储值
    tmp_val = src; // 整个表达式的值就是等号右边的表达式的值
}

void IRBuilder::visit(SyntaxTree::Literal &node) {
    if (node.literal_type == SyntaxTree::Type::INT) // 字面量是个整形
        tmp_val = CONST_INT(node.int_const);
    else // 字面量是个浮点数
        tmp_val = CONST_FLOAT(node.float_const);
}

void IRBuilder::visit(SyntaxTree::ReturnStmt &node) {
    node.ret->accept(*this);
    auto ret_val = tmp_val;
    this->builder->create_ret(ret_val);
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
}

void IRBuilder::visit(SyntaxTree::BinaryCondExpr &node) {
    if (node.op == SyntaxTree::BinaryCondOp::LOR) {
        node.lhs->accept(*this);

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
        return;
    }
    if (node.op == SyntaxTree::BinaryCondOp::LAND) {
        node.lhs->accept(*this);

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
        return;
    }
    
    node.lhs->accept(*this);
    auto lhs = tmp_val;
    node.lhs->accept(*this);
    auto rhs = tmp_val;
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
    }
}

void IRBuilder::visit(SyntaxTree::BinaryExpr &node) {
    node.lhs->accept(*this);
    auto lhs = tmp_val;
    node.rhs->accept(*this);
    auto rhs = tmp_val;
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
    } else if (node.op == SyntaxTree::BinOp::MULTIPLY) {
        if (lhs->get_type()->is_float_type()) {
            if (rhs->get_type()->is_integer_type())
                rhs = this->builder->create_sitofp(rhs, FLOAT_T);
            tmp_val = this->builder->create_fmul(lhs, rhs);
        } else if (rhs->get_type()->is_float_type()) {
            lhs = this->builder->create_sitofp(lhs, FLOAT_T);
            tmp_val = this->builder->create_fmul(lhs, rhs);
        } else {
            tmp_val = this->builder->create_imul(lhs, rhs);
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
    } else if (node.op == SyntaxTree::BinOp::MODULO) {
        tmp_val = this->builder->create_isrem(lhs, rhs);
    }
}

void IRBuilder::visit(SyntaxTree::UnaryExpr &node) {
    node.rhs->accept(*this);
    if (node.op == SyntaxTree::UnaryOp::MINUS) {
        if (tmp_val->get_type()->is_float_type())
            tmp_val = this->builder->create_fsub(CONST_FLOAT(0.0), tmp_val);
        else 
            tmp_val = this->builder->create_isub(CONST_INT(0), tmp_val);
    }
}

void IRBuilder::visit(SyntaxTree::FuncCallStmt &node) {
    auto ret = this->scope.find(node.name, true);
    std::vector<Value *> params{}; // 实参集合
    for (const auto &expr : node.params) { // 对每个实参进行求值，并放到 params 中
        expr->accept(*this);
        params.push_back(tmp_val);
    }
    tmp_val = this->builder->create_call(ret, std::move(params));
}

void IRBuilder::visit(SyntaxTree::IfStmt &node) {
    auto trueBB = BasicBlock::create(this->builder->get_module(), "IfTrue" + std::to_string(label++), this->builder->get_module()->get_functions().back());
    auto nextBB = BasicBlock::create(this->builder->get_module(), "IfNext" + std::to_string(label++), this->builder->get_module()->get_functions().back());
    if(node.else_statement==nullptr){
        node.cond_exp->accept(*this);
        if(tmp_val->get_type()==INT32_T){
            tmp_val = builder->create_icmp_ne(tmp_val,CONST_INT(0));
        }
        else if(tmp_val->get_type()==FLOAT_T){
            tmp_val = builder->create_fcmp_ne(tmp_val,CONST_FLOAT(0));
        }
        this->builder->create_cond_br(tmp_val, trueBB, nextBB);
        this->builder->set_insert_point(trueBB);
        node.if_statement->accept(*this);
        this->builder->create_br(nextBB);
        this->builder->set_insert_point(nextBB);
    }
    else{
        auto falseBB = BasicBlock::create(this->builder->get_module(), "IfFalse" + std::to_string(label++), this->builder->get_module()->get_functions().back());
        node.cond_exp->accept(*this);
        if(tmp_val->get_type()==INT32_T){
            tmp_val = builder->create_icmp_ne(tmp_val,CONST_INT(0));
        }
        else if(tmp_val->get_type()==FLOAT_T){
            tmp_val = builder->create_fcmp_ne(tmp_val,CONST_FLOAT(0));
        }
        this->builder->create_cond_br(tmp_val, trueBB, falseBB);
        this->builder->set_insert_point(trueBB);
        node.if_statement->accept(*this);
        this->builder->create_br(nextBB);
        this->builder->set_insert_point(falseBB);

        node.else_statement->accept(*this);
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
    if (tmp_val->get_type() == INT32_T){
        tmp_val = builder->create_icmp_ne(tmp_val,CONST_INT(0));
    }
    else if(tmp_val->get_type()==FLOAT_T){
        tmp_val = builder->create_fcmp_ne(tmp_val,CONST_FLOAT(0));
    }
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

