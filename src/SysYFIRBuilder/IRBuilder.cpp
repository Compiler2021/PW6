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
    node.target->accept(*this);
    auto dest = tmp_val; // 获取左值
    node.value->accept(*this);
    auto src = tmp_val; // 获取等号右边表达式的值
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

void IRBuilder::visit(SyntaxTree::UnaryCondExpr &node) {}

void IRBuilder::visit(SyntaxTree::BinaryCondExpr &node) {}

void IRBuilder::visit(SyntaxTree::BinaryExpr &node) {}

void IRBuilder::visit(SyntaxTree::UnaryExpr &node) {}

void IRBuilder::visit(SyntaxTree::FuncCallStmt &node) {}

void IRBuilder::visit(SyntaxTree::IfStmt &node) {}

void IRBuilder::visit(SyntaxTree::WhileStmt &node) {}

void IRBuilder::visit(SyntaxTree::BreakStmt &node) {}

void IRBuilder::visit(SyntaxTree::ContinueStmt &node) {}
