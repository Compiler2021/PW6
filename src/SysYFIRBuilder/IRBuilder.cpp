#include "IRBuilder.h"
#include "BasicBlock.h"
#include "SyntaxTree.h"

#define CONST_INT(num) ConstantInt::get(num, module.get())
#define CONST_FLOAT(num) ConstantFloat::get(num, module.get())

// You can define global variables and functions here
// to store state

// store temporary value
Value *tmp_val = nullptr;

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

void IRBuilder::visit(SyntaxTree::InitVal &node) {}

void IRBuilder::visit(SyntaxTree::FuncDef &node) {
    std::vector<Type *> params;
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

void IRBuilder::visit(SyntaxTree::VarDef &node) {}

void IRBuilder::visit(SyntaxTree::LVal &node) {}

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

void IRBuilder::visit(SyntaxTree::BlockStmt &node) {}

void IRBuilder::visit(SyntaxTree::EmptyStmt &node) {}

void IRBuilder::visit(SyntaxTree::ExprStmt &node) {}

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
                "if_true",
                this->builder->get_insert_block()->get_parent());
        auto if_false = BasicBlock::create(
                this->module.get(),
                "if_false",
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
                "if_true",
                this->builder->get_insert_block()->get_parent());
        auto if_false = BasicBlock::create(
                this->module.get(),
                "if_false",
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

void IRBuilder::visit(SyntaxTree::FuncCallStmt &node) {}

void IRBuilder::visit(SyntaxTree::IfStmt &node) {}

void IRBuilder::visit(SyntaxTree::WhileStmt &node) {}

void IRBuilder::visit(SyntaxTree::BreakStmt &node) {}

void IRBuilder::visit(SyntaxTree::ContinueStmt &node) {}
